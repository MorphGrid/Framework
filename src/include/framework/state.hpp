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

#ifndef FRAMEWORK_STATE_HPP
#define FRAMEWORK_STATE_HPP

#include <framework/encoding.hpp>
#include <framework/metrics.hpp>
#include <framework/router.hpp>
#include <framework/support.hpp>

namespace framework {
class state : public std::enable_shared_from_this<state> {
  shared_router router_ = std::make_shared<router>();
  map_hash_of<std::string, shared_queue, std::less<>> queues_;
  std::mutex queues_mutex_;
  boost::asio::io_context ioc_{static_cast<int>(std::thread::hardware_concurrency())};
  shared_of<boost::mysql::connection_pool> connection_pool_;
  shared_of<metrics> metrics_ = std::make_shared<metrics>();

  atomic_of<bool> running_{false};
  atomic_of<unsigned short int> port_{0};
  std::string key_ = base64url_decode(dotenv::getenv("APP_KEY", "-66WcolkZd8-oHejFFj1EUhxg3-8UWErNkgMqCwLDEI"));

 public:
  state();
  ~state();
  shared_of<boost::mysql::connection_pool> get_connection_pool();
  bool get_running() const;
  shared_metrics get_metrics();
  std::string get_key() const;
  unsigned short int get_port() const;
  void set_port(unsigned short int port);
  void set_running(bool running);
  map_hash_of<std::string, shared_queue, std::less<>>& queues() noexcept;
  shared_router get_router() const noexcept;
  shared_queue get_queue(const std::string& name) noexcept;
  bool remove_queue(const std::string& name) noexcept;
  bool queue_exists(const std::string& name) noexcept;
  void run() noexcept;
  boost::asio::io_context& ioc() noexcept;
};
}  // namespace framework

#endif  // FRAMEWORK_STATE_HPP
