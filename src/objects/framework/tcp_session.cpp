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
  co_return co_await async_read(
      socket, buffer, boost::asio::transfer_exactly(n), boost::asio::as_tuple);
}

std::uint32_t read_uint32_from_buffer(boost::asio::streambuf &buffer) {
  static_assert(HEADER_SIZE == 4, "HEADER_SIZE must be 4");
  std::istream _input_stream(&buffer);
  std::array<unsigned char, HEADER_SIZE> _header{};
  _input_stream.read(reinterpret_cast<char *>(_header.data()), HEADER_SIZE);
  return static_cast<std::uint32_t>(_header[0]) << 24 |
         static_cast<std::uint32_t>(_header[1]) << 16 |
         static_cast<std::uint32_t>(_header[2]) << 8 |
         static_cast<std::uint32_t>(_header[3]) << 0;
}

async_of<void> notify_error_and_close(
    const shared_of<tcp_service> service,
    const shared_of<tcp_connection> connection,
    boost::asio::ip::tcp::socket &socket, const std::exception error) {
  if (service->handlers()->on_error())
    co_await service->handlers()->on_error()(service, connection, error);
  if (service->handlers()->on_disconnected())
    co_await service->handlers()->on_disconnected()(service, connection);
  service->remove(connection->get_id());
  boost::system::error_code _ec;
  socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
  socket.close(_ec);
  co_return;
}
async_of<void> notify_disconnected_if_present(
    const shared_of<tcp_service> service,
    const shared_of<tcp_connection> connection) {
  if (service->handlers()->on_disconnected())
    co_await service->handlers()->on_disconnected()(service, connection);
  service->remove(connection->get_id());
  co_return;
}

async_of<void> tcp_session(shared_state state, shared_of<tcp_service> service,
                           shared_of<tcp_connection> connection) {
  boost::ignore_unused(state);

  const auto _cancel_state =
      co_await boost::asio::this_coro::cancellation_state;
  if (service->handlers()->on_accepted())
    co_await service->handlers()->on_accepted()(service, connection);

  auto &_socket = connection->get_stream()->socket();
  auto &_buffer = connection->get_buffer();

  while (!_cancel_state.cancelled()) {
    connection->get_stream()->expires_after(std::chrono::minutes(60));

    if (auto [_read_header_ec, _header_read_bytes] =
            co_await read_exactly(_socket, _buffer, HEADER_SIZE);
        _read_header_ec) {
      boost::ignore_unused(_header_read_bytes);
      co_await notify_disconnected_if_present(service, connection);
      co_return;
    }

    const std::uint32_t _payload_size = read_uint32_from_buffer(_buffer);
    if (_payload_size == 0) continue;
    if (_payload_size > MAX_FRAME_SIZE) {
      co_await notify_error_and_close(service, connection, _socket,
                                      errors::tcp::frame_too_large{});
      co_return;
    }

    if (auto [_read_payload_ec, _payload_read_bytes] =
            co_await read_exactly(_socket, _buffer, _payload_size);
        _read_payload_ec) {
      boost::ignore_unused(_payload_read_bytes);
      co_await notify_error_and_close(service, connection, _socket,
                                      errors::tcp::on_read_error{});
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

    if (service->handlers()->on_read())
      co_await service->handlers()->on_read()(service, connection,
                                              std::move(_payload));
  }

  co_await notify_disconnected_if_present(service, connection);

  if (!connection->get_stream()->socket().is_open()) co_return;
  _socket.shutdown(boost::asio::socket_base::shutdown_send);
}
}  // namespace framework
