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

#ifndef FRAMEWORK_STRING_HASHER_HPP
#define FRAMEWORK_STRING_HASHER_HPP

#include <string>

struct string_hasher {
  using is_transparent = void;
  std::size_t operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
  std::size_t operator()(std::string const& s) const noexcept { return operator()(std::string_view{s}); }
  std::size_t operator()(char const* s) const noexcept { return operator()(std::string_view{s}); }
};

#endif  // FRAMEWORK_STRING_HASHER_HPP
