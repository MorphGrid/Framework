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
      callback_(std::move(handlers)) {
  LOG("tcp_service constructor: id=" << id_ << " host=" << host_
                                     << " port=" << port_);
}

shared_of<tcp_handlers> tcp_service::handlers() const {
  LOG("handlers() called");
  return callback_;
}

void tcp_service::set_task_group(shared_of<task_group> tg) {
  LOG("set_task_group()");
  task_group_ = tg;
}

shared_of<task_group> tcp_service::get_task_group() const {
  LOG("get_task_group()");
  return task_group_;
}

void tcp_service::stop() {
  LOG("stop() enter");

  if (task_group_) {
    LOG("stop(): emitting cancellation");
    task_group_->emit(boost::asio::cancellation_type::total);
  }

  LOG("stop(): calling stop_clients()");
  stop_clients();

  LOG("stop() exit");
}

uuid tcp_service::get_id() const {
  LOG("get_id() called, returning id=" << id_);
  return id_;
}

int tcp_service::get_scale() const {
  int v = scale_.load(std::memory_order_acquire);
  LOG("get_scale() returning " << v);
  return v;
}

std::string tcp_service::get_host() const {
  LOG("get_host() returning host=" << host_);
  return host_;
}

unsigned short int tcp_service::get_port() const {
  LOG("get_port() returning port=" << port_);
  return port_;
}

void tcp_service::set_port(const unsigned short int port) {
  LOG("set_port(" << port << ")");
  port_ = port;
}

bool tcp_service::get_running() const {
  bool v = running_.load(std::memory_order_acquire);
  LOG("get_running() returning " << v);
  return v;
}

void tcp_service::set_running(const bool running) {
  LOG("set_running(" << running << ")");
  running_.store(running, std::memory_order_release);
}

void tcp_service::add(shared_of<tcp_connection> writer) {
  LOG("add(): locking mutex, adding connection id="
      << (writer ? writer->get_id() : uuid{}));

  std::lock_guard lock(mutex_);
  writers_.emplace_back(std::move(writer));

  LOG("add(): writer added successfully");
}

bool tcp_service::remove(uuid session_id) {
  LOG("remove(" << session_id << ") enter");

  std::lock_guard lock(mutex_);

  const auto _iterator =
      std::find_if(writers_.begin(), writers_.end(),
                   [&session_id](const shared_of<tcp_connection>& w) {
                     return w && w->get_id() == session_id;
                   });

  if (_iterator == writers_.end()) {
    LOG("remove(): session_id not found");
    return false;
  }

  writers_.erase(_iterator);
  LOG("remove(): session_id removed successfully");
  return true;
}

void tcp_service::scale_to(const int quantity) {
  LOG("scale_to(" << quantity << ")");
  scale_.store(quantity, std::memory_order_release);
}

bool tcp_service::contains(uuid session_id) {
  LOG("contains(" << session_id << ") enter");

  std::lock_guard lock(mutex_);

  bool found = std::any_of(writers_.begin(), writers_.end(),
                           [&session_id](const shared_of<tcp_connection>& w) {
                             return w->get_id() == session_id;
                           });

  LOG("contains(): result=" << found);
  return found;
}

vector_of<shared_of<tcp_connection>> tcp_service::snapshot() {
  LOG("snapshot() enter");

  std::lock_guard lock(mutex_);
  LOG("snapshot(): copying " << writers_.size() << " connections");

  return writers_;
}

void tcp_service::stop_clients() {
  LOG("stop_clients() enter");

  running_.store(false, std::memory_order_release);

  for (auto _connections = snapshot(); const auto& _connection : _connections) {
    if (!_connection) {
      LOG("stop_clients(): null connection skipped");
      continue;
    }

    LOG("stop_clients(): closing connection id=" << _connection->get_id());

    if (const auto _stream = _connection->get_stream()) {
      boost::system::error_code _ec;
      auto& _socket = _stream->socket();
      _socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
      _socket.close(_ec);

      LOG("stop_clients(): socket closed for id=" << _connection->get_id()
                                                  << " ec=" << _ec.message());
    }

    if (bool was_removed = remove(_connection->get_id());
        was_removed && callback_ && callback_->on_disconnected()) {
      LOG("stop_clients(): invoking on_disconnected handler for id="
          << _connection->get_id());

      auto _service = shared_from_this();
      auto _connection_copied = _connection;

      co_spawn(
          *_connection->get_strand(),
          [_service, _connection_copied]() -> async_of<void> {
            try {
              LOG("stop_clients(): co_spawn on_disconnected begin");
              co_await _service->handlers()->on_disconnected()(
                  _service, _connection_copied);
              LOG("stop_clients(): co_spawn on_disconnected completed");
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

  LOG("stop_clients() exit");
}
}  // namespace framework
