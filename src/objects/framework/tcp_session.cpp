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
#include <framework/tcp_session.hpp>

namespace framework {
async_of<void> tcp_session(const shared_state state, const shared_tcp_service service, const tcp_handlers callbacks,
                           const shared_of<auth> auth, const shared_tcp_connection connection) {
  auto _cancellation_state = co_await boost::asio::this_coro::cancellation_state;

  if (callbacks.on_accepted_) co_await callbacks.on_accepted_(service, auth, connection);

  while (!_cancellation_state.cancelled()) {
    connection->get_stream()->expires_after(std::chrono::minutes(60));

    auto [_read_ec, _] = co_await async_read(*connection->get_stream(), connection->get_buffer(), boost::asio::as_tuple);

    if (callbacks.on_read_) co_await callbacks.on_read_(service, auth, connection);

    if (_read_ec == boost::beast::http::error::end_of_stream) {
      co_return;
    }
  }

  if (callbacks.on_disconnected_) co_await callbacks.on_disconnected_(service, auth, connection);

  if (!connection->get_stream()->socket().is_open()) co_return;

  connection->get_stream()->socket().shutdown(socket::shutdown_send);
}
}  // namespace framework
