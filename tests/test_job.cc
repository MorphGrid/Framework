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

#include <framework/job.hpp>
#include <framework/support.hpp>
#include <framework/task.hpp>

using namespace framework;

TEST(test_job, can_run) {
  boost::asio::io_context _ioc;
  atomic_of _executed{false};
  const auto _task = std::make_shared<task>([&_executed](auto& cancelled, auto& data) -> async_of<void> {
    boost::ignore_unused(cancelled, data);
    _executed.store(true, std::memory_order_release);
    co_return;
  });
  const auto _job = std::make_shared<job>(_task, object{});
  auto fut = co_spawn(make_strand(_ioc), _job->run(), boost::asio::use_future);
  _ioc.run();
  fut.get();
  ASSERT_TRUE(_executed.load(std::memory_order_acquire));
}

TEST(test_job, run_is_promise) {
  boost::asio::io_context _ioc;
  atomic_of _executed{false};
  const auto _task = std::make_shared<task>([&_executed](auto& cancelled, auto& data) -> async_of<void> {
    boost::ignore_unused(cancelled, data);
    _executed.store(true, std::memory_order_release);
    co_return;
  });
  const auto _job = std::make_shared<job>(_task, object{});
  const auto _promise = _job->run();
  ASSERT_FALSE(_executed.load(std::memory_order_acquire));
  atomic_of<bool> _cancelled;
  auto _result = (*_task->callback())(_cancelled, object{});
  ASSERT_FALSE(_executed.load(std::memory_order_acquire));

  const auto _running_job = [&_job]() -> async_of<void> {
    co_await _job->run();
    co_return;
  };

  auto fut = co_spawn(_ioc, _running_job, boost::asio::use_future);
  _ioc.run();
  fut.get();

  ASSERT_TRUE(_job->finished());
}
