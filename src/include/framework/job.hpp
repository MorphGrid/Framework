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

#ifndef FRAMEWORK_JOB_HPP
#define FRAMEWORK_JOB_HPP

#include <framework/support.hpp>

namespace framework {
class job : public std::enable_shared_from_this<job> {
  uuid id_ = boost::uuids::random_generator()();
  shared_task task_;
  atomic_of<bool> started_{false};
  atomic_of<bool> cancelled_{false};
  atomic_of<bool> finished_{false};
  atomic_of<bool> failed_{false};
  std::exception_ptr exception_;
  time_point started_at_;
  time_point cancelled_at_;
  time_point finished_at_;
  object data_;

 public:
  explicit job(const shared_task& task, object data);
  uuid id() const noexcept;
  shared_task get_task() const noexcept;
  bool started() const noexcept;
  bool failed() const noexcept;
  bool finished() const noexcept;
  bool cancelled() const noexcept;
  void mark_as_started() noexcept;
  void mark_as_cancelled() noexcept;
  void mark_as_failed(const std::exception_ptr& exception) noexcept;
  void mark_as_finished() noexcept;
  void throw_if_cancelled() const;
  std::exception_ptr exception() const noexcept;
  time_point started_at() const noexcept;
  time_point finished_at() const noexcept;
  async_of<void> run() noexcept;
  void cancel() noexcept;
};
}  // namespace framework

#endif  // FRAMEWORK_JOB_HPP
