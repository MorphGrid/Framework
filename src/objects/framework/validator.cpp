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

#include <framework/validator.hpp>

namespace framework {
bool validator::per_rule(const value &value, const std::string &attribute, const std::string &rule) {
  if (attribute == "*") {
    if (!value.is_object()) {
      const std::string _error_message = "Message must be an JSON object.";
      this->insert_or_push(attribute, _error_message);
      return true;
    }
    return false;
  }

  vector_of<std::string> _scoped_rules;
  split(_scoped_rules, rule, boost::is_any_of(","), boost::token_compress_off);

  for (const std::string &_scoped_rule : _scoped_rules) {
    if (const auto should_break = this->per_scope_rule(value, attribute, _scoped_rule); should_break) {
      break;
    }
  }
  return false;
}

bool validator::per_scope_rule(const value &value, const std::string &attribute, const std::string_view &rule) {
  if (!value.as_object().contains(attribute) && rule != "nullable") {
    const std::string _error_message = "Attribute " + attribute + " is required.";
    this->insert_or_push(attribute, _error_message);
    return true;
  }

  if (rule == "is_string") {
    this->on_string_rule(value, attribute);
  } else if (rule == "is_uuid") {
    this->on_uuid_rule(value, attribute);
  } else if (rule == "confirmed") {
    this->on_confirmation_rule(value, attribute);
  } else if (rule == "is_object") {
    this->on_object_rule(value, attribute);
  } else if (rule == "is_number") {
    this->on_number_rule(value, attribute);
  } else if (rule == "is_array_of_strings") {
    this->on_array_of_strings_rule(value, attribute);
  }

  return false;
}

void validator::on_confirmation_rule(const value &value, const std::string &attribute) {
  if (!value.as_object().contains(attribute + "_confirmation")) {
    const std::string _error_message = "Attribute " + attribute + "_confirmation" + " must be present.";
    this->insert_or_push(attribute, _error_message);
  } else {
    if (!value.as_object().at(attribute + "_confirmation").is_string()) {
      const std::string _error_message = "Attribute " + attribute + "_confirmation" + " must be string.";
      this->insert_or_push(attribute, _error_message);
    } else {
      const std::string value_{value.as_object().at(attribute).as_string()};
      const std::string value_confirmation_{value.as_object().at(attribute + "_confirmation").as_string()};
      if (value_ != value_confirmation_) {
        const std::string _error_message = "Attribute " + attribute + " and " + attribute + "_confirmation" + " must be equals.";
        this->insert_or_push(attribute, _error_message);
      }
    }
  }
}

void validator::on_array_of_strings_rule(const value &value, const std::string &attribute) {
  if (!value.as_object().at(attribute).is_array()) {
    const std::string _error_message = "Attribute " + attribute + " must be an array.";
    this->insert_or_push(attribute, _error_message);
  } else {
    this->on_array_of_strings_per_element_rule(value, attribute);
  }
}

void validator::on_array_of_strings_per_element_rule(const value &value, const std::string &attribute) {
  if (auto _elements = value.as_object().at(attribute).as_array(); _elements.empty()) {
    const std::string _error_message = "Attribute " + attribute + " cannot be empty.";
    this->insert_or_push(attribute, _error_message);
  } else {
    size_t _i = 0;
    for (const auto &_element : _elements) {
      if (!_element.is_string()) {
        const std::string _error_message = "Attribute " + attribute + " at position " + std::to_string(_i) + " must be string.";
        this->insert_or_push(attribute, _error_message);
      }
      _i++;
    }
  }
}

void validator::on_number_rule(const value &value, const std::string &attribute) {
  if (!value.as_object().at(attribute).is_int64()) {
    const std::string _error_message = "Attribute " + attribute + " must be a number.";
    this->insert_or_push(attribute, _error_message);
  }
}

void validator::on_object_rule(const value &value, const std::string &attribute) {
  if (!value.as_object().at(attribute).is_object()) {
    const std::string _error_message = "Attribute " + attribute + " must be an object.";
    this->insert_or_push(attribute, _error_message);
  }
}

void validator::on_uuid_rule(const value &value, const std::string &attribute) {
  if (!value.as_object().at(attribute).is_string()) {
    const std::string _error_message = "Attribute " + attribute + " must be string.";
    this->insert_or_push(attribute, _error_message);
  } else {
    try {
      boost::lexical_cast<boost::uuids::uuid>(value.as_object().at(attribute).as_string().data());
    } catch (boost::bad_lexical_cast & /* exception */) {
      const std::string _error_message = "Attribute " + attribute + " must be uuid.";
      this->insert_or_push(attribute, _error_message);
    }
  }
}

void validator::on_string_rule(const value &value, const std::string &attribute) {
  if (!value.as_object().at(attribute).is_string()) {
    const std::string _error_message = "Attribute " + attribute + " must be string.";
    this->insert_or_push(attribute, _error_message);
  }
}

void validator::insert_or_push(const std::string &key, const std::string &message) {
  if (!this->errors_.contains(key)) this->errors_[key] = array({});

  this->errors_.at(key).as_array().emplace_back(message);
}

object validator::get_errors() { return errors_; }

bool validator::get_success() { return success_; }
}  // namespace framework
