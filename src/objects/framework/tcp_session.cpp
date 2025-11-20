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

#include <framework/errors/tcp/frame_too_large.hpp>
#include <framework/errors/tcp/on_read_error.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_session.hpp>

namespace framework {
async_of<std::tuple<boost::system::error_code, std::size_t>> read_exactly(
    boost::asio::ip::tcp::socket &socket, boost::asio::streambuf &buffer,
    const std::size_t n) {
  LOG("enter requested_bytes=" << n << ", socket_native_handle="
                               << socket.native_handle());
  auto _result = co_await async_read(
      socket, buffer, boost::asio::transfer_exactly(n), boost::asio::as_tuple);
  const auto &_ec = std::get<0>(_result);
  const auto &_bytes = std::get<1>(_result);
  LOG("exit bytes_transferred=" << _bytes << ", error_value=" << _ec.value()
                                << ", message=" << _ec.message());
  co_return _result;
}

std::uint32_t read_uint32_from_buffer(boost::asio::streambuf &buffer) {
  LOG("enter buffer_size=" << buffer.size());
  static_assert(HEADER_SIZE == 4, "HEADER_SIZE must be 4");
  std::istream _input_stream(&buffer);
  std::array<unsigned char, HEADER_SIZE> _header{};
  _input_stream.read(reinterpret_cast<char *>(_header.data()), HEADER_SIZE);
  const uint32_t _value = static_cast<std::uint32_t>(_header[0]) << 24 |
                          static_cast<std::uint32_t>(_header[1]) << 16 |
                          static_cast<std::uint32_t>(_header[2]) << 8 |
                          static_cast<std::uint32_t>(_header[3]);
  LOG("exit parsed=" << _value << ", header_bytes=" << HEADER_SIZE
                     << ", remaining_buffer=" << buffer.size());
  return _value;
}

async_of<void> notify_error_and_close(
    const shared_of<tcp_service> service,
    const shared_of<tcp_connection> connection,
    boost::asio::ip::tcp::socket &socket, const std::exception error) {
  LOG("enter connection_id=" << connection->get_id());

  if (service->handlers()->on_error()) {
    LOG("invoking on_error for connection_id=" << connection->get_id());
    co_await service->handlers()->on_error()(service, connection, error);
  }
  if (service->handlers()->on_disconnected()) {
    LOG("invoking on_disconnected for connection_id=" << connection->get_id());
    co_await service->handlers()->on_disconnected()(service, connection);
  }

  LOG("removing connection_id=" << connection->get_id());
  service->remove(connection->get_id());

  boost::system::error_code _ec;
  LOG("shutting down socket for connection_id=" << connection->get_id());
  socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
  LOG("shutdown result=" << _ec.value() << ", msg=" << _ec.message());
  socket.close(_ec);
  LOG("close result=" << _ec.value() << ", msg=" << _ec.message());

  LOG("exit connection_id=" << connection->get_id());
  co_return;
}

async_of<void> notify_disconnected_if_present(
    const shared_of<tcp_service> service,
    const shared_of<tcp_connection> connection) {
  LOG("enter connection_id=" << connection->get_id());
  if (service->handlers()->on_disconnected()) {
    LOG("invoking on_disconnected for "
        "connection_id="
        << connection->get_id());
    co_await service->handlers()->on_disconnected()(service, connection);
  }
  LOG("removing connection_id=" << connection->get_id());
  service->remove(connection->get_id());
  LOG("exit connection_id=" << connection->get_id());
  co_return;
}

async_of<void> tcp_session(shared_state state, shared_of<tcp_service> service,
                           shared_of<tcp_connection> connection) {
  LOG("enter connection_id=" << connection->get_id());
  boost::ignore_unused(state);

  const auto _cancel_state =
      co_await boost::asio::this_coro::cancellation_state;
  if (service->handlers()->on_accepted()) {
    LOG("calling on_accepted for connection_id=" << connection->get_id());
    co_await service->handlers()->on_accepted()(service, connection);
  }

  auto &_socket = connection->get_stream()->socket();
  auto &_buffer = connection->get_buffer();

  while (!_cancel_state.cancelled()) {
    LOG("loop start connection_id=" << connection->get_id()
                                    << ", buffer_size=" << _buffer.size());

    connection->get_stream()->expires_after(std::chrono::minutes(60));
    LOG("timeout set (60m) for connection_id=" << connection->get_id());

    LOG("reading header (" << HEADER_SIZE
                           << ") for connection_id=" << connection->get_id());
    auto [_read_header_ec, _header_read_bytes] =
        co_await read_exactly(_socket, _buffer, HEADER_SIZE);
    if (_read_header_ec) {
      LOG("header read error connection_id="
          << connection->get_id() << ", err_val=" << _read_header_ec.value()
          << ", msg=" << _read_header_ec.message());
      boost::ignore_unused(_header_read_bytes);
      co_await notify_disconnected_if_present(service, connection);
      co_return;
    }

    LOG("header read success bytes=" << _header_read_bytes << " connection_id="
                                     << connection->get_id());
    const std::uint32_t _payload_size = read_uint32_from_buffer(_buffer);
    LOG("payload_size parsed=" << _payload_size
                               << " connection_id=" << connection->get_id());

    if (_payload_size == 0) {
      LOG("payload_size is 0, continuing loop for connection_id="
          << connection->get_id());
      continue;
    }

    if (_payload_size > MAX_FRAME_SIZE) {
      LOG("payload too large connection_id="
          << connection->get_id() << ", payload_size=" << _payload_size
          << ", max=" << MAX_FRAME_SIZE);
      co_await notify_error_and_close(service, connection, _socket,
                                      errors::tcp::frame_too_large{});
      co_return;
    }

    LOG("reading payload of size "
        << _payload_size << " for connection_id=" << connection->get_id());
    auto [_read_payload_ec, _payload_read_bytes] =
        co_await read_exactly(_socket, _buffer, _payload_size);
    if (_read_payload_ec) {
      LOG("failed to read payload connection_id="
          << connection->get_id() << ", err_val=" << _read_payload_ec.value()
          << ", msg=" << _read_payload_ec.message());
      boost::ignore_unused(_payload_read_bytes);
      co_await notify_error_and_close(service, connection, _socket,
                                      errors::tcp::on_read_error{});
      co_return;
    }

    LOG("payload read success bytes="
        << _payload_read_bytes << " connection_id=" << connection->get_id());

    if (static_cast<bool>(_cancel_state.cancelled())) {
      LOG("cancellation detected for connection_id=" << connection->get_id());
      co_await notify_disconnected_if_present(service, connection);
      co_return;
    }

    LOG("extracting payload into string for connection_id="
        << connection->get_id() << ", size=" << _payload_size);
    std::string _payload;
    _payload.resize(_payload_size);
    {
      std::istream _input_stream(&_buffer);
      _input_stream.read(_payload.data(), _payload_size);
    }

    LOG("dispatching on_read for connection_id="
        << connection->get_id() << ", payload_size=" << _payload_size);
    if (service->handlers()->on_read())
      co_await service->handlers()->on_read()(service, connection,
                                              std::move(_payload));

    LOG("loop end for connection_id=" << connection->get_id());
  }

  LOG("cancellation reached, notifying disconnect for "
      "connection_id="
      << connection->get_id());
  co_await notify_disconnected_if_present(service, connection);

  if (!connection->get_stream()->socket().is_open()) {
    LOG("socket already closed for connection_id=" << connection->get_id()
                                                   << ", exit");
    co_return;
  }

  LOG("shutting down socket send channel for connection_id="
      << connection->get_id());
  _socket.shutdown(boost::asio::socket_base::shutdown_send);

  LOG("exit connection_id=" << connection->get_id());
  co_return;
}
}  // namespace framework
