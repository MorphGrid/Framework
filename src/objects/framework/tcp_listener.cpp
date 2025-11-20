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
#include <framework/errors/session_error.hpp>
#include <framework/state.hpp>
#include <framework/task_group.hpp>
#include <framework/tcp_connection.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_listener.hpp>
#include <framework/tcp_service.hpp>
#include <framework/tcp_session.hpp>

namespace framework {
async_of<void> tcp_listener(task_group &task_group, const shared_state state,
                            shared_of<tcp_service> service) {
  LOG("entry for service_id=" << service->get_id());

  const auto _cancellation_state =
      co_await boost::asio::this_coro::cancellation_state;
  const auto _executor = co_await boost::asio::this_coro::executor;
  LOG("executor obtained for service_id=" << service->get_id());

  auto _endpoint =
      endpoint{boost::asio::ip::make_address("0.0.0.0"), service->get_port()};
  LOG("binding to endpoint " << _endpoint.address().to_string() << ":"
                             << _endpoint.port()
                             << " for service_id=" << service->get_id());

  auto _acceptor = acceptor{_executor, _endpoint};
  LOG("acceptor created, local_port(before set)="
      << _acceptor.local_endpoint().port()
      << " for service_id=" << service->get_id());

  co_await boost::asio::this_coro::reset_cancellation_state(
      boost::asio::enable_total_cancellation());
  LOG("cancellation state reset for service_id=" << service->get_id());

  service->set_port(_acceptor.local_endpoint().port());
  service->set_running(true);
  LOG("service set running=true, port="
      << service->get_port() << ", service_id=" << service->get_id());

  while (!_cancellation_state.cancelled()) {
    LOG("waiting for new connection (service_id=" << service->get_id() << ")");

    auto _socket_executor = std::make_shared<tcp_executor>(
        make_strand(_executor.get_inner_executor()));
    LOG("created socket executor for new accept, service_id="
        << service->get_id());

    auto [_ec, _socket] = co_await _acceptor.async_accept(
        *_socket_executor, boost::asio::as_tuple);

    LOG("accept returned for service_id=" << service->get_id()
                                          << ", error_value=" << _ec.value()
                                          << ", msg=" << _ec.message());

    // capture some socket metadata before moving it into stream
    const auto _native_handle = _socket.native_handle();
    std::uint16_t _remote_port = 0;
    std::string _remote_addr = "<unknown>";
    try {
      if (_socket.is_open()) {
        _remote_addr = _socket.remote_endpoint().address().to_string();
        _remote_port = _socket.remote_endpoint().port();
      }
    } catch (...) {
      // avoid changing behaviour on remote_endpoint() exceptions; just leave
      // unknown
    }
    LOG("socket metadata: native_handle="
        << _native_handle << ", remote=" << _remote_addr << ":" << _remote_port
        << ", service_id=" << service->get_id());

    auto _session_id = state->generate_id();
    LOG("generated session_id=" << _session_id
                                << " for service_id=" << service->get_id());

    auto _stream = std::make_shared<tcp_stream>(std::move(_socket));
    LOG("tcp_stream created for session_id="
        << _session_id << ", stream_socket_native_handle="
        << _stream->socket().native_handle());

    auto _connection = std::make_shared<tcp_connection>(
        _session_id, _socket_executor, _stream, service);
    LOG("tcp_connection constructed connection_id="
        << _connection->get_id() << ", session_id=" << _session_id
        << ", service_id=" << service->get_id());

    service->add(_connection);
    LOG("connection added to service (connection_id="
        << _connection->get_id() << ", service_id=" << service->get_id()
        << ")");

    if (service->handlers()->on_connect()) {
      LOG("invoking on_connect handler for connection_id="
          << _connection->get_id() << ", service_id=" << service->get_id());
      co_await service->handlers()->on_connect()(service, _connection);
      LOG("on_connect handler completed for connection_id="
          << _connection->get_id());
    }

    if (_ec == boost::asio::error::operation_aborted) {
      LOG("accept aborted (operation_aborted) for service_id="
          << service->get_id());
      service->set_running(false);
      co_return;
    }

    if (_ec) {
      LOG("accept returned error, throwing system_error for service_id="
          << service->get_id() << ", error_value=" << _ec.value()
          << ", msg=" << _ec.message());
      throw boost::system::system_error{_ec};
    }

    LOG("spawning tcp_session for connection_id="
        << _connection->get_id() << ", session_id=" << _session_id
        << ", service_id=" << service->get_id());

    co_spawn(
        *_socket_executor, tcp_session(state, service, _connection),
        task_group.adapt([](const std::exception_ptr &throwable) noexcept {
          if (throwable) {
            try {
              std::rethrow_exception(throwable);
            } catch (const system_error &exception) {
              LOG("Boost system_error caught in spawned task: "
                  << exception.what());
              std::cerr << "Boost error: " << exception.what() << std::endl;
            } catch (const std::exception &e) {
              LOG("std::exception caught in spawned task: " << e.what());
              std::cerr << "Unknown exception thrown: " << e.what()
                        << std::endl;
            } catch (...) {
              LOG("Unknown non-standard exception caught in spawned task.");
              std::cerr << "Unknown exception thrown." << std::endl;
            }
          } else {
            LOG("spawned tcp_session completed without exception.");
          }
        }));

    LOG("spawn requested for connection_id="
        << _connection->get_id() << ", session_id=" << _session_id
        << ", service_id=" << service->get_id());
  }

  LOG("cancellation detected in listener loop, exiting for service_id="
      << service->get_id());
  co_return;
}
}  // namespace framework
