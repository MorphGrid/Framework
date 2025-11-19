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

#ifndef FRAMEWORK_TCP_ENDPOINT_HPP
#define FRAMEWORK_TCP_ENDPOINT_HPP

#include <framework/support.hpp>

namespace framework {
class tcp_endpoint : public std::enable_shared_from_this<tcp_endpoint> {
  atomic_of<bool> running_{false};
  uuid id_;
  unsigned short int port_;
  std::mutex mutex_;
  vector_of<shared_of<tcp_connection<tcp_endpoint>>> writers_;
  shared_of<tcp_handlers<tcp_endpoint, tcp_connection<tcp_endpoint>>> callback_;

 public:
  explicit tcp_endpoint(uuid id, unsigned short int port = 0,
                        shared_of<tcp_handlers<tcp_endpoint, tcp_connection<tcp_endpoint>>> handlers = nullptr);
  shared_of<tcp_handlers<tcp_endpoint, tcp_connection<tcp_endpoint>>> handlers() const;
  uuid get_id() const;
  unsigned short int get_port() const;
  void set_port(unsigned short int port);
  bool get_running() const;
  void set_running(bool running);
  void add(shared_of<tcp_connection<tcp_endpoint>> writer);
  void remove(uuid session_id);
  vector_of<shared_of<tcp_connection<tcp_endpoint>>> snapshot();
};
}  // namespace framework

#endif  // FRAMEWORK_TCP_ENDPOINT_HPP
