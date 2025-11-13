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

#include <gtest/gtest.h>

#include <framework/validator.hpp>

using namespace framework;

TEST(test_validator, can_validate) {
  const map_of<std::string, std::string> _rules = {
      {"*", "is_object"},      {"uuid", "is_uuid"},     {"string", "is_string"},
      {"string", "confirmed"}, {"number", "is_number"}, {"array_of_strings", "is_array_of_strings"},
      {"object", "is_object"},
  };

  const std::string _valid_object =
      R"({"uuid":"7f058c7f-b9e3-4692-a443-e9b317e39424","string":"hello world","string_confirmation":"hello world","number":10,"array_of_strings":["hello","world","!"],"object":{}})";
  const auto _value = boost::json::parse(_valid_object);
  const auto _response = validator::make(_rules, _value);

  ASSERT_TRUE(_response->get_success());
}

TEST(test_validator, fails_on_missing) {
  const map_of<std::string, std::string> _rules = {
      {"*", "is_object"},      {"uuid", "is_uuid"},      {"string", "is_string"},
      {"string", "confirmed"}, {"number", "is_number"},  {"array_of_strings", "is_array_of_strings"},
      {"object", "is_object"}, {"optional", "nullable"},
  };

  const std::string _empty_object = R"({})";
  const auto _value = boost::json::parse(_empty_object);
  const auto _response = validator::make(_rules, _value);

  ASSERT_FALSE(_response->get_success());

  const std::string _expected_errors =
      R"({"array_of_strings":["Attribute array_of_strings is required."],"number":["Attribute number is required."],"object":["Attribute object is required."],"string":["Attribute string is required."],"uuid":["Attribute uuid is required."]})";
  ASSERT_EQ(serialize(_response->get_errors()), _expected_errors);
}

TEST(test_validator, fails_on_error) {
  const map_of<std::string, std::string> _rules = {
      {"*", "is_object"},
      {"uuid", "is_string,is_uuid"},
      {"uuid_v4", "is_uuid"},
      {"string", "is_string"},
      {"string", "confirmed"},
      {"strong", "confirmed"},
      {"streng", "confirmed"},
      {"strung", "confirmed"},
      {"number", "is_number"},
      {"array_of_strings", "is_array_of_strings"},
      {"wrong_array_of_strings", "is_array_of_strings"},
      {"empty_array_of_strings", "is_array_of_strings"},
      {"object", "is_object"},
  };

  const std::string _invalid_object =
      R"({"uuid":"invalid-uuid","uuid_v4":10,"string":7,"string_confirmation":"hello world","strong":"hello","strong_confirmation":"world","streng":"test","strung":"abc","strung_confirmation":10,"number":"10","array_of_strings":[10,[],"hello"],"wrong_array_of_strings":"abc","empty_array_of_strings":[],"object":[]})";
  const auto _value = boost::json::parse(_invalid_object);
  const auto _response = validator::make(_rules, _value);

  ASSERT_FALSE(_response->get_success());

  const std::string _expected_errors =
      R"({"array_of_strings":["Attribute array_of_strings at position 0 must be string.","Attribute array_of_strings at position 1 must be string."],"empty_array_of_strings":["Attribute empty_array_of_strings cannot be empty."],"number":["Attribute number must be a number."],"object":["Attribute object must be an object."],"streng":["Attribute streng_confirmation must be present."],"string":["Attribute string must be string."],"strong":["Attribute strong and strong_confirmation must be equals."],"strung":["Attribute strung_confirmation must be string."],"uuid":["Attribute uuid must be uuid."],"uuid_v4":["Attribute uuid_v4 must be string."],"wrong_array_of_strings":["Attribute wrong_array_of_strings must be an array."]})";
  ASSERT_EQ(serialize(_response->get_errors()), _expected_errors);
}

TEST(test_validator, reject_primitives_different_than_object) {
  const map_of<std::string, std::string> _rules = {{"*", "is_object"}, {"id", "is_uuid"}};

  const std::string _input = R"([1, 2, 3])";
  const auto _value = boost::json::parse(_input);

  const auto _response = validator::make(_rules, _value);
  ASSERT_FALSE(_response->get_success());
}

TEST(test_validator, validate_null_values) {
  const map_of<std::string, std::string> _rules = {{"*", "is_object"}, {"comment", "nullable"}};

  const std::string _input = R"({"comment": null})";
  const auto _value = boost::json::parse(_input);

  const auto _response = validator::make(_rules, _value);
  ASSERT_TRUE(_response->get_success());
}

TEST(test_validator, validate_null_missing_values) {
  const map_of<std::string, std::string> _rules = {{"*", "is_object"}, {"note", "nullable"}};

  const std::string _input = R"({})";
  const auto _value = boost::json::parse(_input);

  const auto _response = validator::make(_rules, _value);
  ASSERT_TRUE(_response->get_success());
}
