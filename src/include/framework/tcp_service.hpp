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

#ifndef FRAMEWORK_TCP_SERVICE_HPP
#define FRAMEWORK_TCP_SERVICE_HPP

#include <framework/support.hpp>

namespace framework {
class tcp_service : std::enable_shared_from_this<tcp_service> {
  atomic_of<bool> running_{false};
  uuid id_;
  unsigned short int port_;
  std::mutex mutex_;
  vector_of<shared_tcp_connection> writers_;
  shared_tcp_handlers callback_;

 public:
  explicit tcp_service(uuid id, unsigned short int port = 0, shared_tcp_handlers handlers = nullptr);
  shared_tcp_handlers handlers() const;
  uuid get_id() const;
  unsigned short int get_port() const;
  void set_port(unsigned short int port);
  bool get_running() const;
  void set_running(bool running);
  void add(shared_tcp_connection writer);
  void remove(uuid session_id);
  vector_of<shared_tcp_connection> snapshot();
};
}  // namespace framework

#endif  // FRAMEWORK_TCP_SERVICE_HPP
