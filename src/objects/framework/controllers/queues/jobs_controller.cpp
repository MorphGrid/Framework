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
#include <framework/controllers/queues/jobs_controller.hpp>
#include <framework/job.hpp>
#include <framework/queue.hpp>
#include <framework/state.hpp>
#include <framework/task.hpp>

namespace framework::controllers::queues {
vector_of<http_verb> jobs_controller::verbs() {
  return vector_of{
      http_verb::get,
  };
}

shared_controller jobs_controller::make() {
  return std::make_shared<controller>(
      [](const shared_state state, const request_type request,
         const params_type params,
         const shared_auth auth) -> async_of<response_type> {
        const auto _queue_name = params.at("queue_name");
        if (!state->queue_exists(_queue_name)) {
          response_empty_type _response{http_status::not_found,
                                        request.version()};
          _response.prepare_payload();
          co_return _response;
        }

        const auto _queue = state->get_queue(_queue_name);
        array _jobs;
        _jobs.reserve(_queue->number_of_jobs());

        for (auto const &[_id, _job] : _queue->get_jobs()) {
          _jobs.push_back(object({
              {"id", to_string(_id)},
              {"task_id", to_string(_job->get_task()->get_id())},
          }));
        }

        response_type _response{http_status::ok, request.version()};
        const object _data = {{"data", _jobs}};
        _response.body() = serialize(_data);
        _response.prepare_payload();
        co_return _response;
      },
      controller_config{.authenticated_ = true});
}
}  // namespace framework::controllers::queues
