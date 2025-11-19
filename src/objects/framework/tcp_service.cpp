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
tcp_service::tcp_service(const uuid id, std::string host,
                         const unsigned short int port,
                         shared_of<tcp_handlers> handlers)
    : id_(id),
      host_(std::move(host)),
      port_(port),
      callback_(std::move(handlers)) {}

shared_of<tcp_handlers> tcp_service::handlers() const { return callback_; }

uuid tcp_service::get_id() const { return id_; }

std::string tcp_service::get_host() const { return host_; }

unsigned short int tcp_service::get_port() const { return port_; }

void tcp_service::set_port(const unsigned short int port) { port_ = port; }

bool tcp_service::get_running() const {
  return running_.load(std::memory_order_acquire);
}

void tcp_service::set_running(const bool running) {
  running_.store(running, std::memory_order_release);
}

void tcp_service::add(shared_of<tcp_connection> writer) {
  std::lock_guard lock(mutex_);
  writers_.emplace_back(std::move(writer));
}

void tcp_service::remove(uuid session_id) {
  std::lock_guard lock(mutex_);
  std::erase_if(writers_, [&session_id](const shared_of<tcp_connection> &w) {
    return w->get_id() == session_id;
  });
}

vector_of<shared_of<tcp_connection>> tcp_service::snapshot() {
  std::lock_guard lock(mutex_);
  return writers_;
}

void tcp_service::stop_clients() {
  running_.store(false, std::memory_order_release);

  for (auto _connections = snapshot(); const auto &_connection : _connections) {
    if (!_connection) continue;

    if (const auto _stream = _connection->get_stream()) {
      boost::system::error_code _ec;
      auto &_socket = _stream->socket();
      _socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
      _socket.close(_ec);
    }

    remove(_connection->get_id());

    if (callback_ && callback_->on_disconnected()) {
      auto _service = shared_from_this();
      auto _connection_copied = _connection;
      co_spawn(
          *_connection->get_strand(),
          [_service, _connection_copied]() -> async_of<void> {
            co_await _service->handlers()->on_disconnected()(
                _service, _connection_copied);
            co_return;
          },
          boost::asio::detached);
    }
  }
}
}  // namespace framework
