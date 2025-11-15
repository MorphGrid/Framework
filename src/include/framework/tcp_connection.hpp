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

#ifndef FRAMEWORK_TCP_CONNECTION_HPP
#define FRAMEWORK_TCP_CONNECTION_HPP

#include <framework/support.hpp>

namespace framework {
class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
  flat_buffer buffer_;

 public:
  tcp_connection(uuid id, shared_of<tcp_executor> strand, shared_of<tcp_stream> stream);

  flat_buffer& get_buffer();
  uuid get_id() const noexcept;
  shared_of<tcp_executor> get_strand() const noexcept;
  shared_of<tcp_stream> get_stream() const noexcept;

  template <typename Buffer>
  void invoke(Buffer&& buf) {
    auto self = shared_from_this();
    co_spawn(*strand_, [self, buf = std::forward<Buffer>(buf)]() mutable -> async_of<void> {
      co_await async_write(*self->stream_, boost::asio::buffer(buf));
      co_return;
    });
  }

 private:
  uuid id_;
  shared_of<tcp_executor> strand_;
  shared_of<tcp_stream> stream_;
};
}  // namespace framework

#endif  // FRAMEWORK_TCP_CONNECTION_HPP
