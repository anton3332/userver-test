project(grpc_server CXX)

add_executable(${PROJECT_NAME} grpc_server.cpp)
target_link_libraries(${PROJECT_NAME} userver-core)
target_link_libraries(${PROJECT_NAME} greeter_proto)

