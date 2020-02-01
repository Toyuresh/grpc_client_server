#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>

#include "dummy.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using dummy::HelloReply;
using dummy::HelloRequest;
using dummy::DummyService;

class DummyClient
{
public:
    DummyClient(std::shared_ptr<Channel> channel)
        : stub_(DummyService::NewStub(channel))
        {

        }

    void Unary()
    {
         ClientContext context;
        HelloRequest request_;
        request_.set_name(" Unary world");
        
        HelloReply reply_;

        Status status = stub_->Unary(&context,request_,&reply_);

        if(status.ok())
        {
            std::cout << reply_.message() << std::endl;
        }
        else
        {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            std::cout << " RPC failed " << std::endl;
        }
    }

    void Server_To_Client()
    {
         ClientContext context;
        dummy::HelloRequest request;

        HelloReply reply;

        request.set_name(" Server_To_Client ");

        std::unique_ptr<ClientReader<HelloReply>> reader(
            stub_->ServerClient(&context,request));
        while(reader->Read(&reply))
        {
            std::cout << "Found reply Message ..  " 
                        << reply.message() << std::endl;
        }

        Status status = reader->Finish();
        if(status.ok())
        {
            std::cout << "ListFeatures rpc succeeded . " << std::endl;
        }
        else
        {
            std::cout << "ListFeatures rpc failed. " << std::endl;
        }
    }

    void Client_To_Server()
    {

        ClientContext context;
        HelloRequest request;
        HelloReply reply;
      

        std::unique_ptr<ClientWriter<HelloRequest>> writer(
            stub_->ClientServer(&context,&reply));
            std::string s = " Client To Server";
            for(int i =0;i<10;i++)
            {
                request.set_name(s);
                if(!writer->Write(request))
                {
                    //broken stream;
                    break;
                }
            }

            writer->WritesDone();
            Status status = writer->Finish();

            if(status.ok())
            {
                std::cout << " Client to Server rpc Succeed " << std::endl;
            }
            else
            {
                std::cout << " Client to Server rpc failed. " << std::endl;
            }
            
    }

    void Bidirectional()
    {
        ClientContext context;
        HelloRequest request;

        
        std::shared_ptr<ClientReaderWriter<HelloRequest,HelloRequest>> stream(
            stub_->Bidirectional(&context));

            std::thread writer([stream,&request](){
                for(int i=0;i<=10;i++)
                {
                    request.set_name(" Bidirectional ..");
                    stream->Write(request);
                }
                stream->WritesDone();
            });
        
        while(stream->Read(&request))
        {
            std::cout << request.name() << std::endl;
        }

        writer.join();
        Status status = stream->Finish();
        if(!status.ok())
        {
            std::cout << " Bidirectional rpc failed.. " << std::endl;
        }
    }

private:
   
    std::unique_ptr<DummyService::Stub> stub_;
    std::vector<HelloReply> helloReply_list_;
};


int main(int argc, char** argv)
{
    DummyClient guide(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));


    std::cout << "----------------Unary----------------------->" << std::endl;
    guide.Unary();
    std::cout << "<----------------Server-To-Client ------------------->" << std::endl;
    guide.Server_To_Client();
    std::cout << "<----------------Client_To_Server -------------------->" << std::endl;
    guide.Client_To_Server();
    std::cout << "<----------------Bidirectional ------------------------>" << std::endl;
    guide.Bidirectional();

    return EXIT_SUCCESS;
}