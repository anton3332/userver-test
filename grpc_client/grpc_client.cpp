#include <userver/utest/using_namespace_userver.hpp>

#include <atomic>
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

#include <greeter_client.usrv.pb.hpp>

namespace samples {

/// [gRPC sample - client]
// A user-defined wrapper around api::GreeterServiceClient that provides
// a simplified interface.
class GreeterClient final : public components::LoggableComponentBase {
 public:
  static constexpr std::string_view kName = "greeter-client";

  GreeterClient(const components::ComponentConfig& config,
                const components::ComponentContext& context)
      : LoggableComponentBase(config, context),
        // ClientFactory is used to create gRPC clients
        client_factory_(
            context.FindComponent<ugrpc::client::ClientFactoryComponent>()
                .GetFactory()),
        endpoint_(config["endpoint"].As<std::string>()) {
	 //The client needs a fixed endpointclient_factory_.MakeClient<api::GreeterServiceClient>(endpoint_)
        //client_(client_factory_.MakeClient<api::GreeterServiceClient>(endpoint_)) {
	  std::cout << "initializing GRPC clients..." << std::endl;
          for (std::size_t i = 0; i < 128; ++i)
            clients_.emplace_back(client_factory_.MakeClient<api::GreeterServiceClient>(endpoint_));
	}

  std::string SayHello(std::string name);

  static yaml_config::Schema GetStaticConfigSchema();

  void OnAllComponentsLoaded() final
  {
    auto g = SayHello("userver");
    std::cout << g << std::endl;
  }


 private:
  ugrpc::client::ClientFactory& client_factory_;
  std::string endpoint_;
  std::deque<api::GreeterServiceClient> clients_;
  //std::vector<api::GreeterServiceClient> clients_;
};
/// [gRPC sample - client]

/// [gRPC sample - client RPC handling]
std::string GreeterClient::SayHello(std::string name) {
  using namespace std::chrono_literals;
  // client_;// MakeClient<sample::ugrpc::UnitTestServiceClient>();
  engine::SingleConsumerEvent request_finished;
  std::vector<engine::TaskWithResult<void>> tasks;
  std::atomic<unsigned int> count_made{0};
  std::atomic<bool> keep_running{true};

  {
    //std::ostringstream ostr;
    //ostr << "grpc-client-thread-id = " << std::this_thread::get_id() << std::endl;
    //std::cout << ostr.str();
  }

  for (std::size_t i = 0; i < clients_.size(); ++i) {
    tasks.push_back(engine::AsyncNoSpan([&request_finished, &keep_running, &count_made, i, this] {
      //auto client = factory_.MakeClient<api::GreeterServiceClient>(endpoint_);
      //auto client = client_factory_.MakeClient<api::GreeterServiceClient>(endpoint_);
      //try {
      api::GreetingRequest out;
      out.set_name("userver");

      while (keep_running) {
        // Deadline must be set manually for each RPC
        auto context = std::make_unique<grpc::ClientContext>();
        context->set_deadline(
            engine::Deadline::FromDuration(20s));
        auto call = clients_[i].SayHello(out, std::move(context));
        auto in = call.Finish();
	//CheckClientContext(call.GetContext());
        //if ("Hello " + out.name() != in.name())
        //  throw std::exception("wrong grpc server response");
        request_finished.Send();
        engine::Yield();
	++count_made;
      }
      //} catch (const std::exception& ex) {
	//std::cout << "exception in loop: " << ex.what() << std::endl;
      //} catch (...) {
      	//std::cout << "unknown exception in loop" << std::endl;
      //}
    }));
  }

  //if (false && !request_finished.WaitForEventFor(utest::kMaxTestWaitTime))
  //    throw std::exception("wrong wait");

  // Make sure that multi-threaded requests work fine for some time
  int seconds_total = 10;
  int seconds_step = 10;
  for (int i = 0; i < seconds_total; i += seconds_step) {
    std::cout << i << " seconds" << std::endl;
    engine::SleepFor(1s * seconds_step);
  }
  keep_running = false;

  for (auto& task : tasks) task.Get();

  return std::to_string(count_made.load() / seconds_total) + " QPS\n";
}
/// [gRPC sample - client RPC handling]

yaml_config::Schema GreeterClient::GetStaticConfigSchema() {
  return yaml_config::MergeSchemas<components::LoggableComponentBase>(R"(
type: object
description: >
    a user-defined wrapper around api::GreeterServiceClient that provides
    a simplified interface.
additionalProperties: false
properties:
    endpoint:
        type: string
        description: >
            the service endpoint (URI). We talk to our own service,
            which is kind of pointless, but works for an example
)");
}


}  // namespace samples

/// [gRPC sample - main]
int main(int argc, char* argv[]) {
  const auto component_list =
      /// [gRPC sample - ugrpc registration]
      components::MinimalServerComponentList()
          .Append<ugrpc::client::ClientFactoryComponent>()
          .Append<samples::GreeterClient>();
  
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
            service-name: grpc-service
        dynamic-config:
            fs-cache-path: ''
        dynamic-config-fallbacks:
            fallback-path: /home/anton_kulikov/work/userver/userver/samples/grpc_service/dynamic_config_fallback.json

# /// [gRPC sample - static config client]
# yaml
        # Creates gRPC clients
        grpc-client-factory:
            # The TaskProcessor for blocking connection initiation
            task-processor: grpc-blocking-task-processor

            # Optional channel parameters for gRPC Core
            # https://grpc.github.io/grpc/core/group__grpc__arg__keys.html
            channel-args:
                "grpc.use_local_subchannel_pool": 1
            channel-count: 64

        # Our wrapper around the generated client for GreeterService
        greeter-client:
            # The service endpoint (URI). We talk to our own service,
            # which is kind of pointless, but works for an example
            endpoint: '[::1]:50051'
# /// [gRPC sample - static config client]

# /// [gRPC sample - static config server]

        # In this example, the tests still communicate with the microservice
        # using HTTP, so we still need an HTTP server and an HTTP handler.
        server:
            listener:
                port: 50080
                task_processor: main-task-processor
        auth-checker-settings:

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
