#include <string>
#include <grpcpp/grpcpp.h>
#include "mathtest.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mathtest::MathTest;
using mathtest::MathRequest;
using mathtest::MathReply;

class MathTestClient
{
public:
    MathTestClient(std::shared_ptr<Channel> channel) : stub_(MathTest::NewStub(channel)){}

    int sendRequest(int a, int b)
    {
        MathRequest request;

        request.set_a(a);
        request.set_b(b);

        MathReply reply;

        ClientContext context;

        Status status = stub_->sendRequest(&context, request, &reply);

        if(status.ok())
        {
            return reply.result();
        }else
        {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return EXIT_FAILURE;
        }   
    }
private:
    std::unique_ptr<MathTest::Stub> stub_; 
};

void Run()
{
    std::string address("0.0.0.0:5000");
    MathTestClient client(grpc::CreateChannel(address,grpc::InsecureChannelCredentials()));

    int response;

    int a = 5;
    int b = 10;
    std::cout << "Enter two number : ";
    std::cout << "a : ";
    std::cin >> a;
    std::cout << std::endl;
    std::cout << "b : ";
    std::cin >> b;
    std::cout << std::endl;
    
    response = client.sendRequest(a,b);
    std::cout << "Answer received: " << a << " * " << b << " = " << response << std::endl;
}

int main(int argc, char** argv)
{
    Run();
    return EXIT_SUCCESS;
}