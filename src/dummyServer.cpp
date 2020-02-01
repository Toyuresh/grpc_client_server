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
#include <grpc/support/log.h>
#include <grpcpp/alarm.h>

#include "dummy.grpc.pb.h"

class ServerImpl;
//Globals to help with this example
static ServerImpl* gServerImpl;

static std::atomic_int32_t gUnaryRpcCounter(0);
static std::atomic_int32_t gServerStreamingRpcCounter(0);
static std::atomic_int32_t gCLientStreamingRpcCounter(0);
static std::atomic_int32_t gBidirectionalStreamingRpcCounter(0);
int temp_values;

//We add a 'TagProcessor' to the completion queue for each event. This way, each tag knows how to process itself.
using TagProcessor = std::function<void(bool)>;
struct TagInfo
{
    TagProcessor* tagProcessor;      // The function to be called to process incoming event.
    bool ok;        // The result of tag processing as indicated by gRPC library. Calling it 'ok' to be in sync with other gRPC example 
};

using TagList = std::list<TagInfo>;

//As the tags become available from completion queue thread, we put them in a queue in order to process them on our application thread.
static TagList gIncomingTags;
std::mutex gIncomingTagsMutex;

//Random sleep code in order to introduce some randomness in this example. This allows for quick stress testing.
static void randomSleepThisThread(int lowerBoundMS, int upperBoundMS)
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<> dist{ lowerBoundMS, upperBoundMS };
    std::this_thread::sleep_for(std::chrono::milliseconds { dist(generator)});
}

static void randomSleepThisThread()
{
    randomSleepThisThread(10,100);
}

// A base class for various rpc types. with gRPC, it is necessary to keep track of pending async operations.
//Only 1 async operation can be pending at a time with an exception that both async read and write can be pending at the same time.
class RpcJob
{
public:
    enum AsyncOpType
    {
        ASYNC_OP_TYPE_INVALID,
        ASYNC_OP_TYPE_QUEUED_REQUEST,
        ASYNC_OP_TYPE_READ,
        ASYNC_OP_TYPE_WRITE,
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

        switch(opType)
        {
        case ASYNC_OP_TYPE_READ:
            mAsyncReadInProgress = true;
            break;
        case ASYNC_OP_TYPE_WRITE:
            mAsyncWriteInProgress = true;

        default:    //Don't care about other ops
            break;
        }
    }        
    // returns true if the rpc processing should keep going. false otherwise.
    bool AsyncOpFinished(AsyncOpType opType)
    {
        --mAsyncOpCounter;

        switch(opType)
        {
            case ASYNC_OP_TYPE_READ:
                mAsyncReadInProgress = false;
                break;
            case ASYNC_OP_TYPE_WRITE:
                mAsyncWriteInProgress = false;
            default:    //Don't care about other ops
                break; 
        }
        
        // No async operations are pending and gRPC library notified as earlier that it is done with the rpc.
        // Finish the rpc.
        if(mAsyncOpCounter == 0 && mOnDoneCalled)
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
        if(mAsyncOpCounter == 0)
        {
            Done();
        }
    }

    //Each different rpc type need to implement the specialization of action when this rpc is done.
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

//The application code communicates with our utility classes using these handlers.
template<typename ServiceType, typename RequestType, typename ResponseType>
struct RpcJobHandlers
{
public:
    //typedefs. see the comments below.
    using ProcessRequestHandler = std::function<void(ServiceType*, RpcJob*, const RequestType*)>;
    using CreateRpcJobHandler = std::function<void()>;
    using RpcJobDoneHandler = std::function<void(ServiceType* , RpcJob*, bool)>;

    using SendResponseHandler = std::function<bool(const ResponseType*)>;
    using RpcJobContextHandler = std::function<void(ServiceType*, RpcJob*,grpc::ServerContext* ,SendResponseHandler)>;

   // Job to Application code handlers/callbacks
    ProcessRequestHandler processRequestHandler; // RpcJob calls this to inform the application of a new request to be processed. 
    CreateRpcJobHandler createRpcJobHandler; // RpcJob calls this to inform the application to create a new RpcJob of this type.
    RpcJobDoneHandler rpcJobDoneHandler; // RpcJob calls this to inform the application that this job is done now. 

    // Application code to job
    RpcJobContextHandler rpcJobContextHandler; // RpcJob calls this to inform the application of the entities it can use to respond to the rpc reques
};

//Each rpc type specializes RpcJobHandlers by deriving from it as each of them have a different responder to talk back to gRPC library.
template<typename ServiceType, typename RequestType, typename ResponseType>
struct UnaryRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncResponseWriter<ResponseType>;
    using QueueRequestHandler = std::function<void(ServiceType*,grpc::ServerContext*,RequestType*,GRPCResponder*,grpc::CompletionQueue* , grpc::ServerCompletionQueue*,void *)>;

    //Job to Appllication code handlers/callbacks
    QueueRequestHandler queueRequestHandler; //RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};


template<typename ServiceType, typename RequestType, typename ResponseType>
struct ServerStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncWriter<ResponseType>;
    using QueueRequestHandler = std::function<void(ServiceType* , grpc::ServerContext*,RequestType*,GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*,void*)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler;    //RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct ClientStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncReader<ResponseType, RequestType>;
    using QueueRequestHandler = std::function<void(ServiceType* , grpc::ServerContext*, GRPCResponder* , grpc::CompletionQueue* , grpc::ServerCompletionQueue*, void*)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler; // RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct BidirectionalStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncReaderWriter<ResponseType, RequestType>;
    using QueueRequestHandler = std::function<void(ServiceType*, grpc::ServerContext*, GRPCResponder*, grpc::CompletionQueue* , grpc::ServerCompletionQueue* ,void *)>;

    // Job to application code handlers/callbacks
    QueueRequestHandler queueRequestHandler;    //RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};

/*
We implement UnaryRpcJob, ServerStreamingRpcJob, ClientStreamingRpcJob and BidirectionalStreamingRpcJob. The application deals with these classes.
As a convention, we always send grpc::Status::OK and add any error info in the google.rpc.Status member of the ResponseType field. In streaming scenarios, this allows us to indicate error
in a request to a client without completion of the rpc (and allow for more requests on same rpc). We do, however, allow server side cancellation of the rpc.
*/

template<typename ServiceType, typename RequestType, typename ResponseType>
class UnaryRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = UnaryRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    UnaryRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
    {
        ++gUnaryRpcCounter;

        // create TagProcessors that we'll use to interact with gRPC CompletionQueue
        mOnRead = std::bind(&UnaryRpcJob::OnRead, this, std::placeholders::_1);
        mOnFinish = std::bind(&UnaryRpcJob::OnFinish, this, std::placeholders::_1);
        mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

        // set up the completion queue to inform us when gRPC is done with this rpc.
        mServerContext.AsyncNotifyWhenDone(&mOnDone);

        // inform the application of the entities it can use to respond to the rpc
        mSendResponse = std::bind(&UnaryRpcJob::SendResponse, this, std::placeholders::_1);
        jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

        // finally, issue the async request needed by gRPC to start handling this rpc.
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
        mHandlers.queueRequestHandler(mService, &mServerContext, &mRequest, &mResponder, mCQ, mCQ, &mOnRead);
    }

private:
    bool SendResponse(const ResponseType* response)
    {
        //We always expect a valid response for Unary rpc. If no response is available, use ServerContext::TryCancel.
        GPR_ASSERT(response);
        if(response == nullptr)
            return false;

        mResponse = *response;

        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(mResponse, grpc::Status::OK,&mOnFinish);

        return true;
    }

    void OnRead(bool ok)
    {
        //A request has come on the service which can now be handled. Create a new rpc of this type to allow the server to handle next request.
        mHandlers.createRpcJobHandler();

        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if(ok)
            {
                //We have a request that can be responded to now. So process it.
                mHandlers.processRequestHandler(mService,this,&mRequest);
            }
            else
            {
                GPR_ASSERT(ok);
            }
        }
    }

    void OnFinish(bool ok)
    {
        AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_FINISH);
    }

    void Done() override
    {
        mHandlers.rpcJobDoneHandler(mService,this,mServerContext.IsCancelled());

        temp_values = --gUnaryRpcCounter;
        gpr_log(GPR_DEBUG,"Pending Unary Rpcs Count = %d", temp_values);
    }

private:

    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;
    ResponseType mResponse;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnRead;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;
};

template<typename ServiceType, typename RequestType, typename ResponseType>
class ServerStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = ServerStreamingRpcJobHandlers<ServiceType, RequestType,ResponseType>;

public:
    ServerStreamingRpcJob(ServiceType* service,grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mServerStreamingDone(false)
        {

        ++gServerStreamingRpcCounter;

            // create TagProcessors that we'll use to interact with gRPC CompletionQueue
            mOnRead = std::bind(&ServerStreamingRpcJob::OnRead, this, std::placeholders::_1);
            mOnWrite = std::bind(&ServerStreamingRpcJob::OnWrite, this, std::placeholders::_1);
            mOnFinish = std::bind(&ServerStreamingRpcJob::OnFinish, this, std::placeholders::_1);
            mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

            // set up the completion queue to inform us when gRPC is done with this rpc.
            mServerContext.AsyncNotifyWhenDone(&mOnDone);

            //inform the application of the entities it can use to respond to the rpc
            mSendResponse = std::bind(&ServerStreamingRpcJob::SendResponse, this, std::placeholders::_1);
            jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

            // finally, issue the async request needed by gRPC to start handling this rpc.
            AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
            mHandlers.queueRequestHandler(mService, &mServerContext, &mRequest, &mResponder, mCQ, mCQ, &mOnRead);
        }

private:
    // gRPC can only do one async write at a time but that is very inconvenient form the application pont of view
    // So we buffer the response below in a queue if gRPC lib is not ready for it.
    // The application can send a null response in order to indicate the completion of server side streaming.
    bool SendResponse(const ResponseType* response)
    {
        if(response != nullptr)
        {
            mResponseQueue.push_back(*response);

            if(!AsyncWriteInProgress())
            {
                doSendResponse();
            }
        }
        else
        {
            mServerStreamingDone = true;
            if(!AsyncWriteInProgress())
            {
                doFinish();
            }
        }
        
        return true;
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

    void OnRead(bool ok)
    {
        mHandlers.createRpcJobHandler();
        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if(ok)
            {
                mHandlers.processRequestHandler(mService, this, &mRequest);
            }
        }
    }

    void OnWrite(bool ok)
    {
        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_WRITE))
        {
            //Get rid of the message that just finished.
            mResponseQueue.pop_front();

            if(ok)
            {
                if(!mResponseQueue.empty())     // If we have more messages waiting to be sent, send them.
                {
                    doSendResponse();
                }
                else if(mServerStreamingDone)   // Previous write completed and we did not have any pending write. If the application has finished streaming responses, finish the rpc processing.
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

        temp_values = --gServerStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Server Streaming Rpcs count = %d", temp_values);
    }

private:
    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnRead;
    TagProcessor mOnWrite;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;

    std::list<ResponseType> mResponseQueue;
    bool mServerStreamingDone;

};

template<typename ServiceType, typename RequestType, typename ResponseType>
class ClientStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = ClientStreamingRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    ClientStreamingRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mClientStreamingDone(false)
        {
            ++gCLientStreamingRpcCounter;

            //create TagProcessors that we'll use to interact with gRPC completionQueue
            mOnInit = std::bind(&ClientStreamingRpcJob::OnInit, this, std::placeholders::_1);
            mOnRead = std::bind(&ClientStreamingRpcJob::OnRead, this, std::placeholders::_1);
            mOnFinish = std::bind(&ClientStreamingRpcJob::OnFinish, this, std::placeholders::_1);
            mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

            //set up the completion queue to inform us when gRPC is done with this rpc.
            mServerContext.AsyncNotifyWhenDone(&mOnDone);

            //inform the applicaton of the entities it can use to respond to the rpc
            mSendResponse = std::bind(&ClientStreamingRpcJob::SendResponse, this, std::placeholders::_1);
            jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

            //finally issuse the async request needed by gRPC to start handling this rpc.
            AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
            mHandlers.queueRequestHandler(mService, &mServerContext, &mResponder, mCQ, mCQ, &mOnInit);

        }

private:
    bool SendResponse(const ResponseType* response)
    {
        //We always expect a valid response for client stremaing rpc. If no response is available, use serverContext::TryCancel
        GPR_ASSERT(response);
        if(response == nullptr)
        {
            return false;
        }

        if(!mClientStreamingDone)
        {
            //It done not sense to send a response before client has streamed all the requests. 
            //Supporting this behavior could lead to writing error-prone
            //code so it is specificallly disallowed. If you need to error out before client can stream all the requests, use ServerContext::TryCancel.
            GPR_ASSERT(false);
            return false;
        }

        mResponse = *response;

        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(mResponse, grpc::Status::OK,&mOnFinish);

        return true;
    }

    void OnInit(bool ok)
    {
        mHandlers.createRpcJobHandler();

        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if(ok)
            {
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
        }
    }

    void OnRead(bool ok)
    {
        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_READ))
        {
            if(ok)
            {
                //inform application that a new request has come in
                mHandlers.processRequestHandler(mService,this, &mRequest);

                //queue up another read operation for this rpc
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest,&mOnRead);
            }
            else
            {
                mClientStreamingDone = true;
                mHandlers.processRequestHandler(mService, this, nullptr);
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

        temp_values  = --gCLientStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending client streaming Rpcs count = %d",temp_values);
    }

private:
    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;
    ResponseType mResponse;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnInit;
    TagProcessor mOnRead;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;

    bool mClientStreamingDone;
};

template<typename ServiceType, typename RequestType, typename ResponseType>
class BidirectionalStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = BidirectionalStreamingRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    BidirectionalStreamingRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mServerStreamingDone(false)
        , mClientStreamingDone(false)
        {
            ++gBidirectionalStreamingRpcCounter;

            //create TagProcessors that we'll use to interact with gRPC CompletionQueue
            mOnInit = std::bind(&BidirectionalStreamingRpcJob::OnInit, this, std::placeholders::_1);


        }
private:
    bool SendResponse(const ResponseType* response)
    {
        if(response == nullptr && !mClientStreamingDone)
        {
            //Wait for client to finish the all the requests. If you want to cancel,use ServerContext::TryCancel.
            GPR_ASSERT(false);
            return false;
        }
        if(response != nullptr)
        {
            mResponseQueue.push_back(*response);

            if(!AsyncWriteInProgress())
            {
                doSendResponse();
            }
        }
        else
        {
            mServerStreamingDone = true;
            if(!AsyncWriteInProgress())
            {
                doFinish();
            }
        }

        return true;        
    }

    void OnInit(bool ok)
    {
        mHandlers.createRpcJobHandler();

        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if(ok)
            {
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
        }
    }
    void OnWrite(bool ok)
    {
        if(AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_WRITE))
        {
            //Get rid of the message that just finished.
            mResponseQueue.pop_front();

            if(ok)
            {
                if(!mResponseQueue.empty())
                {
                    doSendResponse();
                }
                else if(mServerStreamingDone)
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

        temp_values = --gBidirectionalStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Bidirectional streaming Rpcs count = %d", temp_values);
    }

    void doSendResponse()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_WRITE);
        mResponder.Finish(grpc::Status::OK,&mOnFinish);
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

    std::list<ResponseType> mResponseQueue;
    bool mServerStreamingDone;
    bool mClientStreamingDone; 
};

class ServerImpl final
{
public:
    ServerImpl()
    {   }

    ~ServerImpl()
    {
        mServer->Shutdown();

        //Always shutdown the completion queue after the server.
        mCQ->Shutdown();
    }
    //There is no shutdown handling in this code.
    void Run()
    {
        std::string server_address("0.0.0.0:50051");

        grpc::ServerBuilder builder;

        //Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        //Register "service_" as instance through which we'll communicate with
        //clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&mDummyService);
        //Get hold of the completion queue used for the asynchronous communication
        //with the gRPC runtime.
        mCQ = builder.AddCompletionQueue();
        //Finally assemble the server.
        mServer = builder.BuildAndStart();
        std::cout << "Server listeninng on " << server_address << std::endl;
        
        //Proceed to the server's main loop.
        HandleRpcs();
    }
private:
    //Handlers for various rpcs. An application could do custom code generation for creating these(except the actual processing logic )
    void createGetHelloReplyRpc()
    {
        UnaryRpcJobHandlers<dummy::DummyService::AsyncService,dummy::HelloRequest, dummy::HelloReply> jobHandlers;
        jobHandlers.rpcJobContextHandler = &GetHelloReplyContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &GetHelloReplyDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createGetHelloReplyRpc,this);
        jobHandlers.queueRequestHandler = &dummy::DummyService::AsyncService::RequestUnary;
        jobHandlers.processRequestHandler = &GetHelloReplyProcessor;
        

        new UnaryRpcJob<dummy::DummyService::AsyncService, dummy::HelloRequest, dummy::HelloReply>(&mDummyService,mCQ.get(),jobHandlers);
    }

    struct GetHelloReplyResponder
    {
        std::function<bool(dummy::HelloReply*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*,GetHelloReplyResponder> mGetHelloReplyResponder;
    static void GetHelloReplyContextSetterImpl(dummy::DummyService::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(dummy::HelloReply*)> sendResponse)
    {
        GetHelloReplyResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mGetHelloReplyResponder[job] = responder;    

    }
    static void GetHelloReplyProcessor(dummy::DummyService::AsyncService* service, RpcJob* job, const dummy::HelloRequest* request)
    {
        dummy::HelloReply reply;
        std::string prefix = "From Unary grpc portocol : - Hellooo .. ";
        reply.set_message(prefix + request->name() );

        randomSleepThisThread();
        gServerImpl->mGetHelloReplyResponder[job].sendFunc(&reply);
    }

    static void GetHelloReplyDone(dummy::DummyService::AsyncService* serice, RpcJob* job , bool rpcCancelled)
    {
        gServerImpl->mGetHelloReplyResponder.erase(job);
        delete job;
    }

    void createListHelloReplyRpc()
    {
        ServerStreamingRpcJobHandlers<dummy::DummyService::AsyncService, dummy::HelloRequest, dummy::HelloReply> jobHandlers;
        jobHandlers.rpcJobContextHandler = &ListHelloReplyContextSetterImpl;
        jobHandlers.rpcJobDoneHandler =  &ListHelloReplyDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createListHelloReplyRpc, this);
        jobHandlers.queueRequestHandler = &dummy::DummyService::AsyncService::RequestServerClient;
        jobHandlers.processRequestHandler = &ListHelloReplyProcessor;

       new ServerStreamingRpcJob<dummy::DummyService::AsyncService,dummy::HelloRequest, dummy::HelloReply>(&mDummyService,mCQ.get(),jobHandlers);
    }

    struct ListHelloReplyResponder
    {
        std::function<bool(dummy::HelloReply*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*, ListHelloReplyResponder> mListHelloReplyResponders;
    static void ListHelloReplyContextSetterImpl(dummy::DummyService::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(dummy::HelloReply*)> sendResponse)
    {
        ListHelloReplyResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mListHelloReplyResponders[job] = responder;
    }

    static void ListHelloReplyProcessor(dummy::DummyService::AsyncService* service, RpcJob* job, const dummy::HelloRequest* request)
    {
        auto lo = request->name();
        for(auto f : gServerImpl->mHelloReplyList)
        {
            gServerImpl->mListHelloReplyResponders[job].sendFunc(&f);
            randomSleepThisThread();
        }
        gServerImpl->mListHelloReplyResponders[job].sendFunc(nullptr);
    }

    static void ListHelloReplyDone(dummy::DummyService::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mListHelloReplyResponders.erase(job);
        delete job;
    }

    void createRecordHelloReplyRpc()
    {
        ClientStreamingRpcJobHandlers<dummy::DummyService::AsyncService, dummy::HelloRequest, dummy::HelloReply> jobHandlers;
        jobHandlers.rpcJobContextHandler = &RecordHelloReplyContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &RecordHelloReplyDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createRecordHelloReplyRpc, this);
        jobHandlers.queueRequestHandler = &dummy::DummyService::AsyncService::RequestClientServer;
        jobHandlers.processRequestHandler = &RecordHelloReplyProcessor;

        new ClientStreamingRpcJob<dummy::DummyService::AsyncService, dummy::HelloRequest,dummy::HelloReply>(&mDummyService, mCQ.get(),jobHandlers);
    }
    
    struct RecordHelloReplyResponder
    {
        std::function<bool(dummy::HelloReply*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*,RecordHelloReplyResponder> mRecordHelloReplyResponders;
    static void RecordHelloReplyContextSetterImpl(dummy::DummyService::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(dummy::HelloReply*)> sendResponse)
    {
        RecordHelloReplyResponder responder;
        responder.sendFunc =  sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mRecordHelloReplyResponders[job] = responder;
    }

    struct RecordHelloReplyState
    {
        std::string name;
        int count;
        std::chrono::system_clock::time_point startTime;
        RecordHelloReplyState()
            : name("Hello Reply state")
            ,count(0)
        {

        }
    };
    std::unordered_map<RpcJob*, RecordHelloReplyState> mRecordHelloReplyStateMap;
    static void RecordHelloReplyProcessor(dummy::DummyService::AsyncService* service, RpcJob* job, const dummy::HelloRequest * request)
    {
        RecordHelloReplyState& state = gServerImpl->mRecordHelloReplyStateMap[job];

        if(request)
        {
            state.count++;
            randomSleepThisThread();
        }
        else
        {

            dummy::HelloReply reply;

            std::string temp = std::to_string(state.count);
            reply.set_message("RecordHelloReply : - " + temp);

            gServerImpl->mRecordHelloReplyResponders[job].sendFunc(&reply);

            gServerImpl->mRecordHelloReplyStateMap.erase(job);

            randomSleepThisThread();            
        }
    }

    static void RecordHelloReplyDone(dummy::DummyService::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mRecordHelloReplyResponders.erase(job);
        delete job;
    }

    void createHelloReplyChatRpc()
    {
        BidirectionalStreamingRpcJobHandlers<dummy::DummyService::AsyncService, dummy::HelloRequest, dummy::HelloRequest> jobHandlers;
        jobHandlers.rpcJobContextHandler = &HelloReplyChatContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &HelloReplyChatDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createHelloReplyChatRpc,this);
        jobHandlers.queueRequestHandler = &dummy::DummyService::AsyncService::RequestBidirectional;
        jobHandlers.processRequestHandler = &HelloReplyChatProcessor;

        new BidirectionalStreamingRpcJob<dummy::DummyService::AsyncService,dummy::HelloRequest,dummy::HelloRequest>(&mDummyService,mCQ.get(),jobHandlers);

    }
    struct HelloReplyChatResponder
    {
        std::function<bool(dummy::HelloRequest*)> sendFunc;
        grpc::ServerContext* serverContext;
    };


    std::unordered_map<RpcJob*,HelloReplyChatResponder> mHelloReplyChatResponders;
    static void HelloReplyChatContextSetterImpl(dummy::DummyService::AsyncService* service,RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(dummy::HelloRequest*)> sendResponse)
    {
        HelloReplyChatResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mHelloReplyChatResponders[job] = responder;
    }

    static void HelloReplyChatProcessor(dummy::DummyService::AsyncService* service, RpcJob* job, const dummy::HelloRequest* request)
    {
        if(request)
        {
            dummy::HelloRequest responseRequest(*request);
            gServerImpl-> mHelloReplyChatResponders[job].sendFunc(&responseRequest);
            randomSleepThisThread();
        }
        else
        {
            gServerImpl->mHelloReplyChatResponders[job].sendFunc(nullptr);
            randomSleepThisThread();
        }
    }

    static void HelloReplyChatDone(dummy::DummyService::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mHelloReplyChatResponders.erase(job);
        delete job;
    }

    void HandleRpcs()
    {
        createGetHelloReplyRpc();
        createListHelloReplyRpc();
        createRecordHelloReplyRpc();
        createHelloReplyChatRpc();

        TagInfo tagInfo;
        while(true)
        {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(mCQ->Next((void**)&tagInfo.tagProcessor,&tagInfo.ok));   //GRPC_TODO - handle returned value.

            gIncomingTagsMutex.lock();
            gIncomingTags.push_back(tagInfo);
            gIncomingTagsMutex.unlock();
        }
    }
private:
    std::unique_ptr<grpc::ServerCompletionQueue> mCQ;
    dummy::DummyService::AsyncService mDummyService;
    std::vector<dummy::HelloReply> mHelloReplyList;
    std::unique_ptr<grpc::Server> mServer;
};

 
 static void processRpcs()
 {
     //Implement a busy-wait loop. Not the most efficient thing in the world but would do for this example.
     while(true)
     {
         gIncomingTagsMutex.lock();
         TagList tags = std::move(gIncomingTags);
         gIncomingTagsMutex.unlock();

         while(!tags.empty())
         {
             TagInfo tagInfo = tags.front();
             tags.pop_front();
             (*(tagInfo.tagProcessor))(tagInfo.ok);

             randomSleepThisThread();   //Simulate processing Time.
         };

         randomSleepThisThread();   //yield cpu
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