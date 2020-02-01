cmake_minimum_required(VERSION 3.0.1)
project(cppdummy CXX)

set(CMAKE_CXX_STANDARD 17)

set (THREADS_PREFER_PTHREAD_FLAG ON)
set( CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -pthread -Wall -Wextra -Wpedantic -Wno-unused-parameter")

  add_definitions(-D_WIN32_WINNT=0x600)
  
find_package (Threads)
set (Boost_USE_STATIC_LIBS ON)


include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")


set(_PROTOBUF_LIBPROTOBUF protobuf)
set(_PROTOBUF_PROTOC protoc)

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

#protobuf_generate_cpp(PROTO_SRS PROTOHDRS chat.proto)
#protobuf_generate_python(PROTO_PYS chat.proto)

# Proto file
get_filename_component(hw_proto "./helloworld.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

get_filename_component(hsw_proto "./hellostreamingworld.proto" ABSOLUTE)
get_filename_component(hsw_proto_path "${hsw_proto}" PATH)

get_filename_component(mt_proto "./mathtest.proto" ABSOLUTE)
get_filename_component(mt_proto_path "${mt_proto}" PATH)

get_filename_component(mst_proto "./math_streaming.proto" ABSOLUTE)
get_filename_component(mst_proto_path "${mst_proto}" PATH)

get_filename_component(rg_proto "./route_guide.proto" ABSOLUTE)
get_filename_component(rg_proto_path "${rg_proto}" PATH)

get_filename_component(du_proto "./dummy.proto" ABSOLUTE)
get_filename_component(du_proto_path "${du_proto}" PATH)

get_filename_component(cd_proto "./chat_demo.proto" ABSOLUTE)
get_filename_component(cd_proto_path "${cd_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/helloworld.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/helloworld.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/helloworld.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/helloworld.grpc.pb.h")


set(hsw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.pb.cc")
set(hsw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.pb.h")
set(hsw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.grpc.pb.cc")
set(hsw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.grpc.pb.h")

set(mt_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/mathtest.pb.cc")
set(mt_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/mathtest.pb.h")
set(mt_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/mathtest.grpc.pb.cc")
set(mt_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/mathtest.grpc.pb.h")


set(mst_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/math_streaming.pb.cc")
set(mst_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/math_streaming.pb.h")
set(mst_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/math_streaming.grpc.pb.cc")
set(mst_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/math_streaming.grpc.pb.h")


set(rg_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/route_guide.pb.cc")
set(rg_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/route_guide.pb.h")
set(rg_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/route_guide.grpc.pb.cc")
set(rg_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/route_guide.grpc.pb.h")

set(du_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/dummy.pb.cc")
set(du_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/dummy.pb.h")
set(du_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/dummy.grpc.pb.cc")
set(du_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/dummy.grpc.pb.h")

set(cd_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/chat_demo.pb.cc")
set(cd_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/chat_demo.pb.h")
set(cd_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/chat_demo.grpc.pb.cc")
set(cd_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/chat_demo.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")


add_custom_command(
  OUTPUT "${hsw_proto_srcs}" "${hsw_proto_hdrs}" "${hsw_grpc_srcs}" "${hsw_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${hsw_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${hsw_proto}"
  DEPENDS "${hsw_proto}")

add_custom_command(
      OUTPUT "${mt_proto_srcs}" "${mt_proto_hdrs}" "${mt_grpc_srcs}" "${mt_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${mt_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${mt_proto}"
      DEPENDS "${mt_proto}")


add_custom_command(
  OUTPUT "${mst_proto_srcs}" "${mst_proto_hdrs}" "${mst_grpc_srcs}" "${mst_grpc_hdrs}"
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${mst_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${mst_proto}"
  DEPENDS "${mst_proto}")

add_custom_command(
        OUTPUT "${rg_proto_srcs}" "${rg_proto_hdrs}" "${rg_grpc_srcs}" "${rg_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${rg_proto_path}"
          --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
          "${rg_proto}"
        DEPENDS "${rg_proto}")

add_custom_command(
        OUTPUT "${du_proto_srcs}" "${du_proto_hdrs}" "${du_grpc_srcs}" "${du_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${du_proto_path}"
          --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
          "${du_proto}"
        DEPENDS "${du_proto}")

add_custom_command(
        OUTPUT "${cd_proto_srcs}" "${cd_proto_hdrs}" "${cd_grpc_srcs}" "${cd_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${cd_proto_path}"
          --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
          "${cd_proto}"
        DEPENDS "${cd_proto}")
      


# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Targets greeter_[async_](client|server)
foreach(_target
  greeter_client greeter_server greeter_async_client greeter_async_server )
  add_executable(${_target} "${_target}.cpp"
    ${hw_proto_srcs}
    ${hw_grpc_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()

foreach(_target
  greeter_streaming_server_async greeter_streaming_server_alarm_async greeter_streaming_server_queue_to_back greeter_streaming_server_queue_to_front greeter_streaming_client)
  add_executable(${_target} "${_target}.cpp"
    ${hsw_proto_srcs}
    ${hsw_grpc_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()

foreach(_target
  math_client math_server math_client_async math_server_async)
  add_executable(${_target} "${_target}.cpp"
    ${mt_proto_srcs}
    ${mt_grpc_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()

foreach(_target
  math_streaming_server_async math_streaming_client_async)
  add_executable(${_target} "${_target}.cpp"
    ${mst_proto_srcs}
    ${mst_grpc_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()

foreach(_target
route_guide_client route_guide_server route_guide_server_async route_guide_client_async)
  add_executable(${_target} "${_target}.cpp" helper.cpp route_guide_db.json
    ${rg_proto_srcs}
    ${rg_grpc_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()

foreach(_target
  dummyServer dummyClient )
  add_executable(${_target} "${_target}.cpp"
    ${du_proto_srcs}
    ${du_grpc_srcs}
    ${cd_proto_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()


foreach(_target
  chatDemoServer chatDemoClient chatDemoSimpleServer )
  add_executable(${_target} "${_target}.cpp"
    ${cd_proto_srcs}
    ${cd_grpc_srcs})
  target_link_libraries(${_target}
    ${_GRPC_GRPCPP_UNSECURE}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()