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
#include <framework/controllers/queues/index_controller.hpp>
#include <framework/jwt.hpp>
#include <framework/queue.hpp>
#include <framework/state.hpp>

namespace framework::controllers::queues {
vector_of<http_verb> index_controller::verbs() {
  return vector_of{
      http_verb::get,
  };
}

shared_controller index_controller::make() {
  return std::make_shared<controller>(
      [](const shared_state &state, const request_type request, const params_type &params,
         const shared_auth &auth) -> async_of<response_type> {
        array _queues;
        _queues.reserve(state->queues().size());

        for (auto &[_name, _queue] : state->queues()) {
          _queues.push_back(object({{"id", to_string(_queue->get_id())}, {"name", _name}}));
        }

        response_type _response{http_status::ok, request.version()};
        const object _data = {{"data", _queues}};
        _response.body() = serialize(_data);
        _response.prepare_payload();
        co_return _response;
      },
      controller_config{.authenticated_ = true});
}
}  // namespace framework::controllers::queues
