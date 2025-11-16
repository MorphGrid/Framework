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

#include <framework/tcp_connection.hpp>
#include <framework/tcp_handlers.hpp>
#include <framework/tcp_service.hpp>
#include <framework/tcp_session.hpp>
#include <framework/errors/tcp/frame_too_large.hpp>
#include <framework/errors/tcp/on_read_error.hpp>

namespace framework {
    async_of<void> tcp_session(const shared_state state, const shared_tcp_service service,
                               const shared_tcp_connection connection) {
        auto _cancellation_state = co_await boost::asio::this_coro::cancellation_state;

        if (service->handlers()->on_accepted()) co_await service->handlers()->on_accepted()(service, connection);

        auto &socket = connection->get_stream()->socket();
        auto &buffer = connection->get_buffer();

        while (!_cancellation_state.cancelled()) {
            connection->get_stream()->expires_after(std::chrono::minutes(60));

            auto [_header_ec, _header_read_bytes] = co_await async_read(
                    socket,
                    buffer,
                    boost::asio::transfer_exactly(HEADER_SIZE),
                    boost::asio::as_tuple);

            if (_header_ec) {
                if (service->handlers()->on_disconnected())
                    co_await service->handlers()->on_disconnected()(
                        service, connection);
                co_return;
            }

            std::uint32_t _payload_length = 0;
            std::istream _input_stream(&buffer);
            unsigned char _header[HEADER_SIZE];
            _input_stream.read(reinterpret_cast<char *>(_header), HEADER_SIZE);
            _payload_length = (static_cast<std::uint32_t>(_header[0]) << 24) |
                              (static_cast<std::uint32_t>(_header[1]) << 16) |
                              (static_cast<std::uint32_t>(_header[2]) << 8) |
                              (static_cast<std::uint32_t>(_header[3]) << 0);

            if (_payload_length == 0) {
                continue;
            }

            if (_payload_length > MAX_FRAME_SIZE) {
                const errors::tcp::frame_too_large _error;
                if (service->handlers()->on_error())
                    co_await service->handlers()->on_error()(
                        service, connection, _error);
                if (service->handlers()->on_disconnected())
                    co_await service->handlers()->on_disconnected()(
                        service, connection);
                boost::system::error_code ignored_ec;
                socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
                socket.close(ignored_ec);
                co_return;
            }

            auto [_payload_ec, _bytes_transferred] = co_await async_read(
                                socket,
                                buffer,
                                boost::asio::transfer_exactly(_payload_length),
                                boost::asio::as_tuple);
            if (_payload_ec) {
                const errors::tcp::on_read_error _error;
                if (service->handlers()->on_error())
                    co_await service->handlers()->on_error()(
                        service, connection, _error);
                if (service->handlers()->on_disconnected())
                    co_await service->handlers()->on_disconnected()(
                        service, connection);
                co_return;
            }

            std::string _payload;
            _payload.resize(_payload_length); {
                std::istream is(&buffer);
                is.read(_payload.data(), _payload_length);
            }

            if (service->handlers()->on_read()) {
                co_await service->handlers()->on_read()(service, connection, std::move(_payload));
            }
        }

        if (service->handlers()->on_disconnected())
            co_await service->handlers()->on_disconnected()(
                service, connection);

        if (!connection->get_stream()->socket().is_open()) co_return;

        connection->get_stream()->socket().shutdown(socket::shutdown_send);
    }
} // namespace framework
