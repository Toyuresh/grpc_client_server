// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: route_guide.proto

#include "route_guide.pb.h"
#include "route_guide.grpc.pb.h"

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
namespace routeguide {

static const char* RouteGuide_method_names[] = {
  "/routeguide.RouteGuide/GetFeature",
  "/routeguide.RouteGuide/ListFeatures",
  "/routeguide.RouteGuide/RecordRoute",
  "/routeguide.RouteGuide/RouteChat",
};

std::unique_ptr< RouteGuide::Stub> RouteGuide::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< RouteGuide::Stub> stub(new RouteGuide::Stub(channel));
  return stub;
}

RouteGuide::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_GetFeature_(RouteGuide_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_ListFeatures_(RouteGuide_method_names[1], ::grpc::internal::RpcMethod::SERVER_STREAMING, channel)
  , rpcmethod_RecordRoute_(RouteGuide_method_names[2], ::grpc::internal::RpcMethod::CLIENT_STREAMING, channel)
  , rpcmethod_RouteChat_(RouteGuide_method_names[3], ::grpc::internal::RpcMethod::BIDI_STREAMING, channel)
  {}

::grpc::Status RouteGuide::Stub::GetFeature(::grpc::ClientContext* context, const ::routeguide::Point& request, ::routeguide::Feature* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_GetFeature_, context, request, response);
}

void RouteGuide::Stub::experimental_async::GetFeature(::grpc::ClientContext* context, const ::routeguide::Point* request, ::routeguide::Feature* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetFeature_, context, request, response, std::move(f));
}

void RouteGuide::Stub::experimental_async::GetFeature(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::routeguide::Feature* response, std::function<void(::grpc::Status)> f) {
  ::grpc_impl::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetFeature_, context, request, response, std::move(f));
}

void RouteGuide::Stub::experimental_async::GetFeature(::grpc::ClientContext* context, const ::routeguide::Point* request, ::routeguide::Feature* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_GetFeature_, context, request, response, reactor);
}

void RouteGuide::Stub::experimental_async::GetFeature(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::routeguide::Feature* response, ::grpc::experimental::ClientUnaryReactor* reactor) {
  ::grpc_impl::internal::ClientCallbackUnaryFactory::Create(stub_->channel_.get(), stub_->rpcmethod_GetFeature_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::routeguide::Feature>* RouteGuide::Stub::AsyncGetFeatureRaw(::grpc::ClientContext* context, const ::routeguide::Point& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::routeguide::Feature>::Create(channel_.get(), cq, rpcmethod_GetFeature_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::routeguide::Feature>* RouteGuide::Stub::PrepareAsyncGetFeatureRaw(::grpc::ClientContext* context, const ::routeguide::Point& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncResponseReaderFactory< ::routeguide::Feature>::Create(channel_.get(), cq, rpcmethod_GetFeature_, context, request, false);
}

::grpc::ClientReader< ::routeguide::Feature>* RouteGuide::Stub::ListFeaturesRaw(::grpc::ClientContext* context, const ::routeguide::Rectangle& request) {
  return ::grpc_impl::internal::ClientReaderFactory< ::routeguide::Feature>::Create(channel_.get(), rpcmethod_ListFeatures_, context, request);
}

void RouteGuide::Stub::experimental_async::ListFeatures(::grpc::ClientContext* context, ::routeguide::Rectangle* request, ::grpc::experimental::ClientReadReactor< ::routeguide::Feature>* reactor) {
  ::grpc_impl::internal::ClientCallbackReaderFactory< ::routeguide::Feature>::Create(stub_->channel_.get(), stub_->rpcmethod_ListFeatures_, context, request, reactor);
}

::grpc::ClientAsyncReader< ::routeguide::Feature>* RouteGuide::Stub::AsyncListFeaturesRaw(::grpc::ClientContext* context, const ::routeguide::Rectangle& request, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc_impl::internal::ClientAsyncReaderFactory< ::routeguide::Feature>::Create(channel_.get(), cq, rpcmethod_ListFeatures_, context, request, true, tag);
}

::grpc::ClientAsyncReader< ::routeguide::Feature>* RouteGuide::Stub::PrepareAsyncListFeaturesRaw(::grpc::ClientContext* context, const ::routeguide::Rectangle& request, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncReaderFactory< ::routeguide::Feature>::Create(channel_.get(), cq, rpcmethod_ListFeatures_, context, request, false, nullptr);
}

::grpc::ClientWriter< ::routeguide::Point>* RouteGuide::Stub::RecordRouteRaw(::grpc::ClientContext* context, ::routeguide::RouteSummary* response) {
  return ::grpc_impl::internal::ClientWriterFactory< ::routeguide::Point>::Create(channel_.get(), rpcmethod_RecordRoute_, context, response);
}

void RouteGuide::Stub::experimental_async::RecordRoute(::grpc::ClientContext* context, ::routeguide::RouteSummary* response, ::grpc::experimental::ClientWriteReactor< ::routeguide::Point>* reactor) {
  ::grpc_impl::internal::ClientCallbackWriterFactory< ::routeguide::Point>::Create(stub_->channel_.get(), stub_->rpcmethod_RecordRoute_, context, response, reactor);
}

::grpc::ClientAsyncWriter< ::routeguide::Point>* RouteGuide::Stub::AsyncRecordRouteRaw(::grpc::ClientContext* context, ::routeguide::RouteSummary* response, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc_impl::internal::ClientAsyncWriterFactory< ::routeguide::Point>::Create(channel_.get(), cq, rpcmethod_RecordRoute_, context, response, true, tag);
}

::grpc::ClientAsyncWriter< ::routeguide::Point>* RouteGuide::Stub::PrepareAsyncRecordRouteRaw(::grpc::ClientContext* context, ::routeguide::RouteSummary* response, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncWriterFactory< ::routeguide::Point>::Create(channel_.get(), cq, rpcmethod_RecordRoute_, context, response, false, nullptr);
}

::grpc::ClientReaderWriter< ::routeguide::RouteNote, ::routeguide::RouteNote>* RouteGuide::Stub::RouteChatRaw(::grpc::ClientContext* context) {
  return ::grpc_impl::internal::ClientReaderWriterFactory< ::routeguide::RouteNote, ::routeguide::RouteNote>::Create(channel_.get(), rpcmethod_RouteChat_, context);
}

void RouteGuide::Stub::experimental_async::RouteChat(::grpc::ClientContext* context, ::grpc::experimental::ClientBidiReactor< ::routeguide::RouteNote,::routeguide::RouteNote>* reactor) {
  ::grpc_impl::internal::ClientCallbackReaderWriterFactory< ::routeguide::RouteNote,::routeguide::RouteNote>::Create(stub_->channel_.get(), stub_->rpcmethod_RouteChat_, context, reactor);
}

::grpc::ClientAsyncReaderWriter< ::routeguide::RouteNote, ::routeguide::RouteNote>* RouteGuide::Stub::AsyncRouteChatRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq, void* tag) {
  return ::grpc_impl::internal::ClientAsyncReaderWriterFactory< ::routeguide::RouteNote, ::routeguide::RouteNote>::Create(channel_.get(), cq, rpcmethod_RouteChat_, context, true, tag);
}

::grpc::ClientAsyncReaderWriter< ::routeguide::RouteNote, ::routeguide::RouteNote>* RouteGuide::Stub::PrepareAsyncRouteChatRaw(::grpc::ClientContext* context, ::grpc::CompletionQueue* cq) {
  return ::grpc_impl::internal::ClientAsyncReaderWriterFactory< ::routeguide::RouteNote, ::routeguide::RouteNote>::Create(channel_.get(), cq, rpcmethod_RouteChat_, context, false, nullptr);
}

RouteGuide::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RouteGuide_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RouteGuide::Service, ::routeguide::Point, ::routeguide::Feature>(
          std::mem_fn(&RouteGuide::Service::GetFeature), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RouteGuide_method_names[1],
      ::grpc::internal::RpcMethod::SERVER_STREAMING,
      new ::grpc::internal::ServerStreamingHandler< RouteGuide::Service, ::routeguide::Rectangle, ::routeguide::Feature>(
          std::mem_fn(&RouteGuide::Service::ListFeatures), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RouteGuide_method_names[2],
      ::grpc::internal::RpcMethod::CLIENT_STREAMING,
      new ::grpc::internal::ClientStreamingHandler< RouteGuide::Service, ::routeguide::Point, ::routeguide::RouteSummary>(
          std::mem_fn(&RouteGuide::Service::RecordRoute), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RouteGuide_method_names[3],
      ::grpc::internal::RpcMethod::BIDI_STREAMING,
      new ::grpc::internal::BidiStreamingHandler< RouteGuide::Service, ::routeguide::RouteNote, ::routeguide::RouteNote>(
          std::mem_fn(&RouteGuide::Service::RouteChat), this)));
}

RouteGuide::Service::~Service() {
}

::grpc::Status RouteGuide::Service::GetFeature(::grpc::ServerContext* context, const ::routeguide::Point* request, ::routeguide::Feature* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RouteGuide::Service::ListFeatures(::grpc::ServerContext* context, const ::routeguide::Rectangle* request, ::grpc::ServerWriter< ::routeguide::Feature>* writer) {
  (void) context;
  (void) request;
  (void) writer;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RouteGuide::Service::RecordRoute(::grpc::ServerContext* context, ::grpc::ServerReader< ::routeguide::Point>* reader, ::routeguide::RouteSummary* response) {
  (void) context;
  (void) reader;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RouteGuide::Service::RouteChat(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::routeguide::RouteNote, ::routeguide::RouteNote>* stream) {
  (void) context;
  (void) stream;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace routeguide

