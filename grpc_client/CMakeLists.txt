project(grpc_client CXX)

add_executable(${PROJECT_NAME} grpc_client.cpp)
target_link_libraries(${PROJECT_NAME} userver-core)
target_link_libraries(${PROJECT_NAME} greeter_proto)

