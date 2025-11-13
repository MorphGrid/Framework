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

#ifndef FRAMEWORK_ROUTER_HPP
#define FRAMEWORK_ROUTER_HPP

#include <framework/support.hpp>

namespace framework {
class router : public std::enable_shared_from_this<router> {
  vector_of<shared_route> routes_;

 public:
  vector_of<shared_route> get_routes() const;
  shared_router add(shared_route route);
  tuple_of<params_type, shared_route> find(http_verb verb, const std::string &path) const;
  vector_of<std::string> methods_of(const std::string &path) const;
};
}  // namespace framework

#endif  // FRAMEWORK_ROUTER_HPP
