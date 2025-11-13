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

#ifndef FRAMEWORK_AUTH_HPP
#define FRAMEWORK_AUTH_HPP

#include <framework/support.hpp>

namespace framework {
class auth : public std::enable_shared_from_this<auth> {
  optional_of<shared_jwt> jwt_;

 public:
  void set_jwt(const shared_jwt &jwt);
  optional_of<shared_jwt> get_jwt();
};
}  // namespace framework

#endif  // FRAMEWORK_AUTH_HPP
