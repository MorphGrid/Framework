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

#include <framework/tcp_connection.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_service.hpp>
#include <framework/tcp_session.hpp>

namespace framework {
async_of<void> tcp_session(const shared_state state, const shared_tcp_service service, const shared_tcp_connection connection) {
  auto _cancellation_state = co_await boost::asio::this_coro::cancellation_state;

  if (service->handlers()->on_accepted()) co_await service->handlers()->on_accepted()(service, connection);

  while (!_cancellation_state.cancelled()) {
    connection->get_stream()->expires_after(std::chrono::minutes(60));

    auto _buffer = connection->get_buffer().prepare(1024);
    auto [_read_ec, _bytes_transferred] = co_await connection->get_stream()->async_read_some(_buffer, boost::asio::as_tuple);

    connection->get_buffer().commit(_bytes_transferred);

    if (_read_ec) {
      if (service->handlers()->on_disconnected()) co_await service->handlers()->on_disconnected()(service, connection);
      co_return;
    }

    if (service->handlers()->on_read()) co_await service->handlers()->on_read()(service, connection);
  }

  if (service->handlers()->on_disconnected()) co_await service->handlers()->on_disconnected()(service, connection);

  if (!connection->get_stream()->socket().is_open()) co_return;

  connection->get_stream()->socket().shutdown(socket::shutdown_send);
}
}  // namespace framework
