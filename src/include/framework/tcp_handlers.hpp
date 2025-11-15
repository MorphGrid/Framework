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
  using handler_type = std::function<async_of<void>(shared_tcp_service, shared_tcp_connection)>;

  explicit tcp_handlers(handler_type on_connect = nullptr, handler_type on_accepted = nullptr, handler_type on_read = nullptr,
                        handler_type on_write = nullptr, handler_type on_disconnected = nullptr)
      : on_connect_(std::move(on_connect)),
        on_accepted_(std::move(on_accepted)),
        on_read_(std::move(on_read)),
        on_write_(std::move(on_write)),
        on_disconnected_(std::move(on_disconnected)) {}

  handler_type on_connect() const { return on_connect_; }
  handler_type on_accepted() const { return on_accepted_; }
  handler_type on_read() const { return on_read_; }
  handler_type on_write() const { return on_write_; }
  handler_type on_disconnected() const { return on_disconnected_; }

  void set_on_connect(handler_type h) { on_connect_ = std::move(h); }
  void set_on_accepted(handler_type h) { on_accepted_ = std::move(h); }
  void set_on_read(handler_type h) { on_read_ = std::move(h); }
  void set_on_write(handler_type h) { on_write_ = std::move(h); }
  void set_on_disconnected(handler_type h) { on_disconnected_ = std::move(h); }

 private:
  handler_type on_connect_;
  handler_type on_accepted_;
  handler_type on_read_;
  handler_type on_write_;
  handler_type on_disconnected_;
};
}  // namespace framework

#endif  // FRAMEWORK_TCP_HANDLERS_HPP
