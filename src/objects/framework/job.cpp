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

#include <framework/errors/cancel_error.hpp>
#include <framework/job.hpp>
#include <framework/task.hpp>

namespace framework {
job::job(const shared_task& task, object data) : task_(task), data_(std::move(data)) {}

uuid job::id() const noexcept { return id_; }

shared_task job::get_task() const noexcept { return task_; }

bool job::started() const noexcept { return started_.load(std::memory_order_acquire); }

bool job::failed() const noexcept { return failed_.load(std::memory_order_acquire); }

bool job::finished() const noexcept { return finished_.load(std::memory_order_acquire); }

bool job::cancelled() const noexcept { return cancelled_.load(std::memory_order_acquire); }

void job::mark_as_started() noexcept {
  started_.store(true, std::memory_order_release);
  started_at_ = std::chrono::system_clock::now();
}

void job::mark_as_cancelled() noexcept {
  cancelled_.store(true, std::memory_order_release);
  cancelled_at_ = std::chrono::system_clock::now();
}

void job::mark_as_failed(const std::exception_ptr& exception) noexcept {
  failed_.store(true, std::memory_order_release);
  exception_ = exception;
}

void job::mark_as_finished() noexcept {
  finished_.store(true, std::memory_order_release);
  finished_at_ = std::chrono::system_clock::now();
}

void job::throw_if_cancelled() const {
  if (cancelled_.load(std::memory_order_acquire)) {
    throw errors::cancel_error();
  }
}

std::exception_ptr job::exception() const noexcept { return exception_; }

time_point job::started_at() const noexcept { return started_at_; }

time_point job::finished_at() const noexcept { return finished_at_; }

async_of<void> job::run() {
  mark_as_started();
  try {
    throw_if_cancelled();
    co_await (*task_->callback())(cancelled_, data_);
  } catch (errors::cancel_error&) {
    mark_as_cancelled();
  } catch (...) {
    mark_as_failed(std::current_exception());
  }
  mark_as_finished();
  co_return;
}

void job::cancel() noexcept { mark_as_cancelled(); }
}  // namespace framework
