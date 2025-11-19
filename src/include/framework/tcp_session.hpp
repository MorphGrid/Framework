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

#ifndef FRAMEWORK_TCP_SESSION_HPP
#define FRAMEWORK_TCP_SESSION_HPP

#include <framework/support.hpp>
#include <framework/tcp_connection.hpp>
#include <framework/tcp_service.hpp>

namespace framework {
async_of<std::tuple<boost::system::error_code, std::size_t>> read_exactly(
    boost::asio::ip::tcp::socket &socket, boost::asio::streambuf &buffer,
    std::size_t n);

std::uint32_t read_uint32_from_buffer(boost::asio::streambuf &buffer);

async_of<void> notify_error_and_close(shared_of<tcp_service> service,
                                      shared_of<tcp_connection> connection,
                                      boost::asio::ip::tcp::socket &socket,
                                      std::exception error);

async_of<void> notify_disconnected_if_present(
    shared_of<tcp_service> service, shared_of<tcp_connection> connection);

async_of<void> tcp_session(shared_state state, shared_of<tcp_service> service,
                           shared_of<tcp_connection> connection);
}  // namespace framework

#endif  // FRAMEWORK_TCP_SESSION_HPP
