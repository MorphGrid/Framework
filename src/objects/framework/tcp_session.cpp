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

#include <framework/tcp_session.hpp>

namespace framework {
async_of<std::tuple<boost::system::error_code, std::size_t>> read_exactly(boost::asio::ip::tcp::socket &socket,
                                                                          boost::asio::streambuf &buffer, const std::size_t n) {
  co_return co_await async_read(socket, buffer, boost::asio::transfer_exactly(n), boost::asio::as_tuple);
}

std::uint32_t read_uint32_from_buffer(boost::asio::streambuf &buffer) {
  static_assert(HEADER_SIZE == 4, "HEADER_SIZE must be 4");
  std::istream _input_stream(&buffer);
  std::array<unsigned char, HEADER_SIZE> _header{};
  _input_stream.read(reinterpret_cast<char *>(_header.data()), HEADER_SIZE);
  return static_cast<std::uint32_t>(_header[0]) << 24 | static_cast<std::uint32_t>(_header[1]) << 16 |
         static_cast<std::uint32_t>(_header[2]) << 8 | static_cast<std::uint32_t>(_header[3]) << 0;
}
}  // namespace framework
