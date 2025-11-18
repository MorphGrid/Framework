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
#include <framework/tcp_endpoint.hpp>

namespace framework {
tcp_endpoint::tcp_endpoint(const uuid id, const unsigned short int port, shared_tcp_handlers handlers)
    : id_(id), port_(port), callback_(std::move(handlers)) {}

shared_tcp_handlers tcp_endpoint::handlers() const { return callback_; }

uuid tcp_endpoint::get_id() const { return id_; }

unsigned short int tcp_endpoint::get_port() const { return port_; }

void tcp_endpoint::set_port(const unsigned short int port) { port_ = port; }

bool tcp_endpoint::get_running() const { return running_.load(std::memory_order_acquire); }

void tcp_endpoint::set_running(bool running) { running_.store(running, std::memory_order_release); }

void tcp_endpoint::add(shared_tcp_connection writer) {
  std::lock_guard lock(mutex_);
  writers_.emplace_back(std::move(writer));
}

void tcp_endpoint::remove(uuid session_id) {
  std::lock_guard lock(mutex_);
  std::erase_if(writers_, [&session_id](const shared_tcp_connection& w) { return w->get_id() == session_id; });
}

vector_of<shared_tcp_connection> tcp_endpoint::snapshot() {
  std::lock_guard lock(mutex_);
  return writers_;
}
}  // namespace framework
