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
  LOG("entry: host=" << host << " port=" << port);
  boost::asio::ip::tcp::resolver _resolver(executor->get_inner_executor());
  auto _result =
      co_await _resolver.async_resolve(host, port, boost::asio::as_tuple);
  LOG("exit: ec=" << std::get<0>(_result).message());
  co_return _result;
}

async_of<boost::system::error_code> async_connect_socket(
    boost::asio::ip::tcp::socket& socket,
    const boost::asio::ip::tcp::resolver::results_type& iterator) {
  LOG("entry");
  // try to print one endpoint for debug
#ifdef DEBUG_ENABLED
  try {
    auto first = iterator.begin();
    if (first != iterator.end()) {
      LOG("first endpoint candidate: " << first->endpoint());
    } else {
      LOG("iterator empty");
    }
  } catch (...) {
    LOG("warning: cannot print iterator endpoint");
  }
#endif

  auto tie_result =
      co_await async_connect(socket, iterator, boost::asio::as_tuple);
  boost::system::error_code ec = std::get<0>(tie_result);
  LOG("exit: ec=" << ec.message());
  co_return ec;
}

async_of<void> backoff_wait(shared_of<tcp_executor> executor,
                            std::mt19937_64& rng,
                            std::uniform_real_distribution<double>& jitter_dist,
                            std::chrono::milliseconds base_delay,
                            std::chrono::milliseconds max_delay, int attempt) {
  LOG("entry: attempt=" << attempt);
  const unsigned int _shift = static_cast<unsigned int>(std::min(16, attempt));
  const auto _candidate = std::chrono::milliseconds(std::min<long long>(
      static_cast<long long>(base_delay.count()) * (1LL << _shift),
      max_delay.count()));
  const double _jitter = jitter_dist(rng);
  const auto _delay_ms = static_cast<long long>(_candidate.count() * _jitter);
  LOG("candidate=" << _candidate.count() << " jitter=" << _jitter
                   << " delay_ms=" << _delay_ms);
  boost::asio::steady_timer _timer{executor->get_inner_executor()};
  _timer.expires_after(std::chrono::milliseconds(_delay_ms));
  auto [ec] = co_await _timer.async_wait(boost::asio::as_tuple);
  if (ec == boost::asio::error::operation_aborted) {
    LOG("aborted by cancellation");
    co_return;
  }
  LOG("exit normally");
  co_return;
}

void configure_socket_options(boost::asio::ip::tcp::socket& socket) {
  LOG("entry");
  boost::system::error_code _ec;
  socket.set_option(boost::asio::ip::tcp::no_delay{true}, _ec);
  if (_ec) {
    std::cerr << "failed to set no_delay: " << _ec.message() << "\n";
  } else {
    LOG("TCP_NODELAY set OK");
  }
  // option: keep_alive commented intentionally
  LOG("exit");
}

async_of<bool> run_single_attempt(task_group& task_group, shared_state state,
                                  shared_of<tcp_service> service,
                                  shared_of<tcp_executor> executor) {
  LOG("entry");
  const auto host = service->get_host();
  const auto port = std::to_string(service->get_port());
  LOG("service_id=" << service->get_id() << " host=" << host
                    << " port=" << port);

  // Resolve
  LOG("resolving " << host << ":" << port);
  auto [_resolve_ec, _resolve_it] =
      co_await async_resolve_host(executor, host, port);
  if (_resolve_ec) {
    LOG("resolve failed: " << _resolve_ec.message());
    auto handlers = service->handlers();
    if (handlers && handlers->on_error()) {
      try {
        LOG("invoking "
            "handlers->on_error(host_not_resolved)");
        errors::tcp::host_not_resolved _error;
        co_await handlers->on_error()(service, nullptr, _error);
        LOG("handlers->on_error returned");
      } catch (const std::exception& ex) {
        std::cerr << "on_error threw: " << ex.what() << std::endl;
      } catch (...) {
        std::cerr << "on_error threw unknown\n";
      }
    } else {
      LOG("no on_error handler");
    }
    LOG("exit=false (resolve failed)");
    co_return false;
  }
  LOG("resolve OK");

  // Port sanity (diagnostic)
  if (service->get_port() == 0) {
    LOG("WARNING: service port == 0 (ephemeral not set?)");
  }

  // Connect
  LOG("attempting connect");
  boost::asio::ip::tcp::socket _socket(executor->get_inner_executor());
  if (auto _connect_ec = co_await async_connect_socket(_socket, _resolve_it);
      _connect_ec) {
    LOG("connect failed: " << _connect_ec.message());
    auto handlers = service->handlers();
    if (handlers && handlers->on_error()) {
      try {
        LOG("invoking "
            "handlers->on_error(service_not_found)");
        errors::tcp::service_not_found _error;
        co_await handlers->on_error()(service, nullptr, _error);
        LOG("handlers->on_error returned");
      } catch (const std::exception& ex) {
        std::cerr << "on_error(service_not_found) threw: " << ex.what()
                  << std::endl;
      } catch (...) {
        std::cerr << "on_error(service_not_found) threw "
                     "unknown\n";
      }
    } else {
      LOG("no on_error handler for connect failure");
    }
    boost::system::error_code _ec;
    _socket.shutdown(boost::asio::socket_base::shutdown_both, _ec);
    _socket.close(_ec);
    LOG("exit=false (connect failed)");
    co_return false;
  }
  LOG("connected OK");

  // configure socket
  LOG("configuring socket options");
  configure_socket_options(_socket);

  // wrap into stream/connection
  auto _stream = std::make_shared<tcp_stream>(std::move(_socket));
  const uuid _session_id = state->generate_id();
  auto _connection =
      std::make_shared<tcp_connection>(_session_id, executor, _stream, service);
  LOG("created connection id=" << _session_id);

  // add to service
  service->add(_connection);
  LOG("service->add done; snapshot_size=" << service->snapshot().size());

  // call on_connect/on_accepted if present (protected)
  {
    auto handlers = service->handlers();
    if (handlers && handlers->on_connect()) {
      try {
        LOG("calling on_connect handler");
        co_await handlers->on_connect()(service, _connection);
        LOG("on_connect returned");
      } catch (const std::exception& ex) {
        std::cerr << "on_connect threw: " << ex.what() << std::endl;
      } catch (...) {
        std::cerr << "on_connect threw unknown\n";
      }
    } else {
      LOG("no on_connect handler");
    }

    if (handlers && handlers->on_accepted()) {
      try {
        LOG("calling on_accepted handler");
        co_await handlers->on_accepted()(service, _connection);
        LOG("on_accepted returned");
      } catch (const std::exception& ex) {
        std::cerr << "on_accepted threw: " << ex.what() << std::endl;
      } catch (...) {
        std::cerr << "on_accepted threw unknown\n";
      }
    } else {
      LOG("no on_accepted handler");
    }
  }

  // spawn session
  LOG("spawning tcp_session for id=" << _session_id);
  co_spawn(*executor, tcp_session(state, service, _connection),
           task_group.adapt([](const std::exception_ptr& throwable) noexcept {
             if (throwable) {
               try {
                 std::rethrow_exception(throwable);
               } catch (const system_error& exception) {
                 LOG("Boost error: " << exception.what());
               } catch (const std::exception& e) {
                 LOG("std::exception: " << e.what());
               } catch (...) {
                 LOG("Unknown exception thrown.");
               }
             }
           }));
  LOG("tcp_session spawned, entering wait loop");

  const auto _cancel_state =
      co_await boost::asio::this_coro::cancellation_state;
  while (!_cancel_state.cancelled()) {
    if (!service->contains(_connection->get_id())) {
      LOG("connection id=" << _connection->get_id() << " removed by session");
      LOG("exit=true (session removed)");
      co_return true;
    }
    boost::asio::steady_timer _timer{executor->get_inner_executor()};
    _timer.expires_after(std::chrono::milliseconds(100));
    auto [wait_ec] = co_await _timer.async_wait(boost::asio::as_tuple);
    if (wait_ec == boost::asio::error::operation_aborted) {
      LOG("wait aborted via cancellation");
      break;
    }
  }

  // cancellation path
  LOG("cancellation detected, cleaning up connection id="
      << _connection->get_id());
  auto& _scoped_sock = _connection->get_stream()->socket();
  errors::tcp::connection_cancelled _error;
  co_await notify_error_and_close(service, _connection, _scoped_sock,
                                  std::runtime_error(_error.what()));
  LOG("exit=false (cancel path)");
  co_return false;
}

async_of<void> single_connection(task_group& task_group, shared_state state,
                                 shared_of<tcp_service> service,
                                 shared_of<tcp_executor> executor) {
  LOG("entry for service_id=" << service->get_id());
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
    LOG("attempt=" << _attempt);

    const bool _ok =
        co_await run_single_attempt(task_group, state, service, executor);
    LOG("run_single_attempt returned ok=" << (_ok ? "true" : "false"));

    if (static_cast<bool>(_cancellation_state.cancelled())) {
      LOG("cancellation observed, exiting");
      co_return;
    }

    if (_ok) {
      _attempt = 0;
      LOG("session finished normally, reset attempts and "
          "sleep 200ms");
      boost::asio::steady_timer _timer{executor->get_inner_executor()};
      _timer.expires_after(std::chrono::milliseconds(200));
      co_await _timer.async_wait(boost::asio::as_tuple);
    } else {
      if (_max_attempts > 0 && _attempt >= _max_attempts) {
        LOG("reached max attempts, exiting");
        co_return;
      }
      LOG("backing off before next attempt");
      co_await backoff_wait(executor, _random, _jitter_distribution,
                            _base_delay, _max_delay, _attempt);
    }
  }

  LOG("exit normally");
  co_return;
}

async_of<void> tcp_client(task_group& task_group, shared_state state,
                          shared_of<tcp_service> service) {
  LOG("entry for service_id=" << service->get_id());
  const auto _executor = co_await boost::asio::this_coro::executor;
  int _parallel_connections = service->get_scale();
  LOG("requested parallel_connections=" << _parallel_connections);

  if (_parallel_connections <= 0) {
    LOG("scale <= 0; forcing _parallel_connections=1 for "
        "debug/testing");
    _parallel_connections = 1;
  }

  for (int _iteration = 0; _iteration < _parallel_connections; ++_iteration) {
    auto _strand = std::make_shared<tcp_executor>(
        make_strand(_executor.get_inner_executor()));
    LOG("spawning single_connection iteration=" << _iteration);

    co_spawn(*_strand, single_connection(task_group, state, service, _strand),
             task_group.adapt([](const std::exception_ptr& throwable) noexcept {
               if (throwable) {
                 try {
                   std::rethrow_exception(throwable);
                 } catch (const system_error& exception) {
                   LOG("[tcp_client -> single_connection] Boost error: "
                       << exception.what());
                 } catch (const std::exception& e) {
                   LOG("[tcp_client -> single_connection] std::exception: "
                       << e.what());
                 } catch (...) {
                   LOG("[tcp_client -> single_connection] Unknown "
                       "exception thrown.");
                 }
               }
             }));
  }

  LOG("all single_connection spawned, setting service->running");
  service->set_running(true);
  LOG("service->set_running(true) done");
  LOG("exit");
  co_return;
}
}  // namespace framework
