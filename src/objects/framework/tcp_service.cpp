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

#include <framework/tcp_service.hpp>
#include <framework/tcp_service_connection.hpp>
#include <framework/tcp_service_handlers.hpp>

namespace framework {
tcp_service::tcp_service(const uuid id, std::string host, const unsigned short int port, shared_tcp_service_handlers handlers)
    : id_(id), host_(std::move(host)), port_(port), callback_(std::move(handlers)) {}

shared_tcp_service_handlers tcp_service::handlers() const { return callback_; }

uuid tcp_service::get_id() const { return id_; }

std::string tcp_service::get_host() const { return host_; }

unsigned short int tcp_service::get_port() const { return port_; }

void tcp_service::set_port(const unsigned short int port) { port_ = port; }

bool tcp_service::get_running() const { return running_.load(std::memory_order_acquire); }

void tcp_service::set_running(const bool running) { running_.store(running, std::memory_order_release); }

void tcp_service::add(shared_tcp_service_connection writer) {
  std::lock_guard lock(mutex_);
  writers_.emplace_back(std::move(writer));
}

void tcp_service::remove(uuid session_id) {
  std::lock_guard lock(mutex_);
  std::erase_if(writers_, [&session_id](const shared_tcp_service_connection &w) { return w->get_id() == session_id; });
}

vector_of<shared_tcp_service_connection> tcp_service::snapshot() {
  std::lock_guard lock(mutex_);
  return writers_;
}

void tcp_service::stop_clients() {
  // marcar el servicio como no running primero
  running_.store(false, std::memory_order_release);

  // tomar snapshot de conexiones y cerrarlas
  auto _connections = snapshot();
  for (auto &_connection : _connections) {
    if (!_connection) continue;

    try {
      auto _stream = _connection->get_stream();
      if (_stream) {
        boost::system::error_code ec;
        auto &_sock = _stream->socket();
        // shutdown & close (ignorar errores)
        _sock.shutdown(boost::asio::socket_base::shutdown_both, ec);
        _sock.close(ec);
      }
    } catch (...) {
      // swallow exceptions durante cierre
    }

    // Remover la conexión de la lista (si tcp_service_session no lo hizo ya)
    try {
      remove(_connection->get_id());
    } catch (...) {
    }

    try {
      if (callback_ && callback_->on_disconnected()) {
        auto _service = shared_from_this();
        auto _connection_copied = _connection;
        co_spawn(
            *_connection->get_strand(),
            [_service, _connection_copied]() -> async_of<void> {
              try {
                co_await _service->handlers()->on_disconnected()(_service, _connection_copied);
              } catch (...) {
              }
              co_return;
            },
            boost::asio::detached);
      }
    } catch (...) {
      // swallow
    }
  }
}
}  // namespace framework
