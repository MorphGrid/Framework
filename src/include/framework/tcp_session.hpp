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

#pragma once

#ifndef FRAMEWORK_TCP_SESSION_HPP
#define FRAMEWORK_TCP_SESSION_HPP

#include <framework/errors/tcp/frame_too_large.hpp>
#include <framework/errors/tcp/on_read_error.hpp>
#include <framework/support.hpp>

namespace framework {
async_of<std::tuple<boost::system::error_code, std::size_t>> read_exactly(boost::asio::ip::tcp::socket &socket,
                                                                          boost::asio::streambuf &buffer, std::size_t n);

std::uint32_t read_uint32_from_buffer(boost::asio::streambuf &buffer);

template <class T, class S>
async_of<void> notify_error_and_close(const T service, const S connection, boost::asio::ip::tcp::socket &socket,
                                      const std::exception error) {
  if (service->handlers()->on_error()) co_await service->handlers()->on_error()(service, connection, error);
  if (service->handlers()->on_disconnected()) co_await service->handlers()->on_disconnected()(service, connection);
  service->remove(connection->get_id());
  boost::system::error_code _ec;
  socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
  socket.close(_ec);
  co_return;
}

template <class T, class S>
async_of<void> notify_disconnected_if_present(const T service, const S connection) {
  if (service->handlers()->on_disconnected()) co_await service->handlers()->on_disconnected()(service, connection);
  service->remove(connection->get_id());
  co_return;
}

template <class T, class S>
async_of<void> tcp_session(shared_state state, T service, S connection) {
  boost::ignore_unused(state);

  const auto _cancel_state = co_await boost::asio::this_coro::cancellation_state;
  if (service->handlers()->on_accepted()) co_await service->handlers()->on_accepted()(service, connection);

  auto &_socket = connection->get_stream()->socket();
  auto &_buffer = connection->get_buffer();

  while (!_cancel_state.cancelled()) {
    connection->get_stream()->expires_after(std::chrono::minutes(60));

    if (auto [_read_header_ec, _header_read_bytes] = co_await read_exactly(_socket, _buffer, HEADER_SIZE); _read_header_ec) {
      boost::ignore_unused(_header_read_bytes);
      co_await notify_disconnected_if_present(service, connection);
      co_return;
    }

    const std::uint32_t _payload_size = read_uint32_from_buffer(_buffer);
    if (_payload_size == 0) continue;
    if (_payload_size > MAX_FRAME_SIZE) {
      co_await notify_error_and_close(service, connection, _socket, errors::tcp::frame_too_large{});
      co_return;
    }

    if (auto [_read_payload_ec, _payload_read_bytes] = co_await read_exactly(_socket, _buffer, _payload_size); _read_payload_ec) {
      boost::ignore_unused(_payload_read_bytes);
      co_await notify_error_and_close(service, connection, _socket, errors::tcp::on_read_error{});
      co_return;
    }

    if (static_cast<bool>(_cancel_state.cancelled())) {
      co_await notify_disconnected_if_present(service, connection);
      co_return;
    }

    std::string _payload;
    _payload.resize(_payload_size);
    {
      std::istream _input_stream(&_buffer);
      _input_stream.read(_payload.data(), _payload_size);
    }

    if (service->handlers()->on_read()) co_await service->handlers()->on_read()(service, connection, std::move(_payload));
  }

  co_await notify_disconnected_if_present(service, connection);

  if (!connection->get_stream()->socket().is_open()) co_return;
  _socket.shutdown(boost::asio::socket_base::shutdown_send);
}
}  // namespace framework

#endif  // FRAMEWORK_TCP_SESSION_HPP
