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
#include <framework/errors/not_found_error.hpp>
#include <framework/route.hpp>
#include <framework/router.hpp>
#include <framework/support.hpp>

using namespace framework;

TEST(test_router, can_resolve_requests) {
  const auto _router = std::make_shared<router>();

  _router
      ->add(std::make_shared<route>(
          vector_of{
              http_verb::post,
          },
          "/transactions/{id}",
          std::make_shared<controller>([](const shared_state &state, const request_type request, const params_type &params,
                                          const shared_auth &auth) -> async_of<response_type> {
            response_empty_type _response{http_status::ok, request.version()};
            _response.prepare_payload();
            co_return _response;
          })))
      ->add(std::make_shared<route>(
          vector_of{
              http_verb::delete_,
          },
          "/users/{id}",
          std::make_shared<controller>([](const shared_state &state, const request_type request, const params_type &params,
                                          const shared_auth &auth) -> async_of<response_type> {
            response_empty_type _response{http_status::ok, request.version()};
            _response.prepare_payload();
            co_return _response;
          })));

  {
    auto [_params, _route] = _router->find(http_verb::delete_, "/users/5");
    ASSERT_EQ(_params.size(), 1);
    ASSERT_EQ(_params.at("id"), "5");
  }

  {
    bool throws = false;
    try {
      auto [_params, _route] = _router->find(http_verb::post, "/users/5");
    } catch (const errors::not_found_error &) {
      throws = true;
    }
    ASSERT_TRUE(throws);
  }

  {
    auto [_params, _route] = _router->find(http_verb::post, "/transactions/7");
    ASSERT_EQ(_params.size(), 1);
    ASSERT_EQ(_params.at("id"), "7");
  }

  {
    bool throws = false;
    try {
      auto [_params, _route] = _router->find(http_verb::delete_, "/transactions/5");
    } catch (const errors::not_found_error &) {
      throws = true;
    }
    ASSERT_TRUE(throws);
  }

  {
    bool throws = false;
    try {
      auto [_params, _route] = _router->find(http_verb::post, "/not_found");
    } catch (const errors::not_found_error &) {
      throws = true;
    }
    ASSERT_TRUE(throws);
  }
}