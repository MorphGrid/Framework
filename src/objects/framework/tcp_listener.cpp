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

#include <boost/asio/co_spawn.hpp>
#include <framework/auth.hpp>
#include <framework/errors/session_error.hpp>
#include <framework/state.hpp>
#include <framework/task_group.hpp>
#include <framework/tcp_connection.hpp>
#include <framework/tcp_endpoint.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_listener.hpp>
#include <framework/tcp_session.hpp>

namespace framework {
async_of<void> tcp_listener(task_group &task_group, const shared_state state, shared_of<tcp_endpoint> service) {
  auto _cancellation_state = co_await boost::asio::this_coro::cancellation_state;
  const auto _executor = co_await boost::asio::this_coro::executor;
  auto _endpoint = endpoint{boost::asio::ip::make_address("0.0.0.0"), service->get_port()};
  auto _acceptor = acceptor{_executor, _endpoint};

  co_await boost::asio::this_coro::reset_cancellation_state(boost::asio::enable_total_cancellation());

  service->set_port(_acceptor.local_endpoint().port());
  service->set_running(true);

  while (!_cancellation_state.cancelled()) {
    auto _socket_executor = std::make_shared<tcp_executor>(make_strand(_executor.get_inner_executor()));
    auto [_ec, _socket] = co_await _acceptor.async_accept(*_socket_executor, boost::asio::as_tuple);

    auto _session_id = state->generate_id();
    auto _stream = std::make_shared<tcp_stream>(std::move(_socket));
    auto _connection = std::make_shared<tcp_connection<tcp_endpoint>>(_session_id, _socket_executor, _stream, service);
    service->add(_connection);

    if (service->handlers()->on_connect()) co_await service->handlers()->on_connect()(service, _connection);

    if (_ec == boost::asio::error::operation_aborted) {
      service->set_running(false);
      co_return;
    }

    if (_ec) throw boost::system::system_error{_ec};

    co_spawn(*_socket_executor, tcp_session<shared_of<tcp_endpoint>, shared_of<tcp_connection<tcp_endpoint>>>(state, service, _connection),
             task_group.adapt([](const std::exception_ptr &throwable) noexcept {
               if (throwable) {
                 try {
                   std::rethrow_exception(throwable);
                 } catch (const system_error &exception) {
                   std::cerr << "[tcp_listener] Boost error: " << exception.what() << std::endl;
                 } catch (...) {
                   std::cerr << "[tcp_listener] Unknown exception thrown." << std::endl;
                 }
               }
             }));
  }

  co_return;
}
}  // namespace framework
