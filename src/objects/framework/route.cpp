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

#include <framework/errors/parse_error.hpp>
#include <framework/route.hpp>

namespace framework {
route::route(vector_of<http_verb> verbs, std::string signature, const shared_controller &controller)
    : signature_(std::move(signature)), controller_(controller), verbs_(std::move(verbs)) {
  compile();
}

void route::compile() {
  std::size_t _open = signature_.find('{');
  if (std::size_t _close = signature_.find('}'); _open == std::string::npos && _close == std::string::npos) {
    expression_ = std::make_shared<std::regex>(signature_);
  } else {
    std::string _regex;
    std::size_t _position = 0;

    while (_open != std::string::npos && _close != std::string::npos) {
      _regex.append(signature_.substr(_position, _open - _position));
      std::string _value{signature_.substr(_open + 1, _close - _open - 1)};

      if (std::ranges::find(parameters_, _value) != parameters_.end()) throw errors::parse_error("Route parameters is duplicated.");

      _regex.append(R"(([a-zA-Z0-9\-_]+))");
      parameters_.emplace_back(_value);

      _position = _close + 1;
      _open = signature_.find('{', _close);
      _close = signature_.find('}', _open);
    }

    if (_position != signature_.size()) _regex.append(signature_.substr(_position, signature_.size() - _position));

    expression_ = std::make_shared<std::regex>(_regex);
  }
}

shared_of<std::regex> &route::get_expression() { return expression_; }

shared_controller &route::get_controller() { return controller_; }

vector_of<http_verb> &route::get_verbs() { return verbs_; }

vector_of<std::string> &route::get_parameters() { return parameters_; }

pair_of<bool, params_type> route::match(const std::string &input) {
  params_type _bindings;
  bool _matches = false;
  if (std::smatch _match; std::regex_match(input, _match, *get_expression())) {
    _matches = true;
    auto _iterator = _match.begin();
    ++_iterator;
    _bindings.reserve(get_parameters().size());
    for (auto const &_key : get_parameters()) {
      _bindings[_key] = *_iterator;
      ++_iterator;
    }
  }
  return std::make_pair(_matches, _bindings);
}
}  // namespace framework
