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

#include <framework/auth.hpp>
#include <framework/support.hpp>

namespace framework {
class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
  boost::asio::streambuf buffer_;
  shared_tcp_endpoint service_;
  shared_auth auth_ = std::make_shared<auth>();

 public:
  tcp_connection(uuid id, shared_of<tcp_executor> strand, shared_of<tcp_stream> stream, shared_tcp_endpoint service);

  boost::asio::streambuf &get_buffer();

  uuid get_id() const noexcept;

  shared_of<tcp_executor> get_strand() const noexcept;

  shared_of<tcp_stream> get_stream() const noexcept;

  async_of<void> notify_write();

  template <typename Buffer>
  void invoke(Buffer &&buf) {
    auto _self = shared_from_this();
    co_spawn(
        *strand_,
        [_self, _buf = std::forward<Buffer>(buf)]() mutable -> async_of<void> {
          auto _payload_buffer = boost::asio::buffer(_buf);
          const std::size_t _payload_size = boost::asio::buffer_size(_payload_buffer);

          if (_payload_size > MAX_FRAME_SIZE) {
            std::cerr << "Payload size is too big" << std::endl;
            co_return;
          }

          const auto _length = static_cast<std::uint32_t>(_payload_size);

          std::array<unsigned char, 4> _header;
          _header[0] = static_cast<unsigned char>(_length >> 24 & 0xFF);
          _header[1] = static_cast<unsigned char>(_length >> 16 & 0xFF);
          _header[2] = static_cast<unsigned char>(_length >> 8 & 0xFF);
          _header[3] = static_cast<unsigned char>(_length >> 0 & 0xFF);

          std::array<boost::asio::const_buffer, 2> _buffers{boost::asio::buffer(_header), _payload_buffer};

          co_await boost::asio::async_write(*_self->stream_, _buffers, boost::asio::as_tuple);
          co_await _self->notify_write();
          co_return;
        },
        boost::asio::detached);
  }

 private:
  uuid id_;
  shared_of<tcp_executor> strand_;
  shared_of<tcp_stream> stream_;
};
}  // namespace framework

#endif  // FRAMEWORK_TCP_CONNECTION_HPP
