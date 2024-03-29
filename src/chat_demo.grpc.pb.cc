// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: chat_demo.proto

#include "chat_demo.pb.h"
#include "chat_demo.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/method_handler.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace chatDemo {

static const char* ChatService_method_names[] = {
  "/chatDemo.ChatService/ClientSide",
};

std::unique_ptr< ChatService::Stub> ChatService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< ChatService::Stub> stub(new ChatService::Stub(channel));
  return stub;
}

ChatService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_ClientSide_(ChatService_method_names[0], ::grpc::internal::RpcMethod::BIDI_STREAMING, channel)
  {}

::grpc::ClientReaderWriter< ::chatDemo::Connect, ::chatDemo::Connect>* ChatService::Stub::ClientSideRaw(::grpc::ClientContext* context) {
  return ::grpc_impl::internal::ClientReaderWriterFactory< ::chatDemo::Connect, ::chatDemo::Connect>::Create(channel_.get(), rpcmethod_ClientSide_, context);
}

void ChatService::Stub::experimental_async::ClientSide(::grpc::ClientContext* context, ::grpc::experimental::ClientBidiReactor< ::chatDemo::Connect,::chatDemo::Connect>* reactor) {
  ::grpc_impl::internal::ClientCallbackReaderWriterFactory< ::chatDemo::Connect,::chatDemo::Connect>::Create(stub_->channel_.get(), stub_->rpcmethod_ClientSide_, context, reactor);
}

::grpc::ClientAsyncReaderWriter< ::chatDemo::Connect, ::chatDemo::Connect>* ChatService::Stub::AsyncClientSideRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc_impl::internal::ClientAsyncReaderWriterFactory< ::chatDemo::Connect, ::chatDemo::Connect>::Create(channel_.get(), cq, rpcmethod_ClientSide_, context, true, tag);
}

::grpc::ClientAsyncReaderWriter< ::chatDemo::Connect, ::chatDemo::Connect>* ChatService::Stub::PrepareAsyncClientSideRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncReaderWriterFactory< ::chatDemo::Connect, ::chatDemo::Connect>::Create(channel_.get(), cq, rpcmethod_ClientSide_, context, false, nullptr);
}

ChatService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ChatService_method_names[0],
      ::grpc::internal::RpcMethod::BIDI_STREAMING,
      new ::grpc::internal::BidiStreamingHandler< ChatService::Service, ::chatDemo::Connect, ::chatDemo::Connect>(
          std::mem_fn(&ChatService::Service::ClientSide), this)));
}

ChatService::Service::~Service() {
}

::grpc::Status ChatService::Service::ClientSide(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::chatDemo::Connect, ::chatDemo::Connect>* stream) {
  (void) context;
  (void) stream;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace chatDemo

