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

#include <framework/cipher.hpp>
#include <framework/controller.hpp>
#include <framework/controllers/auth/attempt_controller.hpp>
#include <framework/jwt.hpp>
#include <framework/state.hpp>

namespace framework::controllers::auth {
vector_of<http_verb> attempt_controller::verbs() {
  return vector_of{
      http_verb::post,
  };
}

shared_controller attempt_controller::make() {
  return std::make_shared<controller>(
      [](const shared_state state, const request_type request, const params_type params,
         const shared_auth auth) -> async_of<response_type> {
        auto _body = boost::json::parse(request.body());
        std::string _email{_body.as_object().at("email").as_string()};
        std::string _password{_body.as_object().at("password").as_string()};

        boost::mysql::pooled_connection _connection =
            co_await state->get_connection_pool()->async_get_connection(boost::asio::cancel_after(std::chrono::seconds(30)));

        boost::mysql::results _result;

        co_await _connection->async_execute(boost::mysql::with_params("SELECT id, password FROM users WHERE email = {}", _email), _result);

        if (_result.rows().size() == 0) {
          response_type _response{http_status::unprocessable_entity, request.version()};
          _response.body() = serialize(
              object({{"message", "The given data was invalid."}, {"errors", {{"email", array{"The email isn't registered."}}}}}));
          _response.prepare_payload();
          _connection.return_without_reset();
          co_return _response;
        }

        if (std::string _hash{_result.rows().at(0).at(1).as_string()}; !password_validator(_password, _hash)) {
          response_type _response{http_status::unprocessable_entity, request.version()};
          _response.body() = serialize(
              object({{"message", "The given data was invalid."}, {"errors", {{"password", array{"The password is incorrect."}}}}}));
          _response.prepare_payload();
          _connection.return_without_reset();
          co_return _response;
        }

        auto _id = boost::lexical_cast<uuid>(_result.rows().at(0).at(0));
        const auto _jwt = jwt::make(_id, state->get_key());
        response_type _response{http_status::ok, request.version()};
        const object _data = {{"data", {{"token", _jwt->as_string()}}}};
        _response.body() = serialize(_data);
        _response.prepare_payload();
        _connection.return_without_reset();
        co_return _response;
      },
      controller_config{.validated_ = true,
                        .validation_rules_ = {
                            {"*", "is_object"},
                            {"email", "is_string"},
                            {"password", "is_string"},
                        }});
}
}  // namespace framework::controllers::auth
