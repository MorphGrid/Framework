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

#include <framework/controller.hpp>
#include <framework/controllers/status_controller.hpp>

namespace framework::controllers {
vector_of<http_verb> status_controller::verbs() {
  return vector_of{
      http_verb::get,
  };
}

shared_controller status_controller::make() {
  return std::make_shared<controller>([](const shared_state &state, const request_type request, const params_type &params,
                                         const shared_auth &auth) -> async_of<response_type> {
    response_empty_type _response{http_status::ok, request.version()};
    _response.prepare_payload();
    co_return _response;
  });
}
}  // namespace framework::controllers
