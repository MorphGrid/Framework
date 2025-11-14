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

#include <framework/auth.hpp>
#include <framework/controller.hpp>
#include <framework/errors/parse_error.hpp>
#include <framework/route.hpp>
#include <framework/state.hpp>
#include <framework/support.hpp>

using namespace framework;

TEST(test_route, can_be_instanced) {
  auto _controller = std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                                     const shared_auth auth) -> async_of<response_type> {
    response_empty_type _response{http_status::ok, request.version()};
    _response.prepare_payload();
    co_return _response;
  });

  route _route(
      {
          http_verb::get,
          http_verb::post,
      },
      "/hello-world",
      std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                      const shared_auth auth) -> async_of<response_type> {
        response_empty_type _response{http_status::ok, request.version()};
        _response.prepare_payload();
        co_return _response;
      }));

  ASSERT_EQ(_route.get_parameters().size(), 0);
  ASSERT_EQ(_route.get_verbs().size(), 2);
}

TEST(test_route, can_be_compiled) {
  auto _controller = std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                                     const shared_auth auth) -> async_of<response_type> {
    response_empty_type _response{http_status::ok, request.version()};
    _response.prepare_payload();
    co_return _response;
  });

  route _route(
      {
          http_verb::get,
          http_verb::post,
      },
      "/api/users/{user_id}/configurations",
      std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                      const shared_auth auth) -> async_of<response_type> {
        response_empty_type _response{http_status::ok, request.version()};
        _response.prepare_payload();
        co_return _response;
      }));

  ASSERT_EQ(_route.get_parameters().size(), 1);
  ASSERT_EQ(_route.get_verbs().size(), 2);
}

TEST(test_route, can_be_invoked) {
  boost::asio::io_context _ioc;

  auto _controller = std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                                     const shared_auth auth) -> async_of<response_type> {
    response_empty_type _response{http_status::ok, request.version()};
    _response.prepare_payload();
    co_return _response;
  });

  auto _executed = std::make_shared<atomic_of<bool>>(false);

  route _route(
      {
          http_verb::get,
          http_verb::post,
      },
      "/endpoint",
      std::make_shared<controller>([&_executed](const shared_state state, const request_type request, params_type params,
                                                shared_auth auth) -> async_of<response_type> {
        response_empty_type _response{http_status::ok, request.version()};
        std::cout << "Inside of controller ..." << std::endl;
        _response.prepare_payload();
        _executed->store(true, std::memory_order_release);
        co_return _response;
      }));

  auto _callback = _route.get_controller()->callback();

  request_type _request{http_verb::get, "/api/status", 11};
  _request.set(http_field::host, "127.0.0.1");
  _request.set(http_field::user_agent, "Client");
  _request.prepare_payload();

  const auto _state = std::make_shared<state>();
  const auto _auth = std::make_shared<auth>();
  params_type _params;
  co_spawn(make_strand(_ioc), (_callback)(_state, _request, _params, _auth), boost::asio::detached);
  _ioc.run();
  ASSERT_TRUE(_executed->load(std::memory_order_acquire));
}

TEST(test_route, can_be_matched) {
  auto _controller = std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                                     const shared_auth auth) -> async_of<response_type> {
    response_empty_type _response{http_status::ok, request.version()};
    _response.prepare_payload();
    co_return _response;
  });

  auto _executed = std::make_shared<atomic_of<bool>>(false);

  route _route(
      {
          http_verb::get,
          http_verb::post,
      },
      "/parameters/{1}/{2}/{3}",
      std::make_shared<controller>(
          [&_executed](const shared_state state, const request_type request, params_type, shared_auth auth) -> async_of<response_type> {
            response_empty_type _response{http_status::ok, request.version()};
            std::cout << "Inside of controller ..." << std::endl;
            _response.prepare_payload();
            _executed->store(true, std::memory_order_release);
            co_return _response;
          }));

  auto [_matched, _parameters] = _route.match("/parameters/4/5/6");
  ASSERT_TRUE(_matched);
  ASSERT_TRUE(_parameters.contains("1"));
  ASSERT_TRUE(_parameters.contains("2"));
  ASSERT_TRUE(_parameters.contains("3"));
  ASSERT_FALSE(_parameters.contains("4"));
  ASSERT_FALSE(_parameters.contains("5"));
  ASSERT_FALSE(_parameters.contains("6"));
  ASSERT_EQ(_parameters.at("1"), "4");
  ASSERT_EQ(_parameters.at("2"), "5");
  ASSERT_EQ(_parameters.at("3"), "6");

  auto [_not_matches, _] = _route.match("/parameters/7/8");
  ASSERT_FALSE(_not_matches);
}

TEST(test_route, throw_error_on_duplicated_parameters) {
  auto _controller = std::make_shared<controller>([](const shared_state state, const request_type request, const params_type params,
                                                     const shared_auth auth) -> async_of<response_type> {
    response_empty_type _response{http_status::ok, request.version()};
    _response.prepare_payload();
    co_return _response;
  });

  auto _executed = std::make_shared<atomic_of<bool>>(false);

  bool throws = false;

  try {
    route _route(
        {
            http_verb::get,
            http_verb::post,
        },
        "/parameters/{1}/{1}/{1}",
        std::make_shared<controller>([&_executed](const shared_state state, const request_type request, params_type params,
                                                  shared_auth auth) -> async_of<response_type> {
          response_type _response{http_status::ok, request.version()};
          std::cout << "Inside of controller ..." << std::endl;
          _response.prepare_payload();
          _executed->store(true, std::memory_order_release);
          co_return _response;
        }));
  } catch (const errors::parse_error &e) {
    std::cout << e.what() << std::endl;
    throws = true;
  }

  ASSERT_TRUE(throws);
}
