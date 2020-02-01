#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <grpc/support/log.h>

#include "hellostreamingworld.grpc.pb.h"

using grpc::Alarm;
using grpc::Server;
using grpc::ServerAsyncWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using hellostreamingworld::HelloRequest;
using hellostreamingworld::HelloReply;
using hellostreamingworld::MultiGreeter;

class ServerImpl final
{
public:
    ~ServerImpl()
    {
        server_->Shutdown();
        cq_->Shutdown();
    }

    void Run()
    {
        std::string server_address("0.0.0.0:50051");

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);

        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        HandleRpcs();
    }

private:
    class CallData
    {
    public:
        CallData(MultiGreeter::AsyncService* service, ServerCompletionQueue* cq)
            : service_(service)
            , cq_(cq)
            , responder_(&ctx_)
            , status_(CREATE)
            , times_(0)
        {
            Proceed();
        }

        void Proceed()
        {
            if (status_ == CREATE)
            {
                status_ = PROCESS;
                service_->RequestsayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
            }
            else if (status_ == PROCESS)
            {
                // Now that we go through this stage multiple times, 
                // we don't want to create a new instance every time.
                // Refer to gRPC's original example if you don't understand 
                // why we create a new instance of CallData here.
                if (times_ == 0)
                {
                    new CallData(service_, cq_);
                }

                if (times_++ >= 6)
                {
                    status_ = FINISH;
                    responder_.Finish(Status::OK, this);
            	}
            	else
            	{
                    if (HasData())
                    {
                        std::string prefix("Hello ");
                        reply_.set_message(prefix + request_.name() + ", no " + request_.num_greetings());

                        responder_.Write(reply_, this);
                    }
                    else
                    {
                        // If there's no data to be written, put the task back into the queue
                        PutTaskBackToQueue();
                    }
            	}
            }
            else
            {
                GPR_ASSERT(status_ == FINISH);
                delete this;
            }
        }

    private:
        bool HasData() 
        {
            // A dummy condition to make it sometimes have data and sometimes not
            return times_ % 2 == 0;
        }

        void PutTaskBackToQueue()
        {
            alarm_.Set(cq_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), this);
        }

        MultiGreeter::AsyncService* service_;
        ServerCompletionQueue* cq_;
        ServerContext ctx_;

        HelloRequest request_;
        HelloReply reply_;

        ServerAsyncWriter<HelloReply> responder_;

        int times_;
        Alarm alarm_;

        enum CallStatus
        {
            CREATE,
            PROCESS,
            FINISH
        };
        CallStatus status_; // The current serving state.
    };

    void HandleRpcs()
    {
        new CallData(&service_, cq_.get());
        void* tag; // uniquely identifies a request.
        bool ok;
        while (true)
        {
            GPR_ASSERT(cq_->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<CallData*>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    MultiGreeter::AsyncService service_;
    std::unique_ptr<Server> server_;
};

int main(int argc, char** argv)
{
    ServerImpl server;
    server.Run();

    return 0;
}