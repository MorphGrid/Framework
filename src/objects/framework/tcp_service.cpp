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

#include <framework/task_group.hpp>
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

void tcp_service::set_task_group(shared_of<task_group> tg) { task_group_ = tg; }

shared_of<task_group> tcp_service::get_task_group() const {
  return task_group_;
}

void tcp_service::stop() {
  if (task_group_) {
    task_group_->emit(boost::asio::cancellation_type::total);
  }
  stop_clients();
}

uuid tcp_service::get_id() const { return id_; }

int tcp_service::get_scale() const {
  return scale_.load(std::memory_order_acquire);
}

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

bool tcp_service::remove(uuid session_id) {
  std::lock_guard lock(mutex_);
  const auto _iterator =
      std::find_if(writers_.begin(), writers_.end(),
                   [&session_id](const shared_of<tcp_connection>& w) {
                     return w && w->get_id() == session_id;
                   });
  if (_iterator == writers_.end()) return false;
  writers_.erase(_iterator);
  return true;
}

void tcp_service::scale_to(const int quantity) {
  scale_.store(quantity, std::memory_order_release);
}

bool tcp_service::contains(uuid session_id) {
  std::lock_guard lock(mutex_);
  return std::any_of(writers_.begin(), writers_.end(),
                     [&session_id](const shared_of<tcp_connection>& w) {
                       return w->get_id() == session_id;
                     });
}

vector_of<shared_of<tcp_connection>> tcp_service::snapshot() {
  std::lock_guard lock(mutex_);
  return writers_;
}

void tcp_service::stop_clients() {
  running_.store(false, std::memory_order_release);

  for (auto _connections = snapshot(); const auto& _connection : _connections) {
    if (!_connection) continue;

    if (const auto _stream = _connection->get_stream()) {
      boost::system::error_code _ec;
      auto& _socket = _stream->socket();
      _socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
      _socket.close(_ec);
    }

    if (bool was_removed = remove(_connection->get_id());
        was_removed && callback_ && callback_->on_disconnected()) {
      auto _service = shared_from_this();
      auto _connection_copied = _connection;
      co_spawn(
          *_connection->get_strand(),
          [_service, _connection_copied]() -> async_of<void> {
            try {
              co_await _service->handlers()->on_disconnected()(
                  _service, _connection_copied);
            } catch (const std::exception& e) {
              std::cerr << "[stop_clients] on_disconnected threw: " << e.what()
                        << "\n";
            } catch (...) {
              std::cerr << "[stop_clients] on_disconnected unknown exception\n";
            }
            co_return;
          },
          boost::asio::detached);
    }
  }
}
}  // namespace framework
