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

#include <framework/state.hpp>
#include <framework/task_group.hpp>
#include <framework/tcp_client.hpp>
#include <framework/tcp_service.hpp>
#include <framework/tcp_service_connection.hpp>
#include <framework/tcp_service_handlers.hpp>
#include <framework/tcp_service_session.hpp>

namespace framework {
async_of<void> single_connection(shared_state state, shared_tcp_service service, shared_of<tcp_executor> exec) {
  const auto _host = service->get_host();
  const auto _port = std::to_string(service->get_port());

  boost::asio::ip::tcp::resolver _resolver(exec->get_inner_executor());
  auto [_resolve_ec, _resolve_iterator] = co_await _resolver.async_resolve(_host, _port, boost::asio::as_tuple);
  if (_resolve_ec) throw boost::system::system_error(_resolve_ec);

  boost::asio::ip::tcp::socket _socket(exec->get_inner_executor());

  if (auto [_connection_ec, _connection_iterator] = co_await async_connect(_socket, _resolve_iterator, boost::asio::as_tuple);
      _connection_ec)
    throw boost::system::system_error(_connection_ec);

  auto _stream = std::make_shared<tcp_stream>(std::move(_socket));
  const uuid _session_id = state->generate_id();
  auto _connection = std::make_shared<tcp_service_connection>(_session_id, exec, _stream, service);
  service->add(_connection);

  if (service->handlers() && service->handlers()->on_connect()) {
    co_await service->handlers()->on_connect()(service, _connection);
  }
  if (service->handlers() && service->handlers()->on_accepted()) {
    co_await service->handlers()->on_accepted()(service, _connection);
  }

  co_await tcp_service_session(state, service, _connection);
  if (service->handlers() && service->handlers()->on_disconnected()) {
    try {
      co_await service->handlers()->on_disconnected()(service, _connection);
    } catch (...) {
    }
  }
  co_return;
}

async_of<void> tcp_client(task_group& task_group, shared_state state, shared_tcp_service service) {
  const auto _executor = co_await boost::asio::this_coro::executor;

  service->set_running(true);

  for (int _i = 0; _i < 4; ++_i) {
    auto _strand = std::make_shared<tcp_executor>(make_strand(_executor.get_inner_executor()));

    co_spawn(*_strand, single_connection(state, service, _strand), task_group.adapt([](const std::exception_ptr& exception) {
      if (exception) {
        try {
          std::rethrow_exception(exception);
        } catch (const std::exception& error) {
          std::cerr << "[tcp_client] connection error: " << error.what() << "\n";
        }
      }
    }));
  }

  co_return;
}
}  // namespace framework
