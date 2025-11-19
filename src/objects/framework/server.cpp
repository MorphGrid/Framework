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
      task_group_(std::make_shared<task_group>(state_->ioc().get_executor())) {}

void server::start(const unsigned short int port) {
  auto const _address = boost::asio::ip::make_address("0.0.0.0");

  const auto _router = state_->get_router();

  _router
      ->add(std::make_shared<route>(controllers::status_controller::verbs(),
                                    "/api/status",
                                    controllers::status_controller::make()))
      ->add(std::make_shared<route>(controllers::user_controller::verbs(),
                                    "/api/user",
                                    controllers::user_controller::make()))
      ->add(std::make_shared<route>(
          controllers::auth::attempt_controller::verbs(), "/api/auth/attempt",
          controllers::auth::attempt_controller::make()))
      ->add(std::make_shared<route>(
          controllers::queues::index_controller::verbs(), "/api/queues",
          controllers::queues::index_controller::make()))
      ->add(
          std::make_shared<route>(controllers::queues::jobs_controller::verbs(),
                                  "/api/queues/{queue_name}/jobs",
                                  controllers::queues::jobs_controller::make()))
      ->add(std::make_shared<route>(
          controllers::queues::tasks_controller::verbs(),
          "/api/queues/{queue_name}/tasks",
          controllers::queues::tasks_controller::make()))
      ->add(std::make_shared<route>(
          controllers::queues::workers_controller::verbs(),
          "/api/queues/{queue_name}/workers",
          controllers::queues::workers_controller::make()))
      ->add(std::make_shared<route>(
          controllers::queues::dispatch_controller::verbs(),
          "/api/queues/{queue_name}/dispatch",
          controllers::queues::dispatch_controller::make()));

  const auto _queue = state_->get_queue("metrics");
  _queue->add_task("increase_requests",
                   [this](auto& cancelled, auto& data) -> async_of<void> {
                     boost::ignore_unused(cancelled, data);
                     ++this->get_state()->get_metrics()->_requests;
                     co_return;
                   });

  co_spawn(make_strand(state_->ioc()),
           http_listener(*task_group_, state_, endpoint{_address, port}),
           task_group_->adapt([](const std::exception_ptr& throwable) noexcept {
             if (throwable) {
               try {
                 std::rethrow_exception(throwable);
               } catch (std::system_error& exception) {
                 std::cerr << "Error in listener: " << exception.what() << "\n";
               } catch (...) {
                 std::cerr << "Error in listener: \n";
               }
             }
           }));

  co_spawn(make_strand(state_->ioc()), signal_handler(*task_group_),
           boost::asio::detached);

  state_->get_connection_pool()->async_run(boost::asio::detached);

  state_->run();
}

shared_of<tcp_service> server::bind(const tcp_kind kind, std::string host,
                                    unsigned short int port,
                                    shared_of<tcp_handlers> callbacks) const {
  auto _service_id = state_->generate_id();
  const auto _service =
      std::make_shared<tcp_service>(_service_id, host, port, callbacks);
  state_->services().try_emplace(_service_id, _service);

  std::optional<async_of<void>> _handler;
  switch (kind) {
    case tcp_kind::SERVER:
      _handler.emplace(tcp_listener(*task_group_, state_, _service));
      break;
    case tcp_kind::CLIENT:
      _handler.emplace(tcp_client(*task_group_, state_, _service));
      break;
  }

  co_spawn(make_strand(state_->ioc()), std::move(_handler.value()),
           task_group_->adapt([](const std::exception_ptr& throwable) noexcept {
             if (throwable) {
               try {
                 std::rethrow_exception(throwable);
               } catch (const std::system_error& e) {
                 std::cerr << "[tcp_client] Boost error: " << e.what() << "\n";
               } catch (...) {
                 std::cerr << "[tcp_client] Unknown exception.\n";
               }
             }
           }));

  return _service;
}

shared_state server::get_state() const { return state_; }

shared_of<task_group> server::get_task_group() { return task_group_; }
}  // namespace framework
