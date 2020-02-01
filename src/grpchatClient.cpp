
#include <thread>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "grpcchat.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::ClientReaderWriter;
using grpc::Status;
using grpcchat::GrpcChat;
using grpcchat::Msg;


class ChatClient{
public:
    ChatClient(std::shared_ptr<Channel> channel):stub(GrpcChat::NewStub(channel)){}

    void yell(const std::string &input){
        ClientContext context;
        Msg request, response;
        request.set_content(input);
        Status status = stub->yellEcho(&context, request, &response);
        std::cout << "Server reply: "<< response.content() << std::endl;
    }

    void yellEchoing(const std::string &input){
        ClientContext context;
        Msg request, response;
        request.set_content(input);
        std::unique_ptr<ClientReader<Msg>> reader(stub->yellEchoing(&context,request));
        while(reader->Read(&response)){
            std::cout << "Server reply: " << response.content() <<std::endl;
        }
        Status status = reader->Finish();
        if (status.ok()) {
            std::cout << "yellEchoing rpc succeeded." << std::endl;
        } else {
            std::cout << "yellEchoing rpc failed." << std::endl;
        }
    }

protected:
    std::unique_ptr<GrpcChat::Stub> stub;
};

class YellChatClient : public ChatClient{
public:
    YellChatClient(std::shared_ptr<Channel> channel): ChatClient(channel), writer(stub->yellingEcho(&context,&response)){}

    bool yelling(const std::string &input){
        Msg request;
        request.set_content(input);
        if(!writer->Write(request))
            return false; //broken stream
    }

    void finishYelling(){
        writer->WritesDone();
        Status status = writer->Finish();
        if(status.ok()){
            std::cout << "Server reply: " << response.content() << std::endl;
        }else {
            std::cout << "yellingEcho rpc failed." << std::endl;
        }
    }

private:
    ClientContext context;
    Msg response;
    std::unique_ptr<ClientWriter<Msg> > writer;
};

class YellStreamEchoStreamClient : public ChatClient{
public:
    YellStreamEchoStreamClient(std::shared_ptr<Channel> channel): ChatClient(channel),stream(stub->yellingEchoing(&context)){}

    void yelling(const std::string &input){
        Msg request;
        request.set_content(input);
        stream->Write(request);
    }

    void finishYelling(){
        stream->WritesDone();
    }

    void recvEchoing(){
        Msg response;
        while(stream->Read(&response)){
            std::cout << "Server reply: " << response.content() <<std::endl;
        }
    }

    void checkStatus(){
        Status status = stream->Finish();
        if (status.ok()) {
            std::cout << "yellingEchoing rpc succeeded." << std::endl;
        } else {
            std::cout << "yellingEchoing rpc failed." << std::endl;
        }
    }

private:
    ClientContext context;
    std::unique_ptr<ClientReaderWriter<Msg,Msg>> stream;
};


int main(int argc, char *argv[]){

    if(argc != 2){
        std::cout << "Usage: " << argv[0] << " host_address:port" << std::endl;
        return -1;
    }

    int choice = 0;

    ChatClient client(grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials()));
    std::cout << "Select mode: 1)1-1 2)1-many 3)many-1 4)many-many" <<std::endl;
    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');

    char buffer[1024];
    switch(choice){
        case 1: //yell()
            while(true){
                std::cout <<"Enter message: "<<std::endl;
                std::cin.getline(buffer,sizeof(buffer));
                client.yell(buffer);
            }
        case 2: //yellEchoing()
            while(true){
                std::cout <<"Enter message: "<<std::endl;
                std::cin.getline(buffer,sizeof(buffer));
                client.yellEchoing(buffer);
            }
        case 3: //yellingEcho()
            while(true){
                //create new ClientContext and ClientWriter
                YellChatClient yell_client(grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials()));
                while(true){
                    std::cout <<"Enter messages, type 'q' to end: "<<std::endl;
                    std::cin.getline(buffer,sizeof(buffer));
                    if(std::strcmp(buffer,"q") == 0){
                        yell_client.finishYelling();
                        break;
                    }else{
                        yell_client.yelling(buffer);
                    }
                }

            }
        case 4: //yellingEchoing()
            while(true){
                //create new ClientContext and a ClientReaderWriter, read and write cooperate separately
                YellStreamEchoStreamClient stream_client(grpc::CreateChannel(argv[1],grpc::InsecureChannelCredentials()));

                //start new thread to "yell" user input to server
                std::thread writer([&stream_client, &buffer](){
                    while(true){
                        std::cout<<"Enter messages, type 'q' to end: "<<std::endl;
                        std::cin.getline(buffer, sizeof(buffer));
                        if(std::strcmp(buffer,"q") == 0){
                            stream_client.finishYelling();
                            break;
                        }else{
                            stream_client.yelling(buffer);
                        }
                    }
                });

                //receive "echo" from server
                stream_client.recvEchoing();

                writer.join();
                stream_client.checkStatus();

            }
    }


}