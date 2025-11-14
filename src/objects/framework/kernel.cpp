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

#include <dotenv.h>

#include <framework/auth.hpp>
#include <framework/controller.hpp>
#include <framework/errors/not_found_error.hpp>
#include <framework/jwt.hpp>
#include <framework/kernel.hpp>
#include <framework/route.hpp>
#include <framework/router.hpp>
#include <framework/state.hpp>
#include <framework/validator.hpp>

namespace framework {
bool authenticated(const shared_state &state, const request_type &request, const shared_auth &auth) {
  using enum http_field;
  try {
    const std::string _bearer{request[authorization]};
    auth->set_jwt(jwt::from(_bearer, state->get_key()));
    return true;
  } catch (...) {
    return false;
  }
}

async_of<message> kernel(const shared_state state, request_type request) noexcept {
  using enum http_field;

  if (request.method() == http_verb::options) {
    auto _verbs = state->get_router()->methods_of(request.target());
    const auto _methods = boost::join(_verbs, ",");
    response_empty_type _response{http_status::no_content, request.version()};
    _response.set(access_control_allow_methods, _methods.empty() ? "" : _methods);
    _response.set(access_control_allow_headers, "Accept,Authorization,Content-Type");
    _response.set(access_control_allow_origin, "*");
    co_return _response;
  }

  try {
    auto [_params, _route] = state->get_router()->find(request.method(), request.target());
    auto _controller = _route->get_controller();

    if (_controller->config().validated_) {
      boost::system::error_code _parse_ec;
      auto _payload = boost::json::parse(request.body(), _parse_ec);
      if (_parse_ec) {
        response_type _response{http_status::unprocessable_entity, request.version()};
        _response.set(access_control_allow_origin, "*");
        _response.body() = serialize(
            object({{"message", "The given data was invalid."}, {"errors", {{"*", array({"The payload must be a valid json value."})}}}}));
        _response.prepare_payload();
        co_return _response;
      }

      if (auto _validator = validator::make(_controller->config().validation_rules_, _payload); !_validator->get_success()) {
        response_type _response{http_status::unprocessable_entity, request.version()};
        _response.set(access_control_allow_origin, "*");
        _response.body() = serialize(object({{"message", "The given data was invalid."}, {"errors", _validator->get_errors()}}));
        _response.prepare_payload();
        co_return _response;
      }
    }

    auto _auth = std::make_shared<auth>();
    if (_controller->config().authenticated_) {
      if (request[authorization].empty()) {
        response_empty_type _response{http_status::unauthorized, request.version()};
        _response.set(access_control_allow_origin, "*");
        _response.prepare_payload();
        co_return _response;
      }
      if (!authenticated(state, request, _auth)) {
        response_empty_type _response{http_status::unauthorized, request.version()};
        _response.set(access_control_allow_origin, "*");
        _response.prepare_payload();
        co_return _response;
      }
    }
    auto _response = co_await _controller->callback()(state, std::move(request), _params, _auth);
    _response.set(access_control_allow_origin, "*");
    co_return _response;
  } catch (const errors::not_found_error &) {
    response_empty_type _response{http_status::not_found, request.version()};
    _response.set(access_control_allow_origin, "*");
    _response.prepare_payload();
    co_return _response;
  } catch (...) {
    response_empty_type _response{http_status::internal_server_error, request.version()};
    _response.set(access_control_allow_origin, "*");
    _response.prepare_payload();
    co_return _response;
  }
}
}  // namespace framework
