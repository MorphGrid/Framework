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
tcp_connection::tcp_connection(const uuid id, shared_of<tcp_executor> strand, shared_of<tcp_stream> stream, shared_tcp_service service)
    : service_(service), id_(id), strand_(std::move(strand)), stream_(std::move(stream)) {}

boost::asio::streambuf& tcp_connection::get_buffer() { return buffer_; }

uuid tcp_connection::get_id() const noexcept { return id_; }

shared_of<tcp_executor> tcp_connection::get_strand() const noexcept { return strand_; }

shared_of<tcp_stream> tcp_connection::get_stream() const noexcept { return stream_; }

shared_tcp_service tcp_connection::get_service() const noexcept { return service_; }

async_of<void> tcp_connection::notify_write() {
  if (service_->handlers()->on_write()) co_await service_->handlers()->on_write()(service_, shared_from_this());
}
}  // namespace framework
