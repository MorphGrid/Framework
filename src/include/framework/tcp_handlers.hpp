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
  using handler_type = std::function<async_of<void>(shared_of<tcp_service>,
                                                    shared_of<tcp_connection>)>;
  using read_handler_type = std::function<async_of<void>(
      shared_of<tcp_service>, shared_of<tcp_connection>,
      std::string /* data */)>;
  using error_handler_type = std::function<async_of<void>(
      shared_of<tcp_service> service, shared_of<tcp_connection> connection,
      const std::exception &exception)>;

  explicit tcp_handlers(handler_type on_connect = nullptr,
                        handler_type on_accepted = nullptr,
                        read_handler_type on_read = nullptr,
                        handler_type on_write = nullptr,
                        handler_type on_disconnected = nullptr,
                        error_handler_type on_error = nullptr)
      : on_connect_(on_connect),
        on_accepted_(on_accepted),
        on_read_(on_read),
        on_write_(on_write),
        on_disconnected_(on_disconnected),
        on_error_(on_error) {}

  handler_type on_connect() const { return on_connect_; }
  handler_type on_accepted() const { return on_accepted_; }
  read_handler_type on_read() const { return on_read_; }
  handler_type on_write() const { return on_write_; }
  handler_type on_disconnected() const { return on_disconnected_; }
  error_handler_type on_error() const { return on_error_; }

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
