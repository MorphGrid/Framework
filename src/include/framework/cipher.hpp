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

#ifndef FRAMEWORK_CIPHER_HPP
#define FRAMEWORK_CIPHER_HPP

#include <framework/support.hpp>

namespace framework {
std::string generate_sha_256();

std::string hmac(const std::string& input, const std::string_view& app_key);

pair_of<std::string, std::string> generate_aes_key_iv();

std::string encrypt(const std::string& input, const std::string& key,
                    const std::string& iv);

std::string decrypt(std::string_view input, const std::string& key,
                    const std::string& iv);

inline bool password_validator(const std::string& input,
                               const std::string& hash) {
  return BCrypt::validatePassword(input, hash);
}

inline std::string password_hash(const std::string& input,
                                 const int workload = 12) {
  return BCrypt::generateHash(input, workload);
}
}  // namespace framework

#endif  // FRAMEWORK_CIPHER_HPP
