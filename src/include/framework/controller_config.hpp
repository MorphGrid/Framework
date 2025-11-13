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

#ifndef FRAMEWORK_CONTROLLER_CONFIG_HPP
#define FRAMEWORK_CONTROLLER_CONFIG_HPP

#include <framework/support.hpp>

namespace framework {
struct controller_config {
  bool authenticated_{false};
  bool validated_{false};
  map_of<std::string, std::string> validation_rules_{};
};
}  // namespace framework

#endif  // FRAMEWORK_CONTROLLER_CONFIG_HPP
