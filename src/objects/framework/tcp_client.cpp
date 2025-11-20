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

#include <framework/errors/tcp/connection_cancelled.hpp>
#include <framework/errors/tcp/host_not_resolved.hpp>
#include <framework/errors/tcp/service_not_found.hpp>
#include <framework/state.hpp>
#include <framework/task_group.hpp>
#include <framework/tcp_client.hpp>
#include <framework/tcp_connection.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_service.hpp>
#include <framework/tcp_session.hpp>

namespace framework {
static constexpr std::chrono::milliseconds DEFAULT_BASE_DELAY{500};
static constexpr std::chrono::milliseconds DEFAULT_MAX_DELAY{30000};
static constexpr int DEFAULT_MAX_ATTEMPTS = -1;
static constexpr double JITTER_MIN = 0.8;
static constexpr double JITTER_MAX = 1.2;

async_of<std::tuple<boost::system::error_code,
                    boost::asio::ip::tcp::resolver::results_type>>
async_resolve_host(shared_of<tcp_executor> executor, const std::string& host,
                   const std::string& port) {
  boost::asio::ip::tcp::resolver _resolver(executor->get_inner_executor());
  auto _result =
      co_await _resolver.async_resolve(host, port, boost::asio::as_tuple);
  co_return _result;
}

async_of<boost::system::error_code> async_connect_socket(
    boost::asio::ip::tcp::socket& socket,
    const boost::asio::ip::tcp::resolver::results_type& iterator) {
  auto tie_result =
      co_await async_connect(socket, iterator, boost::asio::as_tuple);
  boost::system::error_code ec = std::get<0>(tie_result);
  co_return ec;
}

async_of<void> backoff_wait(shared_of<tcp_executor> executor,
                            std::mt19937_64& rng,
                            std::uniform_real_distribution<>& jitter_dist,
                            std::chrono::milliseconds base_delay,
                            std::chrono::milliseconds max_delay, int attempt) {
  const unsigned int _shift = static_cast<unsigned int>(std::min(16, attempt));
  const auto _candidate = std::chrono::milliseconds(std::min<long long>(
      static_cast<long long>(base_delay.count()) * (1LL << _shift),
      max_delay.count()));
  const double _jitter = jitter_dist(rng);
  const auto _delay_ms = static_cast<long long>(_candidate.count() * _jitter);
  boost::asio::steady_timer _timer{executor->get_inner_executor()};
  _timer.expires_after(std::chrono::milliseconds(_delay_ms));
  auto [ec] = co_await _timer.async_wait(boost::asio::as_tuple);
  if (ec == boost::asio::error::operation_aborted) {
    co_return;
  }
}

void configure_socket_options(boost::asio::ip::tcp::socket& socket) {
  boost::system::error_code _ec;
  socket.set_option(boost::asio::ip::tcp::no_delay{true}, _ec);
  if (_ec) {
    std::cerr << "[configure_socket_options] failed to set no_delay: "
              << _ec.message() << "\n";
  }
}

async_of<bool> run_single_attempt(task_group& task_group, shared_state state,
                                  shared_of<tcp_service> service,
                                  shared_of<tcp_executor> executor) {
  const auto host = service->get_host();
  const auto port = std::to_string(service->get_port());

  std::cerr << "[run_single_attempt] resolving " << host << ":" << port << "\n";

  auto [_resolve_ec, _resolve_it] =
      co_await async_resolve_host(executor, host, port);
  if (_resolve_ec) {
    std::cerr << "[run_single_attempt] resolve failed: "
              << _resolve_ec.message() << "\n";
    if (service->handlers()->on_error()) {
      errors::tcp::host_not_resolved _error;
      co_await service->handlers()->on_error()(service, nullptr, _error);
    }
    co_return false;
  }

  std::cerr << "[run_single_attempt] resolved, attempting connect\n";

  boost::asio::ip::tcp::socket _socket(executor->get_inner_executor());
  if (auto _connect_ec = co_await async_connect_socket(_socket, _resolve_it);
      _connect_ec) {
    std::cerr << "[run_single_attempt] connect failed: "
              << _connect_ec.message() << "\n";
    if (service->handlers()->on_error()) {
      errors::tcp::service_not_found _error;
      co_await service->handlers()->on_error()(service, nullptr, _error);
    }
    boost::system::error_code _ec;
    _socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
    _socket.close(_ec);
    co_return false;
  }

  std::cerr << "[run_single_attempt] connected OK, configuring socket\n";

  configure_socket_options(_socket);

  auto _stream = std::make_shared<tcp_stream>(std::move(_socket));
  const uuid _session_id = state->generate_id();
  auto _connection =
      std::make_shared<tcp_connection>(_session_id, executor, _stream, service);

  std::cerr << "[run_single_attempt] created connection object id="
            << _session_id << "\n";
  service->add(_connection);
  std::cerr << "[run_single_attempt] service->add done. snapshot size="
            << service->snapshot().size() << "\n";

  if (service->handlers()->on_connect())
    co_await service->handlers()->on_connect()(service, _connection);

  if (service->handlers()->on_accepted())
    co_await service->handlers()->on_accepted()(service, _connection);

  co_spawn(*executor, tcp_session(state, service, _connection),
           task_group.adapt([](const std::exception_ptr& throwable) noexcept {
             if (throwable) {
               try {
                 std::rethrow_exception(throwable);
               } catch (const system_error& exception) {
                 std::cerr << "[tcp_session] Boost error: " << exception.what()
                           << std::endl;
               } catch (...) {
                 std::cerr << "[tcp_session] Unknown exception thrown."
                           << std::endl;
               }
             }
           }));

  const auto cancel_state = co_await boost::asio::this_coro::cancellation_state;
  while (!cancel_state.cancelled()) {
    if (!service->contains(_connection->get_id())) {
      co_return true;
    }
    boost::asio::steady_timer _timer{executor->get_inner_executor()};
    _timer.expires_after(std::chrono::milliseconds(100));
    co_await _timer.async_wait(boost::asio::as_tuple);
  }

  auto& _scoped_sock = _connection->get_stream()->socket();
  errors::tcp::connection_cancelled _error;
  co_await notify_error_and_close(service, _connection, _scoped_sock,
                                  static_cast<std::exception>(_error));
  co_return false;
}

async_of<void> single_connection(task_group& task_group, shared_state state,
                                 shared_of<tcp_service> service,
                                 shared_of<tcp_executor> executor) {
  std::mt19937_64 _random(std::random_device{}());
  std::uniform_real_distribution<double> _jitter_distribution(JITTER_MIN,
                                                              JITTER_MAX);

  int _attempt = 0;

  const std::chrono::milliseconds _base_delay = DEFAULT_BASE_DELAY;
  const std::chrono::milliseconds _max_delay = DEFAULT_MAX_DELAY;
  int _max_attempts = DEFAULT_MAX_ATTEMPTS;

  const auto _cancellation_state =
      co_await boost::asio::this_coro::cancellation_state;

  while (!_cancellation_state.cancelled()) {
    ++_attempt;

    const bool _ok =
        co_await run_single_attempt(task_group, state, service, executor);

    if (static_cast<bool>(_cancellation_state.cancelled())) {
      co_return;
    }

    if (_ok) {
      _attempt = 0;
      boost::asio::steady_timer _timer{executor->get_inner_executor()};
      _timer.expires_after(std::chrono::milliseconds(200));
      co_await _timer.async_wait(boost::asio::as_tuple);
    } else {
      if (_max_attempts > 0 && _attempt >= _max_attempts) {
        co_return;
      }
      co_await backoff_wait(executor, _random, _jitter_distribution,
                            _base_delay, _max_delay, _attempt);
    }
  }

  co_return;
}

async_of<void> tcp_client(task_group& task_group, shared_state state,
                          shared_of<tcp_service> service) {
  const auto _executor = co_await boost::asio::this_coro::executor;
  const int _parallel_connections = service->get_scale();

  for (int _iteration = 0; _iteration < _parallel_connections; ++_iteration) {
    auto _strand = std::make_shared<tcp_executor>(
        make_strand(_executor.get_inner_executor()));

    co_spawn(*_strand, single_connection(task_group, state, service, _strand),
             task_group.adapt([](const std::exception_ptr& throwable) noexcept {
               if (throwable) {
                 try {
                   std::rethrow_exception(throwable);
                 } catch (const system_error& exception) {
                   std::cerr
                       << "[tcp_client -> single_connection] Boost error: "
                       << exception.what() << std::endl;
                 } catch (...) {
                   std::cerr << "[tcp_client -> single_connection] Unknown "
                                "exception thrown."
                             << std::endl;
                 }
               }
             }));
  }

  service->set_running(true);

  co_return;
}
}  // namespace framework
