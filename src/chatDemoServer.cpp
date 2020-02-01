#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <random>
#include <unordered_map>
#include <atomic>


#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <grpc/support/log.h>
#include "chat_demo.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using chatDemo::Connect;
using chatDemo::ChatService;

class ServerImpl;
// Globals to help with this example
static ServerImpl* gServerImpl;

// Globals to analyze the results and make sure we are not leaking any rpcs
static std::atomic_int32_t gBidirectionalStreamingRpcCounter(0);

// We add a 'TagProcessor' to the completion queue for each event. This way, each tag knows how to process itself. 
using TagProcessor = std::function<void(bool)>;
struct TagInfo
{
    TagProcessor* tagProcessor; // The function to be called to process incoming event
    bool ok; // The result of tag processing as indicated by gRPC library. Calling it 'ok' to be in sync with other gRPC examples.
};

using TagList = std::list<TagInfo>;

// As the tags become available from completion queue thread, we put them in a queue in order to process them on our application thread. 
static TagList gIncomingTags;
std::mutex gIncomingTagsMutex;

// Random sleep code in order to introduce some randomness in this example. This allows for quick stress testing. 
static void randomSleepThisThread(int lowerBoundMS, int upperBoundMS)
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<> dist{ lowerBoundMS, upperBoundMS };
    std::this_thread::sleep_for(std::chrono::milliseconds{ dist(generator) });
}

static void randomSleepThisThread()
{
    randomSleepThisThread(10, 100);
}


// A base class for various rpc types. With gRPC, it is necessary to keep track of pending async operations.
// Only 1 async operation can be pending at a time with an exception that both async read and write can be pending at the same time.
class RpcJob
{
public:
    enum AsyncOpType
    {
        ASYNC_OP_TYPE_INVALID,
        ASYNC_OP_TYPE_QUEUED_REQUEST,
        ASYNC_OP_TYPE_READ,
        ASYNC_OP_TYPE_WRITE,
        ASYNC_OP_TYPE_PUSH_TO_BACK,
        ASYNC_OP_TYPE_FINISH
    };

    RpcJob()
        : mAsyncOpCounter(0)
        , mAsyncReadInProgress(false)
        , mAsyncWriteInProgress(false)
        , mOnDoneCalled(false)
    {

    }

    virtual ~RpcJob() {};

    void AsyncOpStarted(AsyncOpType opType)
    {
        ++mAsyncOpCounter;

        switch (opType)
        {
        case ASYNC_OP_TYPE_READ:
            mAsyncReadInProgress = true;
            break;
        case ASYNC_OP_TYPE_WRITE:
            mAsyncWriteInProgress = true;
        default: //Don't care about other ops
            break;
        }
    }

    // returns true if the rpc processing should keep going. false otherwise.
    bool AsyncOpFinished(AsyncOpType opType)
    {
        --mAsyncOpCounter;

        switch (opType)
        {
        case ASYNC_OP_TYPE_READ:
            mAsyncReadInProgress = false;
            break;
        case ASYNC_OP_TYPE_WRITE:
            mAsyncWriteInProgress = false;
        default: //Don't care about other ops
            break;
        }

        // No async operations are pending and gRPC library notified as earlier that it is done with the rpc.
        // Finish the rpc. 
        if (mAsyncOpCounter == 0 && mOnDoneCalled)
        {
            Done();
            return false;
        }

        return true;
    }

    bool AsyncOpInProgress() const
    {
        return mAsyncOpCounter != 0;
    }

    bool AsyncReadInProgress() const
    {
        return mAsyncReadInProgress;
    }

    bool AsyncWriteInProgress() const
    {
        return mAsyncWriteInProgress;
    }

    // Tag processor for the 'done' event of this rpc from gRPC library
    void OnDone(bool /*ok*/)
    {
        mOnDoneCalled = true;
        if (mAsyncOpCounter == 0)
            Done();
    }

    // Each different rpc type need to implement the specialization of action when this rpc is done.
    virtual void Done() = 0;
private:
    int32_t mAsyncOpCounter;
    bool mAsyncReadInProgress;
    bool mAsyncWriteInProgress;

    // In case of an abrupt rpc ending (for example, client process exit), gRPC calls OnDone prematurely even while an async operation is in progress
    // and would be notified later. An example sequence would be
    // 1. The client issues an rpc request. 
    // 2. The server handles the rpc and calls Finish with response. At this point, ServerContext::IsCancelled is NOT true.
    // 3. The client process abruptly exits. 
    // 4. The completion queue dispatches an OnDone tag followed by the OnFinish tag. If the application cleans up the state in OnDone, OnFinish invocation would result in undefined behavior. 
    // This actually feels like a pretty odd behavior of the gRPC library (it is most likely a result of our multi-threaded usage) so we account for that by keeping track of whether the OnDone was called earlier. 
    // As far as the application is considered, the rpc is only 'done' when no asyn Ops are pending. 
    bool mOnDoneCalled;
};


// The application code communicates with our utility classes using these handlers. 
template<typename ServiceType, typename RequestType, typename ResponseType>
struct RpcJobHandlers
{
public:
    // typedefs. See the comments below. 
    using ProcessRequestHandler = std::function<void(ServiceType*, RpcJob*, const RequestType*)>;
    using CreateRpcJobHandler = std::function<void()>;
    using RpcJobDoneHandler = std::function<void(ServiceType*, RpcJob*, bool)>;

    using SendResponseHandler = std::function<bool(const ResponseType*)>; // GRPC_TODO - change to a unique_ptr instead to avoid internal copying.
    using RpcJobContextHandler = std::function<void(ServiceType*, RpcJob*, grpc::ServerContext*, SendResponseHandler)>;

    // Job to Application code handlers/callbacks
    ProcessRequestHandler processRequestHandler; // RpcJob calls this to inform the application of a new request to be processed. 
    CreateRpcJobHandler createRpcJobHandler; // RpcJob calls this to inform the application to create a new RpcJob of this type.
    RpcJobDoneHandler rpcJobDoneHandler; // RpcJob calls this to inform the application that this job is done now. 

    // Application code to job
    RpcJobContextHandler rpcJobContextHandler; // RpcJob calls this to inform the application of the entities it can use to respond to the rpc request.
};


template<typename ServiceType, typename RequestType, typename ResponseType>
struct BidirectionalStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncReaderWriter<ResponseType, RequestType>;
    using QueueRequestHandler = std::function<void(ServiceType*, grpc::ServerContext*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void *)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler; // RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};



template<typename ServiceType, typename RequestType, typename ResponseType>
class BidirectionalStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = BidirectionalStreamingRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    BidirectionalStreamingRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers,int client_num)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mServerStreamingDone(false)
        , mClientStreamingDone(false)
    {
        ++gBidirectionalStreamingRpcCounter;

        // create TagProcessors that we'll use to interact with gRPC CompletionQueue
        mOnInit = std::bind(&BidirectionalStreamingRpcJob::OnInit, this, std::placeholders::_1);
        mOnRead = std::bind(&BidirectionalStreamingRpcJob::OnRead, this, std::placeholders::_1);
        mOnWrite = std::bind(&BidirectionalStreamingRpcJob::OnWrite, this, std::placeholders::_1);
        mOnFinish = std::bind(&BidirectionalStreamingRpcJob::OnFinish, this, std::placeholders::_1);
        mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

        // set up the completion queue to inform us when gRPC is done with this rpc.
        mServerContext.AsyncNotifyWhenDone(&mOnDone);

        //inform the application of the entities it can use to respond to the rpc
        mSendResponse = std::bind(&BidirectionalStreamingRpcJob::SendResponse, this, std::placeholders::_1);
        jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

        // finally, issue the async request needed by gRPC to start handling this rpc.
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
        mHandlers.queueRequestHandler(mService, &mServerContext, &mResponder, mCQ, mCQ, &mOnInit);
    }

private:

    bool SendResponse(const ResponseType* response)
    {
        if (response == nullptr && !mClientStreamingDone)
        {
            // Wait for client to finish the all the requests. If you want to cancel, use ServerContext::TryCancel. 
            GPR_ASSERT(false);
            return false;
        }

        if (response != nullptr)
        {
            mResponseQueue.push_back(*response); // We need to make a copy of the response because we need to maintain it until we get a completion notification. 

            if (!AsyncWriteInProgress())
            {
                doSendResponse();
            }
          
        }
        else
        {
            mServerStreamingDone = true;

            if (!AsyncWriteInProgress()) // Kick the async op if our state machine is not going to be kicked from the completion queue
            {
                doFinish();
            }
        }

        return true;
    }

    void OnInit(bool ok)
    {
        mHandlers.createRpcJobHandler();

        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if (ok)
            {
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
        }
    }
    void OnRead(bool ok)
    {
        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_READ))
        {
            if (ok)
            {
               
                // inform application that a new request has come in
                mHandlers.processRequestHandler(mService, this, &mRequest);

                // queue up another read operation for this rpc
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
                
            }
            else
            {
                mClientStreamingDone = true;
                mHandlers.processRequestHandler(mService, this, nullptr);
            }
        }
    }

    void OnWrite(bool ok)
    {
        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_WRITE))
        {
            // Get rid of the message that just finished. 
            mResponseQueue.pop_front();
            if (ok)
            {
                if (!mResponseQueue.empty()) // If we have more messages waiting to be sent, send them.
                {
                    doSendResponse();
                }
                else if (mServerStreamingDone) // Previous write completed and we did not have any pending write. If the application indicated a done operation, finish the rpc processing.
                {
                    doFinish();
                }
            }
        }
    }

    void OnFinish(bool ok)
    {
        AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_FINISH);
    }

    void Done() override
    {
        mHandlers.rpcJobDoneHandler(mService, this, mServerContext.IsCancelled());

        int a = --gBidirectionalStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Bidirectional Streaming Rpcs Count = %d", a);
    }

    void doSendResponse()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_WRITE);
        mResponder.Write(mResponseQueue.front(), &mOnWrite);
    }

    void doFinish()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(grpc::Status::OK, &mOnFinish);
    }

private:
    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnInit;
    TagProcessor mOnRead;
    TagProcessor mOnWrite;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;

    grpc::Alarm alarm_;

    std::list<ResponseType> mResponseQueue;
    bool mServerStreamingDone;
    bool mClientStreamingDone;

};

class ServerImpl final{
public:
    ServerImpl()
    {

    }
    ~ServerImpl()
    {
        mServer->Shutdown();
        //Always shutdown the completion queue after the server.
        mCQ->Shutdown();
    }

    // There is no shutdown handling in this code.
    void Run() 
    {
        std::string server_address("0.0.0.0:50051");

        grpc::ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&mRouteGuideService);
        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        mCQ = builder.AddCompletionQueue();
        // Finally assemble the server.
        mServer = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        // Proceed to the server's main loop.
        HandleRpcs();
    }

private:    
    void createRouteChatRpc()
    {
        BidirectionalStreamingRpcJobHandlers<chatDemo::ChatService::AsyncService, chatDemo::Connect, chatDemo::Connect> jobHandlers;
        jobHandlers.rpcJobContextHandler = &RouteChatContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &RouteChatDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createRouteChatRpc, this);
        jobHandlers.queueRequestHandler = &chatDemo::ChatService::AsyncService::RequestClientSide;
        jobHandlers.processRequestHandler = &RouteChatProcessor;

        new BidirectionalStreamingRpcJob<chatDemo::ChatService::AsyncService, chatDemo::Connect, chatDemo::Connect>(&mRouteGuideService, mCQ.get(), jobHandlers,Client_num);
    }

    struct RouteChatResponder
    {
        std::function<bool(chatDemo::Connect*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*, RouteChatResponder> mRouteChatResponders;
    static void RouteChatContextSetterImpl(chatDemo::ChatService::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(chatDemo::Connect*)> sendResponse)
    {
        RouteChatResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mRouteChatResponders[job] = responder;
    }
    
    static void RouteChatProcessor(chatDemo::ChatService::AsyncService* service, RpcJob* job, const chatDemo::Connect* note)
    {
        //Simply echo the note back.
        if (note)
        {
            
            chatDemo::Connect responseNote(*note);
            responseNote.set_text(" --->  " + responseNote.text());
            gServerImpl->mRouteChatResponders[job].sendFunc(&responseNote);
            randomSleepThisThread();
        }
        else
        {
            gServerImpl->mRouteChatResponders[job].sendFunc(nullptr);
            randomSleepThisThread();
        }
    } 
   
    static void RouteChatDone(chatDemo::ChatService::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mRouteChatResponders.erase(job);
        delete job;
    }

    void HandleRpcs() 
    {
        createRouteChatRpc();

        TagInfo tagInfo;
        while (true) 
        {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(mCQ->Next((void**)&tagInfo.tagProcessor, &tagInfo.ok)); //GRPC_TODO - Handle returned value
            
            gIncomingTagsMutex.lock();
            gIncomingTags.push_back(tagInfo);
            gIncomingTagsMutex.unlock();

        }
    }

private:
    int Client_num = 0;
    std::unique_ptr<grpc::ServerCompletionQueue> mCQ;
    chatDemo::ChatService::AsyncService mRouteGuideService;
    std::unique_ptr<grpc::Server> mServer;

};

static void processRpcs()
{
    // Implement a busy-wait loop. Not the most efficient thing in the world but but would do for this example
    while (true)
    {
        gIncomingTagsMutex.lock();
        TagList tags = std::move(gIncomingTags);
        gIncomingTagsMutex.unlock();

        while (!tags.empty())
        {
            TagInfo tagInfo = tags.front();
            tags.pop_front();
            (*(tagInfo.tagProcessor))(tagInfo.ok);

            randomSleepThisThread(); //Simulate processing Time
        };
        randomSleepThisThread(); //yield cpu
    }
}

int main(int argc, char** argv)
{
    std::thread processorThread(processRpcs);
    gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);

    ServerImpl server;
    gServerImpl = &server;
    server.Run();

    return EXIT_SUCCESS;
}