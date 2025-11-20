// Copyright (C) 2025 Ian Torres <iantorres@outlook.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <framework/controllers/auth/attempt_controller.hpp>
#include <framework/controllers/queues/dispatch_controller.hpp>
#include <framework/controllers/queues/index_controller.hpp>
#include <framework/controllers/queues/jobs_controller.hpp>
#include <framework/controllers/queues/tasks_controller.hpp>
#include <framework/controllers/queues/workers_controller.hpp>
#include <framework/controllers/status_controller.hpp>
#include <framework/controllers/user_controller.hpp>
#include <framework/http_listener.hpp>
#include <framework/metrics.hpp>
#include <framework/queue.hpp>
#include <framework/route.hpp>
#include <framework/router.hpp>
#include <framework/server.hpp>
#include <framework/signal_handler.hpp>
#include <framework/state.hpp>
#include <framework/task_group.hpp>
#include <framework/tcp_client.hpp>
#include <framework/tcp_kind.hpp>
#include <framework/tcp_listener.hpp>
#include <framework/tcp_service.hpp>

namespace framework {
server::server()
    : state_(std::make_shared<state>()),
      task_group_(std::make_shared<task_group>(state_->ioc().get_executor())) {
  LOG("constructed server object");
  LOG("ioc executor addr=" << &state_->ioc());
}

void server::start(const unsigned short int port) {
  LOG("entry port=" << port);
  auto const _address = boost::asio::ip::make_address("0.0.0.0");
  LOG("address resolved to " << _address.to_string());

  const auto _router = state_->get_router();
  LOG("obtained router");

  LOG("adding routes...");
  _router->add(std::make_shared<route>(controllers::status_controller::verbs(),
                                       "/api/status",
                                       controllers::status_controller::make()));
  LOG("added /api/status");

  _router->add(std::make_shared<route>(controllers::user_controller::verbs(),
                                       "/api/user",
                                       controllers::user_controller::make()));
  LOG("added /api/user");

  _router->add(std::make_shared<route>(
      controllers::auth::attempt_controller::verbs(), "/api/auth/attempt",
      controllers::auth::attempt_controller::make()));
  LOG("added /api/auth/attempt");

  _router->add(std::make_shared<route>(
      controllers::queues::index_controller::verbs(), "/api/queues",
      controllers::queues::index_controller::make()));
  LOG("added /api/queues");

  _router->add(
      std::make_shared<route>(controllers::queues::jobs_controller::verbs(),
                              "/api/queues/{queue_name}/jobs",
                              controllers::queues::jobs_controller::make()));
  LOG("added /api/queues/{queue_name}/jobs");

  _router->add(
      std::make_shared<route>(controllers::queues::tasks_controller::verbs(),
                              "/api/queues/{queue_name}/tasks",
                              controllers::queues::tasks_controller::make()));
  LOG("added /api/queues/{queue_name}/tasks");

  _router->add(
      std::make_shared<route>(controllers::queues::workers_controller::verbs(),
                              "/api/queues/{queue_name}/workers",
                              controllers::queues::workers_controller::make()));
  LOG("added /api/queues/{queue_name}/workers");

  _router->add(std::make_shared<route>(
      controllers::queues::dispatch_controller::verbs(),
      "/api/queues/{queue_name}/dispatch",
      controllers::queues::dispatch_controller::make()));
  LOG("added /api/queues/{queue_name}/dispatch");

  const auto _queue = state_->get_queue("metrics");
  LOG("obtained metrics queue at "
      << reinterpret_cast<const void*>(_queue.get()));
  _queue->add_task("increase_requests",
                   [this](auto& cancelled, auto& data) -> async_of<void> {
                     LOG("[metrics task] increase_requests invoked");
                     boost::ignore_unused(cancelled, data);
                     ++this->get_state()->get_metrics()->_requests;
                     LOG("[metrics task] requests incremented to "
                         << this->get_state()->get_metrics()->_requests);
                     co_return;
                   });
  LOG("metrics task registered");

  LOG("spawning http_listener coroutine");
  co_spawn(make_strand(state_->ioc()),
           http_listener(*task_group_, state_, endpoint{_address, port}),
           task_group_->adapt([](const std::exception_ptr& throwable) noexcept {
             if (throwable) {
               try {
                 std::rethrow_exception(throwable);
               } catch (std::system_error& exception) {
                 LOG("Error in listener: " << exception.what());
               } catch (...) {
                 LOG("Error in listener: unknown");
               }
             } else {
               LOG("listener coroutine finished without exception");
             }
           }));

  LOG("spawning signal_handler coroutine");
  co_spawn(make_strand(state_->ioc()), signal_handler(*task_group_),
           boost::asio::detached);
  LOG("signal_handler spawned");

  LOG("starting connection pool async_run");
  state_->get_connection_pool()->async_run(boost::asio::detached);

  LOG("running state_->run()");
  state_->run();

  LOG("exit");
}

shared_of<tcp_service> server::bind(const tcp_kind kind, std::string host,
                                    unsigned short int port,
                                    shared_of<tcp_handlers> callbacks,
                                    unsigned short int connections) const {
  LOG("entry kind=" << static_cast<int>(kind) << " host=" << host
                    << " port=" << port << " connections=" << connections);

  auto _service_id = state_->generate_id();
  LOG("generated service_id=" << _service_id);

  const auto _service =
      std::make_shared<tcp_service>(_service_id, host, port, callbacks);

  LOG("created tcp_service object at "
      << reinterpret_cast<const void*>(_service.get()));

  _service->scale_to(connections);
  LOG("scaled service to connections=" << connections);

  state_->services().try_emplace(_service_id, _service);
  LOG("emplaced service into state_->services()");

  const auto _service_task_group =
      std::make_shared<task_group>(state_->ioc().get_executor());
  LOG("created service_task_group at "
      << reinterpret_cast<const void*>(_service_task_group.get()));

  _service->set_task_group(_service_task_group);
  LOG("set task_group on service");

  std::optional<async_of<void>> _handler;
  switch (kind) {
    case tcp_kind::SERVER:
      LOG("kind SERVER selected, creating tcp_listener handler");
      _handler.emplace(tcp_listener(*_service_task_group, state_, _service));
      break;
    case tcp_kind::CLIENT:
      LOG("kind CLIENT selected, creating tcp_client handler");
      _handler.emplace(tcp_client(*_service_task_group, state_, _service));
      break;
    default:
      LOG("unknown tcp_kind");
      break;
  }

  LOG("spawning handler coroutine with service_task_group");
  co_spawn(make_strand(state_->ioc()), std::move(_handler.value()),
           _service_task_group->adapt(
               [](const std::exception_ptr& throwable) noexcept {
                 if (throwable) {
                   try {
                     std::rethrow_exception(throwable);
                   } catch (const std::system_error& e) {
                     LOG("Boost error: " << e.what());
                   } catch (const std::exception& e) {
                     LOG("exception: " << e.what());
                   } catch (...) {
                     LOG("Unknown exception.");
                   }
                 } else {
                   LOG("handler coroutine finished without exception");
                 }
               }));

  LOG("returning service id=" << _service_id);
  return _service;
}

shared_state server::get_state() const {
  LOG("called");
  return state_;
}

shared_of<task_group> server::get_task_group() {
  LOG("called");
  return task_group_;
}
}  // namespace framework
