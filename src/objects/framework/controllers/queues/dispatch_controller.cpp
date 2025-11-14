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
#include <framework/controllers/queues/dispatch_controller.hpp>
#include <framework/errors/task_not_found.hpp>
#include <framework/queue.hpp>
#include <framework/state.hpp>

namespace framework::controllers::queues {
vector_of<http_verb> dispatch_controller::verbs() {
  return vector_of{
      http_verb::post,
  };
}

shared_controller dispatch_controller::make() {
  return std::make_shared<controller>(
      [](const shared_state state, const request_type request, const params_type params,
         const shared_auth auth) -> async_of<response_type> {
        const auto _queue_name = params.at("queue_name");
        auto _body = boost::json::parse(request.body());
        std::string _task{_body.as_object().at("task").as_string()};
        if (!state->queue_exists(_queue_name)) {
          response_empty_type _response{http_status::not_found, request.version()};
          _response.prepare_payload();
          co_return _response;
        }

        const auto _queue = state->get_queue(_queue_name);

        try {
          _queue->dispatch(_task, _body.at("data").as_object());
          response_empty_type _response{http_status::ok, request.version()};
          _response.prepare_payload();
          co_return _response;
        } catch (const errors::task_not_found &e) {
          response_empty_type _response{http_status::not_found, request.version()};
          _response.prepare_payload();
          co_return _response;
        }
      },
      controller_config{.authenticated_ = true,
                        .validated_ = true,
                        .validation_rules_ = {
                            {"*", "is_object"},
                            {"task", "is_string"},
                            {"data", "is_object"},
                        }});
}
}  // namespace framework::controllers::queues
