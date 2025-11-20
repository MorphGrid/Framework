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

namespace framework {
tcp_connection::tcp_connection(uuid id, shared_of<tcp_executor> strand,
                               shared_of<tcp_stream> stream,
                               shared_of<tcp_service> service)
    : service_(std::move(service)), id_(id), strand_(strand), stream_(stream) {
  LOG("constructor enter, connection_id=" << id_);
  LOG("constructor metadata: has_stream="
      << static_cast<bool>(stream_)
      << ", has_strand=" << static_cast<bool>(strand_)
      << ", service_id=" << (service_ ? service_->get_id() : uuid{}));
  LOG("constructor exit, connection_id=" << id_);
}

boost::asio::streambuf& tcp_connection::get_buffer() {
  LOG("get_buffer() enter, connection_id=" << id_ << ", buffer_size="
                                           << buffer_.size());
  LOG("get_buffer() exit, connection_id=" << id_);
  return buffer_;
}

uuid tcp_connection::get_id() const noexcept {
  LOG("get_id() called, returning connection_id=" << id_);
  return id_;
}

shared_of<tcp_executor> tcp_connection::get_strand() const noexcept {
  LOG("get_strand() called, connection_id=" << id_ << ", has_strand="
                                            << static_cast<bool>(strand_));
  return strand_;
}

shared_of<tcp_stream> tcp_connection::get_stream() const noexcept {
  LOG("get_stream() called, connection_id=" << id_ << ", has_stream="
                                            << static_cast<bool>(stream_));
  return stream_;
}

async_of<void> tcp_connection::notify_write() {
  LOG("notify_write() enter, connection_id=" << id_);
  if (service_->handlers()->on_write()) {
    LOG("notify_write() handler present, invoking on_write for connection_id="
        << id_);
    co_await service_->handlers()->on_write()(service_,
                                              this->shared_from_this());
    LOG("notify_write() handler completed for connection_id=" << id_);
  } else {
    LOG("notify_write() no on_write handler registered for connection_id="
        << id_);
  }
  LOG("notify_write() exit, connection_id=" << id_);
}
}  // namespace framework
