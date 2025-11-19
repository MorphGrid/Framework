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

#ifndef FRAMEWORK_JWT_HPP
#define FRAMEWORK_JWT_HPP

#include <framework/support.hpp>

namespace framework {
class jwt : public std::enable_shared_from_this<jwt> {
  uuid id_;
  uuid sub_;
  std::string header_;
  object payload_;
  std::string signature_;

 public:
  jwt(uuid id, uuid sub, std::string header, object payload,
      std::string signature);

  std::string as_string() const;

  uuid get_id() const;
  uuid get_sub() const;
  object get_payload() const;
  std::string get_signature() const;

  static shared_jwt make(uuid id, const std::string &key);
  static shared_jwt from(const std::string_view &bearer,
                         const std::string &key);
};
}  // namespace framework

#endif  // FRAMEWORK_JWT_HPP
