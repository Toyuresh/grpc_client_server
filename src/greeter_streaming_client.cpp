#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "hellostreamingworld.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;
using hellostreamingworld::HelloRequest;
using hellostreamingworld::HelloReply;
using hellostreamingworld::MultiGreeter;

class GreeterClient
{
public:
    GreeterClient(std::shared_ptr<Channel> channel)
        : stub_(MultiGreeter::NewStub(channel))
    {}

    void SayHello(const std::string& user, const std::string& num_greetings)
    {
        HelloRequest request;
        request.set_name(user);
        request.set_num_greetings(num_greetings);

        ClientContext context;
        std::unique_ptr<ClientReader<HelloReply>> reader(stub_->sayHello(&context, request));

        HelloReply reply;
        while (reader->Read(&reply)) 
        {
            std::cout << "Got reply: " << reply.message() << std::endl;
        }

        Status status = reader->Finish();
        if (status.ok()) 
        {
            std::cout << "sayHello rpc succeeded." << std::endl;
        } 
        else 
        {
            std::cout << "sayHello rpc failed." << std::endl;
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
        }
    }

private:
    std::unique_ptr<MultiGreeter::Stub> stub_;
};

int main(int argc, char** argv)
{
    GreeterClient greeter(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    std::string user("world");
    greeter.SayHello(user, "123");

    return 0;
}