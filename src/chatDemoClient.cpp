#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <atomic>

#include <grpcpp/grpcpp.h>
#include "chat_demo.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::ChannelArguments;
using grpc::Status;
using chatDemo::ChatService;
using chatDemo::Connect;


static std::atomic_int32_t routeChatResponseCount = 0;

static const uint16_t kThreadsCount = 10;

Connect MakeRouteNote(const std::string& message, const int& id_)
{
    Connect n;
    n.set_text(message);
    n.set_id(id_);
    return n;
}

class ChatClient
{
public:
    ChatClient(std::shared_ptr<Channel> channel)
        : stub_(ChatService::NewStub(channel))
        {

        }

    void RouteChat() {
        ClientContext context;

        std::shared_ptr<ClientReaderWriter<Connect, Connect> > stream(
            stub_->ClientSide(&context));

        std::thread writer([stream]() {
        std::vector<Connect> notes{
            MakeRouteNote("First message",1),
            MakeRouteNote("Second message",2),
            MakeRouteNote("Third message",3),
            MakeRouteNote("Fourth message",4)};
        for (const Connect& note : notes) 
            {
                std::cout << "Sending message " << note.text() << note.id() <<std::endl;
                stream->Write(note);
            }
        stream->WritesDone();
        });

        Connect server_note;
        while (stream->Read(&server_note)) {
        std::cout << "Got message " << server_note.text() << server_note.id() << std::endl;
        ++routeChatResponseCount;
        }
        writer.join();
        Status status = stream->Finish();
        if (!status.ok()) {
        std::cout << "RouteChat rpc failed." << std::endl;
        }
    }

    std::unique_ptr<ChatService::Stub> stub_;
};


static void randomSleepThisThread()
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<> dist{ 10, 100 };
    std::this_thread::sleep_for(std::chrono::milliseconds{ dist(generator) });
}


static void routeChat(ChatClient* client)
{
    randomSleepThisThread();

    std::cout << "-------------- RouteChat --------------" << std::endl;
    client->RouteChat();
}

int main(int argc, char** argv)
{  
    ChatClient client(grpc::CreateChannel("localhost:50051",grpc::InsecureChannelCredentials()));

    std::thread routeChatThreads[kThreadsCount];
    for (uint8_t index = 0; index < kThreadsCount; ++index)
    {
        routeChatThreads[index] = std::thread(routeChat, &client);
    }

    for (auto& th : routeChatThreads)
    {
        th.join();
    }
    std::cout << "-------------- RouteChat Response Count = " << routeChatResponseCount << " --------------" << std::endl;
    return EXIT_SUCCESS;
}