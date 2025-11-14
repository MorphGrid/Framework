#ifndef FRAMEWORK_TASK_GROUP_HPP
#define FRAMEWORK_TASK_GROUP_HPP

#include <framework/support.hpp>

namespace framework {
class task_group {
  std::mutex mutex_;
  boost::asio::steady_timer timer_;
  std::list<boost::asio::cancellation_signal> signals_;

 public:
  explicit task_group(boost::asio::any_io_executor exec) : timer_{std::move(exec), boost::asio::steady_timer::time_point::max()} {}

  task_group(task_group const&) = delete;
  task_group(task_group&&) = delete;

  template <typename CompletionToken>
  auto adapt(CompletionToken&& completion_token) {
    auto _lock = std::lock_guard{mutex_};
    auto _signal = signals_.emplace(signals_.end());

    class remover {
      task_group* task_group_;
      std::list<boost::asio::cancellation_signal>::iterator signal_;

     public:
      remover(task_group* tg, std::list<boost::asio::cancellation_signal>::iterator cs) : task_group_{tg}, signal_{cs} {}

      remover(remover&& other) noexcept : task_group_{std::exchange(other.task_group_, nullptr)}, signal_{other.signal_} {}

      ~remover() {
        if (task_group_) {
          auto _lock = std::lock_guard{task_group_->mutex_};
          boost::system::error_code _ec;
          if (task_group_->signals_.erase(signal_) == task_group_->signals_.end()) {
            try {
              task_group_->timer_.cancel();
            } catch (const boost::system::system_error& e) {
              std::cerr << e.what() << std::endl;
            }
          }
        }
      }
    };

    return boost::asio::bind_cancellation_slot(
        _signal->slot(), boost::asio::consign(std::forward<CompletionToken>(completion_token), remover{this, _signal}));
  }

  void emit(boost::asio::cancellation_type type) {
    auto _lock = std::lock_guard{mutex_};
    for (auto& _signal : signals_) _signal.emit(type);
  }

  template <typename CompletionToken = boost::asio::default_completion_token_t<boost::asio::any_io_executor>>
  auto async_wait(CompletionToken&& completion_token = boost::asio::default_completion_token_t<boost::asio::any_io_executor>{}) {
    return boost::asio::async_compose<CompletionToken, void(boost::system::error_code)>(
        [this, _scheduled = false](auto&& self, boost::system::error_code ec = {}) mutable {
          if (!_scheduled) self.reset_cancellation_state(boost::asio::enable_total_cancellation());

          if (!self.cancelled() && ec == boost::asio::error::operation_aborted) ec = {};

          {
            auto _lock = std::lock_guard{mutex_};

            if (!signals_.empty() && !ec) {
              _scheduled = true;
              return timer_.async_wait(std::move(self));
            }
          }

          if (!std::exchange(_scheduled, true)) return boost::asio::post(boost::asio::append(std::move(self), ec));

          self.complete(ec);
        },
        completion_token, timer_);
  }
};
}  // namespace framework

#endif  // FRAMEWORK_TASK_GROUP_HPP
