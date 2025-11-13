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

#include <framework/state.hpp>
#include <framework/support.hpp>

using namespace framework;

TEST(test_state, can_create_shared_instance) {
  const auto _state = std::make_shared<state>();
  ASSERT_NE(_state.get(), nullptr);
}

TEST(test_state, can_manage_queues) {
  const auto _state = std::make_shared<state>();
  std::string _channel = "notifications";

  // State persists queue after scope exit.
  {
    const auto _queue = _state->get_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 1);
    ASSERT_NE(_queue.get(), nullptr);
    ASSERT_TRUE(_state->queue_exists(_channel));
  }
  ASSERT_TRUE(_state->queue_exists(_channel));

  // State can remove queue after scope exit.
  {
    ASSERT_EQ(_state->queues().size(), 1);
    _state->remove_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 0);
    ASSERT_FALSE(_state->queue_exists(_channel));
  }

  // State can remove a local scoped queue
  {
    const auto _queue = _state->get_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 1);
    ASSERT_NE(_queue.get(), nullptr);
    ASSERT_TRUE(_state->queue_exists(_channel));
    _state->remove_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 0);
    ASSERT_FALSE(_state->queue_exists(_channel));
    ASSERT_NE(_queue.get(), nullptr);
  }

  // State creates queue on get.
  {
    const std::string _on_missing_create = "ops";
    ASSERT_FALSE(_state->queue_exists(_on_missing_create));
    ASSERT_NE(_state->get_queue(_on_missing_create).get(), nullptr);
    ASSERT_EQ(_state->queues().size(), 1);
    _state->remove_queue(_on_missing_create);
    ASSERT_EQ(_state->queues().size(), 0);
  }

  // State handles unique queues.
  {
    const auto _queue = _state->get_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 1);
    ASSERT_NE(_queue.get(), nullptr);

    const auto _same_queue = _state->get_queue(_channel);
    ASSERT_EQ(_same_queue.get(), _queue.get());
    _state->remove_queue(_channel);
  }

  // State returns existing queue on add.
  {
    const auto _queue = _state->get_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 1);
    ASSERT_NE(_queue.get(), nullptr);

    const auto _same_queue = _state->get_queue(_channel);
    ASSERT_EQ(_state->queues().size(), 1);
    ASSERT_EQ(_same_queue.get(), _queue.get());
    _state->remove_queue(_channel);
  }

  // State copies the name of queues.
  {
    std::string _channel_name = "developers";
    const auto _queue = _state->get_queue(_channel_name);
    ASSERT_EQ(_state->queues().size(), 1);
    ASSERT_NE(_queue.get(), nullptr);
  }
  ASSERT_TRUE(_state->queue_exists("developers"));
}
