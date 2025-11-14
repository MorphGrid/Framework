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

#include <framework/kernel.hpp>
#include <framework/session.hpp>

namespace framework {
async_of<void> session(const shared_state state, tcp_stream stream) {
  flat_buffer _buffer;
  auto _cancellation_state = co_await boost::asio::this_coro::cancellation_state;

  while (!_cancellation_state.cancelled()) {
    stream.expires_after(std::chrono::seconds(5));

    request_type _request;
    auto [_read_ec, _] = co_await async_read(stream, _buffer, _request, boost::asio::as_tuple);
    if (_read_ec == boost::beast::http::error::end_of_stream) {
      co_return;
    }
    message _message = co_await kernel(state, std::move(_request));

    const bool keep_alive = _message.keep_alive();
    co_await async_write(stream, std::move(_message));

    if (!keep_alive) {
      co_return;
    }
  }

  if (!stream.socket().is_open()) co_return;

  stream.socket().shutdown(socket::shutdown_send);
}
}  // namespace framework
