#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <grpcpp/alarm.h>

#include "chat_demo.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using chatDemo::Connect;
using chatDemo::ChatService;

class ServerImpl final
{
public:
    ~ServerImpl()
    {
        num_clients_ = 0;
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
        CallData(ChatService::AsyncService* service, ServerCompletionQueue* cq, int client_id)
            : service_(service)
            , cq_(cq)
            , responder_(&ctx_)
            , status_(CREATE)
            , times_(0)
            , client_id_(client_id)
        {
            std::cout << "Created CallData " << client_id_ << std::endl;
            Proceed();
        }

        void Proceed()
        {
            if (status_ == CREATE)
            {
                status_ = PROCESS;
                service_->RequestClientSide(&ctx_,&responder_,cq_,cq_,this);
            }
            else if (status_ == PROCESS)
            {
                std::cout << "Client being processed: " << client_id_  << std::endl;
                
                new CallData(service_, cq_, client_id_ + 1);
                
                if(times_ >= 1)
                {
                    times_ = 0;
                    std::cout << " 1 " << std::endl;
                    status_ = FINISH;
                    responder_.Finish(Status::OK, this);
                }
                else
                {
                    times_++;
                    std::cout <<  " 2 " << std::endl;
                    responder_.Read(&msg,this);
                    Connect msgWrite = msg;
                    // For illustrating the queue-to-front behaviour
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);

                    msgWrite.set_text(" -->  " + msgWrite.text());
                    responder_.Write(msgWrite,this);
                    status_ = PUSH_TO_BACK;

                }
            }
            else if(status_ == PUSH_TO_BACK)
            {
                std::cout << " 3 " << std::endl;
                status_ = PROCESS;
                alarm_.Set(cq_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), this);
            }
            else
            {
                std::cout << " 4 " << std::endl;
                GPR_ASSERT(status_ == FINISH);
                delete this;
            }
        }
        void Stop()
        {
            std::cerr << " 5 " << std::endl;
            status_ = CallStatus::FINISH;
        }

    private:
        ChatService::AsyncService* service_;
        ServerCompletionQueue* cq_;
        grpc::ServerContext ctx_;

        grpc::Alarm alarm_;
        Connect msg;
        ServerAsyncReaderWriter<Connect,Connect> responder_;

        int times_;
        int client_id_;

        enum CallStatus
        {
            CREATE,
            PROCESS,
            FINISH,
            PUSH_TO_BACK
        };
        CallStatus status_; // The current serving state.
    };

    void HandleRpcs()
    {
        new CallData(&service_, cq_.get(), num_clients_++);
        void* tag; // uniquely identifies a request.
        bool ok;
        while (true)
        {
            GPR_ASSERT(cq_->Next(&tag, &ok));
            if(!ok)
            {
                static_cast<CallData*>(tag)->Stop();
                continue;
            }
            static_cast<CallData*>(tag)->Proceed();
        }
    }

    int num_clients_ = 0;
    std::unique_ptr<ServerCompletionQueue> cq_;
    ChatService::AsyncService service_;
    std::unique_ptr<Server> server_;
};

int main(int argc, char** argv)
{
    ServerImpl server;
    server.Run();

    return EXIT_SUCCESS;
}