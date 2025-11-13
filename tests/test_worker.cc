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

#include <framework/support.hpp>
#include <framework/task.hpp>
#include <framework/worker.hpp>

using namespace framework;

TEST(test_worker, can_run) {
  boost::asio::io_context _ioc;
  std::atomic _executed{false};
  const auto _task = std::make_shared<task>([&_executed](auto& cancelled, auto& data) -> async_of<void> {
    boost::ignore_unused(cancelled, data);
    _executed.store(true, std::memory_order_release);
    co_return;
  });
  const auto _worker = std::make_shared<worker>(make_strand(_ioc));
  auto fut = co_spawn(
      _ioc,
      [&_worker, &_task]() -> async_of<void> {
        _worker->dispatch(_task, object{});
        co_return;
      },
      boost::asio::use_future);
  _ioc.run();
  fut.get();
  ASSERT_TRUE(_executed.load(std::memory_order_acquire));
}

TEST(test_worker, can_be_instanced) {
  boost::asio::io_context _ioc;
  const worker _worker(make_strand(_ioc));
  ASSERT_EQ(_worker.number_of_tasks(), 0);
}
