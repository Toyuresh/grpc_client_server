

#include "grpcchat.grpc.pb.h"
#include <grpcpp/server_builder.h>

using grpc::Status;
using grpc::Server;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerBuilder;
using grpcchat::GrpcChat;
using grpcchat::Msg;

class ServiceImpl final : public GrpcChat::Service{

public:
    explicit ServiceImpl(){}

    Status yellEcho(ServerContext *context, const Msg *input, Msg *output) override{
        output->set_content(input->content());
        return Status::OK;
    }

    Status yellEchoing(ServerContext *context, const Msg *input, ServerWriter<Msg> *writer) override{
        Msg output;
        output.set_content(input->content());
        for(size_t i = 0; i < 10;++i){
            writer->Write(output);
        }
        return Status::OK;
    }

    Status yellingEcho(ServerContext *context, ServerReader<Msg> *reader, Msg *output) override{
        Msg input;
        std::string msg;
        while(reader->Read(&input)){
            msg.append(input.content() + " ");
        }
        output->set_content(msg);
        return Status::OK;
    }

    Status yellingEchoing(ServerContext *context, ServerReaderWriter<Msg,Msg> *stream) override {
        Msg msg;
        while(stream->Read(&msg)){
            stream->Write(msg);
        }
        return Status::OK;
    }
};

void RunServer(const std::string *serv_addr){

    //create server and start listening
    ServiceImpl service;
    ServerBuilder builder;

    builder.AddListeningPort(*serv_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << *serv_addr << std::endl;
    server->Wait();
}

int main(int argc, char* argv[]){
    //Expect arg for address and port in format: "1.2.3.4:5678"
    if(argc != 2){
        std::cout << "Usage: "<<argv[0]<<" host_address:port" << std::endl;
        return -1;
    }
    std::string addr(argv[1]);
    RunServer(&addr);

    return 0;
}