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

#include <gtest/gtest.h>

#include <framework/controller.hpp>
#include <framework/jwt.hpp>
#include <framework/metrics.hpp>
#include <framework/queue.hpp>
#include <framework/route.hpp>
#include <framework/router.hpp>
#include <framework/server.hpp>
#include <framework/state.hpp>
#include <framework/support.hpp>
#include <framework/task_group.hpp>
#include <framework/tcp_connection.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_kind.hpp>
#include <framework/tcp_service.hpp>

using namespace framework;

class test_server : public testing::Test {
 public:
  shared_of<server> server_;
  shared_of<std::jthread> thread_;
  std::atomic<int> tcp_counter_{0};
  std::atomic<bool> client_connected_{false};
  std::atomic<bool> client_accepted_{false};
  std::atomic<bool> client_read_{false};
  std::atomic<bool> client_write_{false};
  std::atomic<bool> client_disconnected_{false};

 protected:
  void SetUp() override {
    server_ = std::make_shared<server>();

    auto _router = server_->get_state()->get_router();

    _router->add(std::make_shared<route>(
        vector_of{
            http_verb::get,
        },
        "/system_error",
        std::make_shared<controller>(
            [](const shared_state state, const request_type request,
               const params_type params,
               const shared_auth auth) -> async_of<response_type> {
              response_empty_type _response{http_status::ok, request.version()};
              _response.prepare_payload();
              throw std::system_error();
              co_return _response;
            })));

    thread_ = std::make_shared<std::jthread>([this]() {
      server_->start();
      server_->get_state()->set_running(false);
    });

    thread_->detach();

    while (server_->get_state()->get_running() == false) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  void TearDown() override {
    server_->get_task_group()->emit(boost::asio::cancellation_type::total);
    while (server_->get_state()->get_running() == true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    server_->get_state()->ioc().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
};

TEST_F(test_server, can_handle_http_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::get, "/api/status", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_unauthorized_requests) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::get, "/api/user", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 401);

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_post_auth_attempt_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::post, "/api/auth/attempt", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.body() = serialize(object({
      {"email", "sysop@morphgrid.localhost"},
      {"password", "password"},
  }));
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("data"));
  ASSERT_TRUE(_result.as_object().at("data").is_object());
  ASSERT_TRUE(_result.as_object().at("data").as_object().contains("token"));
  ASSERT_TRUE(
      _result.as_object().at("data").as_object().at("token").is_string());
  std::string _auth_id{
      _result.as_object().at("data").as_object().at("token").as_string()};

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_post_auth_attempt_request_on_wrong_email) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::post, "/api/auth/attempt", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.body() = serialize(object({
      {"email", "wrong@zendev.cl"},
      {"password", "password"},
  }));
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 422);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("message"));
  ASSERT_TRUE(_result.as_object().at("message").is_string());
  ASSERT_TRUE(_result.as_object().at("errors").is_object());
  ASSERT_TRUE(_result.as_object().at("errors").as_object().contains("email"));
  ASSERT_TRUE(
      _result.as_object().at("errors").as_object().at("email").is_array());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("email")
                .as_array()
                .size(),
            1);
  ASSERT_TRUE(_result.as_object()
                  .at("errors")
                  .as_object()
                  .at("email")
                  .as_array()
                  .at(0)
                  .is_string());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("email")
                .as_array()
                .at(0)
                .as_string(),
            "The email isn't registered.");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_post_auth_attempt_request_on_wrong_password) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::post, "/api/auth/attempt", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.body() = serialize(object({
      {"email", "sysop@morphgrid.localhost"},
      {"password", "wrong_password"},
  }));
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 422);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("message"));
  ASSERT_TRUE(_result.as_object().at("message").is_string());
  ASSERT_TRUE(_result.as_object().at("errors").is_object());
  ASSERT_TRUE(
      _result.as_object().at("errors").as_object().contains("password"));
  ASSERT_TRUE(
      _result.as_object().at("errors").as_object().at("password").is_array());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("password")
                .as_array()
                .size(),
            1);
  ASSERT_TRUE(_result.as_object()
                  .at("errors")
                  .as_object()
                  .at("password")
                  .as_array()
                  .at(0)
                  .is_string());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("password")
                .as_array()
                .at(0)
                .as_string(),
            "The password is incorrect.");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_get_user_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  auto _id = boost::uuids::random_generator()();
  auto _jwt = jwt::make(_id, server_->get_state()->get_key());
  request_type _request{http_verb::get, "/api/user", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, _jwt->as_string());
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 54);
  ASSERT_EQ(_response.result_int(), 200);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("data"));
  ASSERT_TRUE(_result.as_object().at("data").is_object());
  ASSERT_TRUE(_result.as_object().at("data").as_object().contains("id"));
  ASSERT_TRUE(_result.as_object().at("data").as_object().at("id").is_string());
  std::string _auth_id{
      _result.as_object().at("data").as_object().at("id").as_string()};
  ASSERT_EQ(_auth_id, to_string(_id));

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_get_queues_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  auto _id = boost::uuids::random_generator()();
  auto _jwt = jwt::make(_id, server_->get_state()->get_key());
  request_type _request{http_verb::get, "/api/queues", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, _jwt->as_string());
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("data"));
  ASSERT_TRUE(_result.as_object().at("data").is_array());
  ASSERT_TRUE(_result.as_object().at("data").as_array().size() == 1);
  ASSERT_TRUE(_result.as_object().at("data").as_array().at(0).is_object());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "id"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("id")
                  .is_string());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "name"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("name")
                  .is_string());
  std::string _name{_result.as_object()
                        .at("data")
                        .as_array()
                        .at(0)
                        .as_object()
                        .at("name")
                        .as_string()};
  ASSERT_EQ(_name, "metrics");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_get_queue_tasks_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  auto _id = boost::uuids::random_generator()();
  auto _jwt = jwt::make(_id, server_->get_state()->get_key());
  request_type _request{http_verb::get, "/api/queues/metrics/tasks", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, _jwt->as_string());
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("data"));
  ASSERT_TRUE(_result.as_object().at("data").is_array());
  ASSERT_TRUE(_result.as_object().at("data").as_array().size() == 1);
  ASSERT_TRUE(_result.as_object().at("data").as_array().at(0).is_object());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "id"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("id")
                  .is_string());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "name"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("name")
                  .is_string());
  std::string _name{_result.as_object()
                        .at("data")
                        .as_array()
                        .at(0)
                        .as_object()
                        .at("name")
                        .as_string()};
  ASSERT_EQ(_name, "increase_requests");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_get_queue_jobs_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  server_->get_state()->get_queue("metrics")->dispatch("increase_requests");

  auto _id = boost::uuids::random_generator()();
  auto _jwt = jwt::make(_id, server_->get_state()->get_key());
  request_type _request{http_verb::get, "/api/queues/metrics/jobs", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, _jwt->as_string());
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("data"));
  ASSERT_TRUE(_result.as_object().at("data").is_array());
  ASSERT_TRUE(_result.as_object().at("data").as_array().size() == 1);
  ASSERT_TRUE(_result.as_object().at("data").as_array().at(0).is_object());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "id"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("id")
                  .is_string());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "task_id"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("task_id")
                  .is_string());

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_get_queue_workers_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  server_->get_state()->get_queue("metrics")->dispatch("increase_requests");

  auto _id = boost::uuids::random_generator()();
  auto _jwt = jwt::make(_id, server_->get_state()->get_key());
  request_type _request{http_verb::get, "/api/queues/metrics/workers", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, _jwt->as_string());
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("data"));
  ASSERT_TRUE(_result.as_object().at("data").is_array());
  ASSERT_TRUE(_result.as_object().at("data").as_array().size() == 1);
  ASSERT_TRUE(_result.as_object().at("data").as_array().at(0).is_object());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "id"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("id")
                  .is_string());
  ASSERT_TRUE(
      _result.as_object().at("data").as_array().at(0).as_object().contains(
          "number_of_tasks"));
  ASSERT_TRUE(_result.as_object()
                  .at("data")
                  .as_array()
                  .at(0)
                  .as_object()
                  .at("number_of_tasks")
                  .is_number());

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_post_queue_dispatch_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  auto _id = boost::uuids::random_generator()();
  auto _jwt = jwt::make(_id, server_->get_state()->get_key());
  request_type _request{http_verb::post, "/api/queues/metrics/dispatch", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, _jwt->as_string());
  _request.body() =
      serialize(object{{"task", "increase_requests"}, {"data", object{}}});
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.result_int(), 200);

  ASSERT_EQ(server_->get_state()->get_metrics()->_requests.load(), 1);

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_throw_unprocessable_entity_on_invalid_body) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::post, "/api/auth/attempt", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.body() = "";
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 422);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("message"));
  ASSERT_TRUE(_result.as_object().at("message").is_string());
  ASSERT_EQ(_result.as_object().at("message").as_string(),
            "The given data was invalid.");
  ASSERT_TRUE(_result.as_object().contains("errors"));
  ASSERT_TRUE(_result.as_object().at("errors").is_object());
  ASSERT_TRUE(_result.as_object().at("errors").as_object().contains("*"));
  ASSERT_TRUE(_result.as_object().at("errors").as_object().at("*").is_array());
  ASSERT_EQ(
      _result.as_object().at("errors").as_object().at("*").as_array().size(),
      1);
  ASSERT_TRUE(_result.as_object()
                  .at("errors")
                  .as_object()
                  .at("*")
                  .as_array()
                  .at(0)
                  .is_string());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("*")
                .as_array()
                .at(0)
                .as_string(),
            "The payload must be a valid json value.");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_throw_unprocessable_entity_on_invalid_payload) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::post, "/api/auth/attempt", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.body() = "{}";
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_GT(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 422);

  boost::system::error_code _parse_ec;
  auto _result = boost::json::parse(_response.body(), _parse_ec);

  ASSERT_EQ(_parse_ec, boost::beast::errc::success);
  ASSERT_TRUE(_result.is_object());
  ASSERT_TRUE(_result.as_object().contains("message"));
  ASSERT_TRUE(_result.as_object().at("message").is_string());
  ASSERT_EQ(_result.as_object().at("message").as_string(),
            "The given data was invalid.");
  ASSERT_TRUE(_result.as_object().contains("errors"));
  ASSERT_TRUE(_result.as_object().at("errors").is_object());
  ASSERT_TRUE(_result.as_object().at("errors").as_object().contains("email"));
  ASSERT_TRUE(
      _result.as_object().at("errors").as_object().at("email").is_array());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("email")
                .as_array()
                .size(),
            1);
  ASSERT_TRUE(_result.as_object()
                  .at("errors")
                  .as_object()
                  .at("email")
                  .as_array()
                  .at(0)
                  .is_string());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("email")
                .as_array()
                .at(0)
                .as_string(),
            "Attribute email is required.");
  ASSERT_TRUE(
      _result.as_object().at("errors").as_object().contains("password"));
  ASSERT_TRUE(
      _result.as_object().at("errors").as_object().at("password").is_array());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("password")
                .as_array()
                .size(),
            1);
  ASSERT_TRUE(_result.as_object()
                  .at("errors")
                  .as_object()
                  .at("password")
                  .as_array()
                  .at(0)
                  .is_string());
  ASSERT_EQ(_result.as_object()
                .at("errors")
                .as_object()
                .at("password")
                .as_array()
                .at(0)
                .as_string(),
            "Attribute password is required.");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_throw_unauthorized_on_invalid_tokens) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::get, "/api/user", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.set(http_field::authorization, "Bearer ...");
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 401);

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_timeout_http_sessions) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::get, "/api/status", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 200);

  std::this_thread::sleep_for(std::chrono::seconds(6));

  boost::beast::error_code _write_ec;
  write(_stream, _request, _write_ec);
  ASSERT_EQ(_write_ec, boost::beast::errc::success);

  boost::beast::error_code _disconnect_ec;
  _stream.socket().shutdown(socket::shutdown_both, _disconnect_ec);

  ASSERT_EQ(_disconnect_ec, boost::beast::errc::not_connected);
}

TEST_F(test_server, can_handle_http_cors_request) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::options, "/api/status", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 204);
  ASSERT_EQ(_response[http_field::access_control_allow_methods], "GET");

  request_type _another_request{http_verb::options, "/not-found", 11};
  _another_request.set(http_field::host, _host);
  _another_request.set(http_field::user_agent, "Client");
  _another_request.prepare_payload();

  write(_stream, _another_request);
  flat_buffer _another_buffer;

  response_type _another_response;
  read(_stream, _another_buffer, _another_response);

  ASSERT_EQ(_another_response.body().size(), 0);
  ASSERT_EQ(_another_response.result_int(), 204);
  ASSERT_EQ(_another_response[http_field::access_control_allow_methods], "");

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, can_handle_exceptions) {
  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const std::string _host = "127.0.0.1";
  const unsigned short int _port = server_->get_state()->get_port();
  auto const _tcp_resolver_results =
      _resolver.resolve(_host, std::to_string(_port));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_tcp_resolver_results);

  request_type _request{http_verb::get, "/system_error", 11};
  _request.set(http_field::host, _host);
  _request.set(http_field::user_agent, "Client");
  _request.prepare_payload();

  write(_stream, _request);
  flat_buffer _buffer;

  response_type _response;
  read(_stream, _buffer, _response);

  ASSERT_EQ(_response.body().size(), 0);
  ASSERT_EQ(_response.result_int(), 500);

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);
}

TEST_F(test_server, basic_tcp_endpoint_check) {
  auto _service =
      server_->bind(tcp_kind::SERVER, "0.0.0.0", 0,
                    std::make_shared<tcp_handlers>(
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_connected_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_accepted_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>, shared_of<tcp_connection>,
                            const std::string payload) -> async_of<void> {
                          boost::ignore_unused(payload);
                          client_read_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_write_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_disconnected_.store(true);
                          co_return;
                        }));

  while (!_service->get_running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  boost::asio::io_context _client_ioc;
  resolver _resolver(_client_ioc);
  const auto _results =
      _resolver.resolve("127.0.0.1", std::to_string(_service->get_port()));
  tcp_stream _stream(_client_ioc);
  _stream.connect(_results);

  std::string _data = "ping";
  auto _payload_length = static_cast<std::uint32_t>(_data.size());

  unsigned char _header[4];
  _header[0] = static_cast<unsigned char>(_payload_length >> 24 & 0xFF);
  _header[1] = static_cast<unsigned char>(_payload_length >> 16 & 0xFF);
  _header[2] = static_cast<unsigned char>(_payload_length >> 8 & 0xFF);
  _header[3] = static_cast<unsigned char>(_payload_length >> 0 & 0xFF);

  std::array<boost::asio::const_buffer, 2> _buffers{
      boost::asio::buffer(_header, sizeof(_header)),
      boost::asio::buffer(_data)};
  boost::asio::write(_stream.socket(), _buffers);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_TRUE(client_connected_.load());
  ASSERT_TRUE(client_accepted_.load());
  ASSERT_TRUE(client_read_.load());

  const auto _writer = _service->snapshot().front();
  std::string _pong = "pong";
  _writer->invoke(_pong);

  unsigned char _response_header[4] = {0, 0, 0, 0};
  boost::asio::read(_stream.socket(), boost::asio::buffer(_response_header, 4));

  std::uint32_t _response_length =
      static_cast<std::uint32_t>(_response_header[0]) << 24 |
      static_cast<std::uint32_t>(_response_header[1]) << 16 |
      static_cast<std::uint32_t>(_response_header[2]) << 8 |
      static_cast<std::uint32_t>(_response_header[3]) << 0;

  ASSERT_EQ(_response_length, 4u);

  std::vector<char> _response_payload(_response_length);
  boost::asio::read(
      _stream.socket(),
      boost::asio::buffer(_response_payload.data(), _response_payload.size()));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_EQ(_response_payload[0], 'p');
  ASSERT_EQ(_response_payload[1], 'o');
  ASSERT_EQ(_response_payload[2], 'n');
  ASSERT_EQ(_response_payload[3], 'g');
  ASSERT_TRUE(client_write_.load());

  boost::beast::error_code _ec;
  _stream.socket().shutdown(socket::shutdown_both, _ec);
  ASSERT_EQ(_ec, boost::beast::errc::success);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_TRUE(client_disconnected_.load());
}

TEST_F(test_server, basic_tcp_endpoint_check_with_runtime_client) {
  const auto _endpoint =
      server_->bind(tcp_kind::SERVER, "0.0.0.0", 0,
                    std::make_shared<tcp_handlers>(
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_connected_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_accepted_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>, shared_of<tcp_connection>,
                            const std::string payload) -> async_of<void> {
                          boost::ignore_unused(payload);
                          client_read_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_write_.store(true);
                          co_return;
                        },
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_disconnected_.store(true);
                          co_return;
                        }));

  auto _wait_for_flag = [](auto _pred, int _timeout_ms) -> bool {
    const int _step_ms = 5;
    int _waited = 0;
    while (!_pred() && _waited < _timeout_ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(_step_ms));
      _waited += _step_ms;
    }
    return _pred();
  };

  while (!_endpoint->get_running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  auto _service =
      server_->bind(tcp_kind::CLIENT, "127.0.0.1", _endpoint->get_port(),
                    std::make_shared<tcp_handlers>(
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection> conn) -> async_of<void> {
                          std::string _ping = "ping";
                          conn->invoke(_ping);
                          co_return;
                        },
                        nullptr,
                        [&](shared_of<tcp_service>, shared_of<tcp_connection>,
                            std::string payload) -> async_of<void> {
                          if (payload == "pong") {
                            client_write_.store(true);
                          }
                          co_return;
                        },
                        nullptr,
                        [&](shared_of<tcp_service>,
                            shared_of<tcp_connection>) -> async_of<void> {
                          client_disconnected_.store(true);
                          co_return;
                        },
                        nullptr));

  while (!_service->get_running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_TRUE(
      _wait_for_flag([&]() { return client_connected_.load(); }, 2000) &&
      "client_connected timed out");
  ASSERT_TRUE(_wait_for_flag([&]() { return client_accepted_.load(); }, 2000) &&
              "client_accepted timed out");
  ASSERT_TRUE(_wait_for_flag([&]() { return client_read_.load(); }, 2000) &&
              "client_read (ping) timed out");

  auto _wait_for_snapshot_non_empty = [&]() -> bool {
    const int _step_ms = 5;
    int _waited = 0;
    while (_service->snapshot().empty() && _waited < 1000) {
      std::this_thread::sleep_for(std::chrono::milliseconds(_step_ms));
      _waited += _step_ms;
    }
    return !_service->snapshot().empty();
  };
  ASSERT_TRUE(_wait_for_snapshot_non_empty() &&
              "service snapshot empty (no client connections)");

  const auto _writer = _endpoint->snapshot().front();
  ASSERT_NE(_writer, nullptr);
  std::string _pong = "pong";
  _writer->invoke(_pong);

  ASSERT_TRUE(_wait_for_flag([&]() { return client_write_.load(); }, 2000) &&
              "client_write (pong) timed out");

  _service->stop_clients();

  ASSERT_TRUE(
      _wait_for_flag([&]() { return client_disconnected_.load(); }, 2000) &&
      "client_disconnected timed out");
}
