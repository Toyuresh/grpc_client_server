#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <grpcpp/alarm.h>

#include "math_streaming.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using mathtest::MathTest;
using mathtest::MathRequest;
using mathtest::MathReply;

class ServerImpl final
{
public:
    ~ServerImpl()
    {
        num_clients_ = 0;
        server_->Shutdown();
    //Always shutdown the completion queue after the server
        cq_->Shutdown();
    }
    //There is no shutdown handling in this code.
    void Run()
    {
        std::string server_address("0.0.0.0:5000");

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        //Register "service_" as the instance through which we'll communicate with
        //clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&service_);
        //Get hold of the completion queue used for the asynchronous communication
        //with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        //Finally assemble the server.
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        //Proceed to the server's main loop.
        HandleRpcs();
    }
private:
    //Class encompasing the state and logic need to serve a request.
    class CallData
    {
    public:
        //Take in the "service" instance (in this case representing an asynchronous
        //server) and the completion queue "cq" used for asynchronous communication
        //with the gRPC runtime.
        CallData(MathTest::AsyncService* service, ServerCompletionQueue* cq, int client_id)
            : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), reply_times_(0),client_id_(client_id)
            {
                //Invoke the serving logic right away.
                std::cout << "Created CallData :- " << client_id_ << std::endl;
                Proceed();
            }
        void Proceed()
        {
            if(status_ == CREATE)
            {
                //Make this instance progress to the PROCESS state.
                status_  = PROCESS;

                // As part of the initial CREATE state, we *request* that the system
                // start processing SayHello requests. In this request, "this" acts are
                // the tag uniquely identifying the request (so that different CallData
                // instances can serve different requests concurrently), in this case
                // the memory address of this CallData instance.
                service_->RequestsendRequest(&ctx_,&request_,&responder_,cq_,cq_,this);
            }else if(status_ == PROCESS)
            {
                // Spawn a new CallData instance to serve new clients while we process
                // the one for this CallData. The instance will deallocate itself as
                // part of its FINISH state.
                std::cout << "Client being processed: " << client_id_ << std::endl;

                if(reply_times_ == 0)
                {
                    new CallData(service_,cq_,client_id_+1);
                }
                
                if(reply_times_++ >= 3)
                {
                       
                    // And we are done! Let the gRPC runtime know we've finished, using the
                    // memory address of this instance as the uniquely identifying tag for
                    // the event.
                    status_ = FINISH;
                    responder_.Finish(Status::OK, this);
                }
                else
                {
                    //The actual processing.
                    int a = request_.a();
                    int b = request_.b();

                    reply_.set_result(a*b);
                    
                    //For illustrating the queue-to_front behaviour
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);

                    responder_.Write(reply_, this);
                    
                    status_ = PUSH_TO_BACK;
                }
            }
            else if(status_ == PUSH_TO_BACK)
            {
                status_ = PROCESS;
                alarm_.Set(cq_,gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME),this);
            }
            else
            {
                GPR_ASSERT(status_ == FINISH);
                //Once in the FINISH state, deallocate ourselves(CallData).
                delete this;
            }
        }

        void Stop()
        {
            std::cerr << "Finishing up client " << client_id_ << std::endl;
            status_ = CallStatus::FINISH; 
        }
    private:
        //The means of communication with gRPC runtime for an asynchronous server.
        MathTest::AsyncService* service_;
        //The producer-consumer queue where for asynchronous server notifaction
        ServerCompletionQueue* cq_;
        //Context for the rpc, allowing to tweak aspects of it such as the use
        // of compression, authentication, as well as to send metadata back to the 
        //client.
        ServerContext ctx_;

        //What we get from the client.
        MathRequest request_;
        //What we send back to the client.
        MathReply reply_;

        grpc::Alarm alarm_;

        //The means to get back to the client.
        ServerAsyncWriter<MathReply> responder_;

        int reply_times_;
        int client_id_;

        //Let's implement a tiny state machine with the following states.
        enum CallStatus { CREATE, PROCESS, FINISH, PUSH_TO_BACK };
        CallStatus status_;  //The current serving state.

    };

    //This can be run in multiple threads if needed.
    void HandleRpcs()
    {
        //Spawn a new CallData instance to serve new clients.
        new CallData(&service_,cq_.get(),num_clients_++);
        void* tag;  //uniquely identifies a request.
        bool ok;
        while(true)
        {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(cq_->Next(&tag,&ok));
            if(!ok)
            {
                static_cast<CallData*>(tag)->Stop();
                continue;
            }
            GPR_ASSERT(ok);
            static_cast<CallData*>(tag)->Proceed();
        }
    }

    int num_clients_ = 0;
    std::unique_ptr<ServerCompletionQueue> cq_;
    MathTest::AsyncService service_;
    std::unique_ptr<Server> server_;
};

int main(int argc, char** argv)
{
    ServerImpl server;
    server.Run();

    return EXIT_SUCCESS;
}