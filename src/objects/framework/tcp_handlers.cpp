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

#include <framework/tcp_handlers.hpp>

namespace framework {
    tcp_handlers::tcp_handlers(handler_type on_connect, handler_type on_accepted, read_handler_type on_read,
    handler_type on_write, handler_type on_disconnected, error_handler_type on_error)
  : on_connect_(std::move(on_connect)),
    on_accepted_(std::move(on_accepted)),
    on_read_(std::move(on_read)),
    on_write_(std::move(on_write)),
on_disconnected_(std::move(on_disconnected)),
on_error_(std::move(on_error))  {}

    tcp_handlers::handler_type tcp_handlers::on_connect() const  { return on_connect_; }

    tcp_handlers::handler_type tcp_handlers::on_accepted() const { return on_accepted_; }

    tcp_handlers::read_handler_type tcp_handlers::on_read() const { return on_read_; }

    tcp_handlers::handler_type tcp_handlers::on_write() const  { return on_write_; }

    tcp_handlers::handler_type tcp_handlers::on_disconnected() const { return on_disconnected_; }

    tcp_handlers::error_handler_type tcp_handlers::on_error() const { return on_error_; }
}  // namespace framework
