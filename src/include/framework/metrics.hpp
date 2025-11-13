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

#ifndef FRAMEWORK_METRICS_HPP
#define FRAMEWORK_METRICS_HPP

#include <framework/support.hpp>

namespace framework {
class metrics : public std::enable_shared_from_this<metrics> {
 public:
  std::atomic<std::size_t> _requests{0};
};
}  // namespace framework

#endif  // FRAMEWORK_METRICS_HPP
