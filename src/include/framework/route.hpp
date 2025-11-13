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

#ifndef FRAMEWORK_ROUTE_HPP
#define FRAMEWORK_ROUTE_HPP

#include <framework/support.hpp>

namespace framework {
class route : public std::enable_shared_from_this<route> {
  std::string signature_;
  shared_of<std::regex> expression_;
  shared_controller controller_;
  vector_of<http_verb> verbs_;
  vector_of<std::string> parameters_;

 public:
  explicit route(vector_of<http_verb> verbs, std::string signature, const shared_controller &controller);
  void compile();
  shared_of<std::regex> &get_expression();
  shared_controller &get_controller();
  vector_of<http_verb> &get_verbs();
  vector_of<std::string> &get_parameters();
  pair_of<bool, params_type> match(const std::string &input);
};
}  // namespace framework

#endif  // FRAMEWORK_ROUTE_HPP
