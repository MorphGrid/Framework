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

#ifndef FRAMEWORK_TCP_HANDLERS_HPP
#define FRAMEWORK_TCP_HANDLERS_HPP

#include <framework/support.hpp>

namespace framework {
class tcp_handlers : public std::enable_shared_from_this<tcp_handlers> {
 public:
  using handler_type = std::function<async_of<void>(shared_tcp_endpoint service, shared_tcp_connection connection)>;
  using read_handler_type =
      std::function<async_of<void>(shared_tcp_endpoint service, shared_tcp_connection connection, std::string payload)>;
  using error_handler_type =
      std::function<async_of<void>(shared_tcp_endpoint service, shared_tcp_connection connection, const std::exception &exception)>;

  explicit tcp_handlers(handler_type on_connect = nullptr, handler_type on_accepted = nullptr, read_handler_type on_read = nullptr,
                        handler_type on_write = nullptr, handler_type on_disconnected = nullptr, error_handler_type on_error = nullptr);

  handler_type on_connect() const;
  handler_type on_accepted() const;
  read_handler_type on_read() const;
  handler_type on_write() const;
  handler_type on_disconnected() const;
  error_handler_type on_error() const;

 private:
  handler_type on_connect_;
  handler_type on_accepted_;
  read_handler_type on_read_;
  handler_type on_write_;
  handler_type on_disconnected_;
  error_handler_type on_error_;
};
}  // namespace framework

#endif  // FRAMEWORK_TCP_HANDLERS_HPP
