#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <random>
#include <cmath>

#include "math_streaming.grpc.pb.h"

using grpc::Channel;
using grpc::ClientReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using mathtest::MathRequest;
using mathtest::MathReply;
using mathtest::MathTest;

class MathClient
{
public:
    explicit MathClient(std::shared_ptr<Channel> channel)
        : stub_(MathTest::NewStub(channel)) {}
    
    //Assembles the client's payload, sends it and presents the response
    //from the server.
    int sendRequest(int a, int b)
    {
      //Data we are sending to the server.
       MathRequest request;
       request.set_a(a);
       request.set_b(b);

       //Container for the data we expect from the server.
       MathReply reply;

       //Context for the client. It could be used to convey extra information
       //the server and/or tweak certain RPC behaviors.
       ClientContext context;

       //The producer-consumer queue we use to communicate asynchronously at 
       //gRPC runtime.
       CompletionQueue cq;

       //Storage for the status of the RPC upon completion.
       Status status;

         // stub_->PrepareAsyncsendRequest() creates an RPC object, returning
        // an instance to store in "call" but does not actually start the RPC
        // Because we are using the asynchronous API, we need to hold on to
        // the "call" instance in order to get updates on the ongoing RPC.
        std::unique_ptr<ClientReader<MathReply> > reader(
            stub_->sendRequest(&context, request));

        while (reader->Read(&reply)) 
        {
            std::cout << "Got reply: " << reply.result() << std::endl;
        }

        status = reader->Finish();
        //Act upon the status of the actual RPC.
        if(status.ok())
        {
            return reply.result();
        }else
        {
            return -1;
        }     
    }
    private:
     // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
     std::unique_ptr<MathTest::Stub> stub_;

};

int main(int argc, char** argv)
{
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureChannelCredentials()).
  MathClient client(grpc::CreateChannel(
      "localhost:5000", grpc::InsecureChannelCredentials()));
    int response;

    std::random_device r;

    //Choose a random mean between 1 and 9
    std::default_random_engine el(r());
    std::uniform_int_distribution<int> uniform_dist(1,9);


    int a = uniform_dist(el);
    int b = uniform_dist(el);
    
    response = client.sendRequest(a,b);
    std::cout << "Answer received: " << a << " * " << b << " = " << response << std::endl;

    return EXIT_SUCCESS;
}