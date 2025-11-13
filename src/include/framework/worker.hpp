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

#pragma once

#ifndef FRAMEWORK_WORKER_HPP
#define FRAMEWORK_WORKER_HPP

#include <framework/support.hpp>

namespace framework {
class worker : public std::enable_shared_from_this<worker> {
  uuid id_ = boost::uuids::random_generator()();
  strand_of<boost::asio::io_context::executor_type> strand_;
  atomic_of<std::uint64_t> number_of_tasks_;

  static async_of<void> run(shared_job job);

 public:
  explicit worker(strand_of<boost::asio::io_context::executor_type> strand);
  const uuid& id() const noexcept;
  std::uint64_t number_of_tasks() const noexcept;
  shared_job dispatch(const shared_task& task, object data);
};
}  // namespace framework

#endif  // FRAMEWORK_WORKER_HPP
