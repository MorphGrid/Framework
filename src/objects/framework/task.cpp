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

#include <framework/task.hpp>

namespace framework {
task::task(handler_type callback)
    : callback_(std::make_shared<handler_type>(std::move(callback))) {}

uuid task::get_id() const noexcept { return id_; }

shared_of<handler_type> task::callback() const noexcept { return callback_; }
}  // namespace framework
