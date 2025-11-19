#ifndef FRAMEWORK_SIGNAL_HANDLER_HPP
#define FRAMEWORK_SIGNAL_HANDLER_HPP

#include <framework/support.hpp>
#include <framework/task_group.hpp>

namespace framework {
inline async_of<void> signal_handler(task_group &task_group) {
  const auto _executor = co_await boost::asio::this_coro::executor;
  auto _signal_set = boost::asio::signal_set{_executor, SIGINT, SIGTERM};

  auto _sig = co_await _signal_set.async_wait();

  if (_sig == SIGINT) {
    task_group.emit(boost::asio::cancellation_type::total);

    auto [ec] = co_await task_group.async_wait(boost::asio::as_tuple(
        boost::asio::cancel_after(std::chrono::seconds{10})));

    if (ec == boost::asio::error::operation_aborted)  // Timeout occurred
    {
      task_group.emit(boost::asio::cancellation_type::terminal);
      co_await task_group.async_wait();
    }

  } else {
    boost::asio::query(_executor.get_inner_executor(),
                       boost::asio::execution::context)
        .stop();
  }
}
}  // namespace framework

#endif  // FRAMEWORK_SIGNAL_HANDLER_HPP
