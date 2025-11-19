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

#include <framework/errors/cancel_error.hpp>
#include <framework/job.hpp>
#include <framework/queue.hpp>
#include <framework/state.hpp>
#include <framework/support.hpp>

using namespace framework;

TEST(test_queue, can_handle_jobs) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");

  std::atomic _task_executed{false};

  _queue->add_task(
      "purchase_order_created",
      [&_task_executed](auto& cancelled, auto& data) -> async_of<void> {
        boost::ignore_unused(cancelled, data);
        _task_executed.store(true, std::memory_order_release);
        co_return;
      });

  const auto _job = _queue->dispatch("purchase_order_created");

  ASSERT_FALSE(_job->started());
  ASSERT_FALSE(_job->finished());
  ASSERT_FALSE(_task_executed.load(std::memory_order_acquire));
  ASSERT_FALSE(_job->started_at() != time_point{});
  ASSERT_FALSE(_job->finished_at() != time_point{});
  ASSERT_FALSE(_job->id().is_nil());

  _state->run();

  ASSERT_TRUE(_job->started());
  ASSERT_TRUE(_job->finished());
  ASSERT_TRUE(_task_executed.load(std::memory_order_acquire));
  ASSERT_TRUE(_job->started_at() != time_point{});
  ASSERT_TRUE(_job->finished_at() != time_point{});
  const auto _duration = _job->finished_at() - _job->started_at();
  ASSERT_GE(_duration.count(), 0);
}

TEST(test_queue, can_handle_multiple_jobs) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");

  atomic_of _first_task_executed{false};
  atomic_of _second_task_executed{false};
  atomic_of _third_task_executed{false};

  _queue->add_task(
      "first_task",
      [&_first_task_executed](auto& cancelled, auto& data) -> async_of<void> {
        boost::ignore_unused(cancelled, data);
        _first_task_executed.store(true, std::memory_order_release);
        co_return;
      });

  _queue->add_task(
      "second_task",
      [&_second_task_executed](auto& cancelled, auto& data) -> async_of<void> {
        boost::ignore_unused(cancelled, data);
        _second_task_executed.store(true, std::memory_order_release);
        co_return;
      });

  _queue->add_task(
      "third_task",
      [&_third_task_executed](auto& cancelled, auto& data) -> async_of<void> {
        boost::ignore_unused(cancelled, data);
        _third_task_executed.store(true, std::memory_order_release);
        co_return;
      });

  const auto _first_job = _queue->dispatch("first_task");
  const auto _second_job = _queue->dispatch("second_task");
  const auto _third_job = _queue->dispatch("third_task");

  _state->run();

  ASSERT_TRUE(_first_job->finished());
  ASSERT_TRUE(_first_task_executed.load(std::memory_order_acquire));
  ASSERT_TRUE(_second_job->finished());
  ASSERT_TRUE(_second_task_executed.load(std::memory_order_acquire));
  ASSERT_TRUE(_third_job->finished());
  ASSERT_TRUE(_third_task_executed.load(std::memory_order_acquire));

  ASSERT_TRUE(_first_job->finished_at() <= _second_job->finished_at());
  ASSERT_TRUE(_second_job->finished_at() <= _third_job->finished_at());
}

TEST(test_queue, can_handle_multiple_jobs_on_multiple_workers) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->set_workers_to(16);

  std::atomic<std::uint64_t> _tasks_executed{0};

  _queue->add_task(
      "item",
      [&_tasks_executed](auto& cancelled, auto& data) -> async_of<void> {
        boost::ignore_unused(cancelled, data);
        _tasks_executed.fetch_add(1, std::memory_order_relaxed);
        co_return;
      });

  for (std::uint32_t i = 0; i < 2048; ++i) {
    _queue->dispatch("item");
  }

  _state->run();
  ASSERT_EQ(_tasks_executed.load(), 2048);
}

TEST(test_queue, can_upscale_and_downscale_workers) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->set_workers_to(16);
  _queue->set_workers_to(16);
  ASSERT_EQ(16, _queue->number_of_workers());
  _queue->set_workers_to(32);
  _queue->set_workers_to(32);
  ASSERT_EQ(32, _queue->number_of_workers());
  _queue->set_workers_to(64);
  _queue->set_workers_to(64);
  ASSERT_EQ(64, _queue->number_of_workers());
  _queue->set_workers_to(32);
  _queue->set_workers_to(32);
  ASSERT_EQ(32, _queue->number_of_workers());
  _queue->set_workers_to(16);
  _queue->set_workers_to(16);
  ASSERT_EQ(16, _queue->number_of_workers());
  _queue->set_workers_to(16);
  _queue->set_workers_to(16);
  ASSERT_EQ(16, _queue->number_of_workers());
}
//
TEST(test_queue, can_be_cancelled) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->set_workers_to(4);

  std::atomic<std::size_t> _jobs_executed;

  _queue->add_task("cancel",
                   [&_queue](auto& cancelled, auto& data) -> async_of<void> {
                     boost::ignore_unused(cancelled, data);
                     _queue->cancel();
                     co_return;
                   });

  _queue->add_task(
      "item", [&_jobs_executed](auto& cancelled, auto& data) -> async_of<void> {
        boost::ignore_unused(cancelled, data);
        _jobs_executed.fetch_add(1, std::memory_order_relaxed);
        co_return;
      });

  _queue->dispatch("cancel");

  for (std::uint32_t i = 0; i < 256; ++i) {
    _queue->dispatch("item");
  }

  _state->run();
  ASSERT_GE(_jobs_executed.load(std::memory_order_relaxed), 0);
  ASSERT_LT(_jobs_executed.load(std::memory_order_relaxed), 256);
  ASSERT_EQ(_queue->number_of_jobs(), 256 + 1);

  std::cout << _jobs_executed.load(std::memory_order_relaxed)
            << " jobs has been processed before cancellation" << std::endl;
}

class custom_exception final : public std::exception {};

TEST(test_queue, can_handle_exceptions) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->set_workers_to(4);

  _queue->add_task("error", [](auto& cancelled, auto& data) -> async_of<void> {
    boost::ignore_unused(cancelled, data);
    throw custom_exception();
    co_return;
  });

  const auto _job = _queue->dispatch("error");

  _state->run();

  ASSERT_TRUE(_job->failed());
  bool _logic_error = false;
  try {
    std::rethrow_exception(_job->exception());
  } catch (const custom_exception&) {
    _logic_error = true;
  }
  ASSERT_TRUE(_logic_error);
}

TEST(test_queue, can_handle_cancellations) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->set_workers_to(4);

  _queue->add_task("cancel", [](auto& cancelled, auto& data) -> async_of<void> {
    boost::ignore_unused(cancelled, data);
    throw errors::cancel_error();
    co_return;
  });

  const auto _job = _queue->dispatch("cancel");

  _state->run();

  ASSERT_FALSE(_job->failed());
  ASSERT_TRUE(_job->cancelled());
}

TEST(test_queue, throw_error_on_undefined_task) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->set_workers_to(4);

  bool _throws = false;

  try {
    const auto _job = _queue->dispatch("cancel");
  } catch (...) {
    _throws = true;
  }

  ASSERT_TRUE(_throws);
}

TEST(test_queue, can_handle_cancellations_on_empty) {
  const auto _state = std::make_shared<state>();
  const auto _queue = _state->get_queue("notifications");
  _queue->cancel();
}
