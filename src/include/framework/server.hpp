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

#ifndef FRAMEWORK_SERVER_HPP
#define FRAMEWORK_SERVER_HPP

#include <framework/state.hpp>
#include <framework/support.hpp>

namespace framework {
class server : public std::enable_shared_from_this<server> {
  shared_state state_ = std::make_shared<state>();
  shared_of<task_group> task_group_;

 public:
  server();
  void start(unsigned short int port = 0);
  shared_state get_state() const;
  shared_of<task_group> get_task_group();
};
}  // namespace framework

#endif  // FRAMEWORK_SERVER_HPP
