#include <userver/utest/using_namespace_userver.hpp>

#include <chrono>
#include <string_view>
#include <sstream>
#include <deque>
#include <utility>
#include <vector>

#include <userver/components/component.hpp>
#include <userver/components/loggable_component_base.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/single_consumer_event.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/engine/task/task_with_result.hpp>
//#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utils/algo.hpp>
#include <userver/components/run.hpp>
//#include <userver/utils/daemon_run.hpp>

#include <userver/yaml_config/merge_schemas.hpp>

#include <userver/ugrpc/client/client_factory_component.hpp>
#include <userver/ugrpc/server/server_component.hpp>
#include <userver/ugrpc/server/service_component_base.hpp>

#include <userver/logging/component.hpp>
#include <userver/os_signals/component.hpp>
#include <userver/components/tracer.hpp>
#include <userver/components/statistics_storage.hpp>

//#include <samples/greeter_client.usrv.pb.hpp>
#include <greeter_service.usrv.pb.hpp>

namespace samples {

/// [gRPC sample - service]
class GreeterServiceComponent final
    : public api::GreeterServiceBase::Component {
 public:
  static constexpr std::string_view kName = "greeter-service";

  GreeterServiceComponent(const components::ComponentConfig& config,
                          const components::ComponentContext& context)
      : api::GreeterServiceBase::Component(config, context),
        prefix_(config["greeting-prefix"].As<std::string>()) {
        if (1) {
	auto& comp = context.FindComponent<ugrpc::server::ServerComponent>();
        comp.GetServer().WithServerBuilder([](grpc::ServerBuilder& builder){
	  builder.SetSyncServerOption(grpc::ServerBuilder::NUM_CQS, 16);
	});
	}
      }

  void SayHello(SayHelloCall& call, api::GreetingRequest&& request) override;

  static yaml_config::Schema GetStaticConfigSchema();

 private:
  const std::string prefix_;
};
/// [gRPC sample - service]


void fake_call() {
  struct timespec tim, tim2;
  tim.tv_sec = 0;
  tim.tv_nsec = 500000; // 0.0005 sec
  nanosleep(&tim , &tim2);
}


/// [gRPC sample - server RPC handling]
void GreeterServiceComponent::SayHello(
    api::GreeterServiceBase::SayHelloCall& call,
    api::GreetingRequest&& request) {
  // Authentication checking could have gone here. For this example, we trust
  // the world.

  api::GreetingResponse response;
  response.set_greeting(prefix_ + " " + request.name());

  //fake_call();
  {
    //std::ostringstream ostr;
    //ostr << "thread-id = " << std::this_thread::get_id() << std::endl;
    //std::cout << ostr.str();
  }

  // Complete the RPC by sending the response. The service should complete
  // each request by calling `Finish` or `FinishWithError`, otherwise the
  // client will receive an Internal Error (500) response.
  call.Finish(response);
}
/// [gRPC sample - server RPC handling]

yaml_config::Schema GreeterServiceComponent::GetStaticConfigSchema() {
  return yaml_config::MergeSchemas<ugrpc::server::ServiceComponentBase>(R"(
type: object
description: gRPC sample greater service component
additionalProperties: false
properties:
    greeting-prefix:
        type: string
        description: greeting prefix
)");
}


}  // namespace samples

/// [gRPC sample - main]
int main(int argc, char* argv[]) {
  const auto component_list =
      /// [gRPC sample - ugrpc registration]
      components::ComponentList()//MinimalServerComponentList()
          .Append<userver::os_signals::ProcessorComponent>()
          .Append<userver::components::Logging>()
	  .Append<userver::components::Tracer>() // used in StatisticsStorage
          .Append<userver::components::StatisticsStorage>()
	  .Append<ugrpc::server::ServerComponent>()
          .Append<samples::GreeterServiceComponent>()
          ;
  // config_dev.yaml
  userver::components::InMemoryConfig config(R"(
# yaml
components_manager:
    components:
        # The required common components
        logging:
            fs-task-processor: fs-task-processor
            loggers:
                default:
                    file_path: '@stderr'
                    level: warning
                    overflow_behavior: discard
        tracer:
            service-name: grpc-service'

# /// [gRPC sample - static config client]
# yaml

# /// [gRPC sample - static config client]

# /// [gRPC sample - static config server]
# yaml
        # Common configuration for gRPC server
        grpc-server:
            # The single listening port for incoming RPCs
            port: 50051

        # Our GreeterService implementation
        greeter-service:
            task-processor: main-task-processor
            greeting-prefix: Hello
# /// [gRPC sample - static config server]

    default_task_processor: main-task-processor
# /// [gRPC sample - task processor]
# yaml
    task_processors:
        grpc-blocking-task-processor:  # For blocking gRPC channel creation
            thread_name: grpc-worker
            worker_threads: 22
# /// [gRPC sample - task processor]
        main-task-processor:           # For non-blocking operations
            thread_name: main-worker
            worker_threads: 24
        fs-task-processor:             # For blocking filesystem operations
            thread_name: fs-worker
            worker_threads: 22
    coro_pool:
        initial_size: 500
        max_size: 10000
  )");
  userver::components::Run(
    config,
    component_list,
    {}, // init_log_path
    userver::logging::Format::kTskv);

  return 0;
  
  //return utils::DaemonMain(argc, argv, component_list);
}
/// [gRPC service sample - main]
