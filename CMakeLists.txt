cmake_minimum_required(VERSION 3.20)

project(userver-samples CXX)

set (CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")


find_package(Threads REQUIRED)
find_package(protobuf REQUIRED COMPONENTS libprotobuf)
find_package(gRPC REQUIRED COMPONENTS grpc++ grpc++_reflection)
#find_package(Grpc REQUIRED)
#find_package(Protobuf REQUIRED)


set(USERVER_NAMESPACE "userver" CACHE STRING "C++ namespace to use")
if (NOT "${USERVER_NAMESPACE}" STREQUAL "")
    set(USERVER_NAMESPACE_BEGIN "namespace ${USERVER_NAMESPACE} {" CACHE STRING "Open C++ namespace to use")
    set(USERVER_NAMESPACE_END "}" CACHE STRING "Close C++ namespace to use")
endif()

include(GrpcTargets)
add_grpc_library(greeter_proto PROTOS greeter.proto)


target_compile_definitions(greeter_proto PUBLIC
  "USERVER_NAMESPACE=${USERVER_NAMESPACE}"
  "USERVER_NAMESPACE_BEGIN=${USERVER_NAMESPACE_BEGIN}"
  "USERVER_NAMESPACE_END=${USERVER_NAMESPACE_END}"
)

add_subdirectory(grpc_server)
add_subdirectory(grpc_client)

