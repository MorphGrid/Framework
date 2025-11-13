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

#include <framework/chrono.hpp>
#include <framework/cipher.hpp>
#include <framework/encoding.hpp>
#include <framework/errors/parse_error.hpp>
#include <framework/errors/signature_error.hpp>
#include <framework/jwt.hpp>

namespace framework {
jwt::jwt(const uuid id, const uuid sub, std::string header, object payload, std::string signature)
    : id_(id), sub_(sub), header_(std::move(header)), payload_(std::move(payload)), signature_(std::move(signature)) {}

std::string jwt::as_string() const {
  return "Bearer " + base64url_encode(header_, false) + "." + base64url_encode(serialize(payload_), false) + "." + signature_;
}

uuid jwt::get_id() const { return id_; }

uuid jwt::get_sub() const { return sub_; }

object jwt::get_payload() const { return payload_; }

std::string jwt::get_signature() const { return signature_; }

shared_jwt jwt::make(uuid id, const std::string &key) {
  const std::string _header = R"({"alg":"HS256","typ":"JWT"})";
  const auto _jti = boost::uuids::random_generator()();
  const object _payload = {
      {"sub", to_string(id)},
      {"iat", now()},
      {"jti", to_string(_jti)},
  };
  const std::string _payload_string = serialize(_payload);
  std::string _signature = base64url_encode(hmac(base64url_encode(_header) + "." + base64url_encode(_payload_string), key), false);
  return std::make_shared<jwt>(_jti, id, _header, _payload, _signature);
}

shared_jwt jwt::from(const std::string_view &bearer, const std::string &key) {
  std::string _bearer{bearer.begin(), bearer.end()};
  static constexpr std::string_view _prefix = "Bearer ";
  const std::string _header = R"({"alg":"HS256","typ":"JWT"})";

  if (bearer.starts_with(_prefix)) {
    _bearer = _bearer.substr(_prefix.size());
  }

  vector_of<std::string> _parts;
  for (auto _part : std::views::split(_bearer, '.')) {
    _parts.emplace_back(_part.begin(), _part.end());
  }

  if (_parts.size() != 3) throw errors::parse_error("JWT token doesn't contains 3 parts.");

  const std::string _challenge = _parts[0] + "." + _parts[1];
  const std::string _signature = base64url_encode(hmac(_challenge, key), false);

  if (_parts[2] != _signature) throw errors::signature_error("Token doesn't matches");

  boost::system::error_code _parse_ec;
  auto _payload = boost::json::parse(base64url_decode(_parts.at(1)), _parse_ec);

  if (_parse_ec) throw errors::parse_error("JWT payload isn't valid JSON.");

  if (!_payload.is_object() || !_payload.as_object().contains("sub") || !_payload.as_object().contains("iat") ||
      !_payload.as_object().contains("jti"))
    throw errors::parse_error("JWT payload doesn't contains required fields.");

  if (!_payload.as_object().at("sub").is_string() || !_payload.as_object().at("iat").is_int64() ||
      !_payload.as_object().at("jti").is_string())
    throw errors::parse_error("JWT payload doesn't contains required fields data types.");

  std::string _jti{_payload.as_object().at("jti").as_string()};
  std::string _sub{_payload.as_object().at("sub").as_string()};
  return std::make_shared<jwt>(boost::lexical_cast<uuid>(_jti), boost::lexical_cast<uuid>(_sub), _header, _payload.as_object(), _signature);
}
}  // namespace framework
