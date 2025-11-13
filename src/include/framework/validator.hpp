
#ifndef FRAMEWORK_VALIDATOR_HPP
#define FRAMEWORK_VALIDATOR_HPP

#include <framework/support.hpp>

namespace framework {

class validator final : public std::enable_shared_from_this<validator> {
  bool per_rule(const value& value, const std::string& attribute, const std::string& rule);
  bool per_scope_rule(const value& value, const std::string& attribute, const std::string_view& rule);

  void on_confirmation_rule(const value& value, const std::string& attribute);
  void on_array_of_strings_rule(const value& value, const std::string& attribute);
  void on_array_of_strings_per_element_rule(const value& value, const std::string& attribute);
  void on_number_rule(const value& value, const std::string& attribute);
  void on_object_rule(const value& value, const std::string& attribute);
  void on_uuid_rule(const value& value, const std::string& attribute);
  void on_string_rule(const value& value, const std::string& attribute);

  object errors_;
  bool success_ = false;

 public:
  void insert_or_push(const std::string& key, const std::string& message);
  object get_errors();
  bool get_success();
  validator() = default;

  static shared_validator make(const map_of<std::string, std::string>& rules, const value& value) {
    auto _response = std::make_shared<validator>();

    for (const auto& [_attribute, _rule] : rules) {
      if (auto const should_break = _response->per_rule(value, _attribute, _rule); should_break) break;
    }

    _response->success_ = _response->errors_.empty();

    return _response;
  }
};
}  // namespace framework

#endif  // FRAMEWORK_VALIDATOR_HPP
