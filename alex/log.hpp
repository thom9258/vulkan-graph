#pragma once

#include <chrono>
#include <functional>
#include <print>
#include <thread>
#include <utility>

#define ALEX_ERROR(MSG, ...)                                                   \
  alex::global::singleton_t::instance().log_fmt(alex::log_level_t::error, MSG, \
                                                ##__VA_ARGS__);
#define ALEX_WARN(MSG, ...)                                                    \
  alex::global::singleton_t::instance().log_fmt(alex::log_level_t::warning,    \
                                                MSG, ##__VA_ARGS__);
#define ALEX_INFO(MSG, ...)                                                    \
  alex::global::singleton_t::instance().log_fmt(alex::log_level_t::info, MSG,  \
                                                ##__VA_ARGS__);

#define ALEX_UNREACHABLE()                                                     \
  ALEX_ERROR("UNREACHABLE! code reached unreachable code path! [F:{} L:{}]",   \
             std::source_location::current().file_name(),                      \
             std::source_location::current().line());

#define ALEX_ERROR_IF(CONDITION, MSG, ...)                                     \
  if (CONDITION) {                                                             \
    ALEX_ERROR(MSG, ##__VA_ARGS__)                                             \
  }

#define ALEX_WARN_IF(CONDITION, MSG, ...)                                      \
  if (CONDITION) {                                                             \
    ALEX_WARN(MSG, ##__VA_ARGS__)                                              \
  }

namespace alex {

enum class log_level_t {
  error = 0,
  warning = 1,
  info = 2,
};

constexpr auto to_string_view(log_level_t level) -> std::string_view {

  switch (level) {
  case log_level_t::error:
    return "error";
  case log_level_t::warning:
    return "warning";
  case log_level_t::info:
    return "info";
  };

  std::unreachable();
}

} // namespace alex

namespace alex::global {

auto default_log_handler(log_level_t level, std::string_view msg) -> void;

using log_handler_t = std::function<void(log_level_t, std::string_view)>;

static_assert(requires(log_handler_t lh) { lh = default_log_handler; });

class singleton_t {
  singleton_t() = default;

public:
  singleton_t(const singleton_t &) = delete;
  singleton_t(singleton_t &&) = delete;
  singleton_t &operator=(const singleton_t &) = delete;
  singleton_t &operator=(singleton_t &&) = delete;

  auto set_log_level(log_level_t level) -> void;
  auto get_log_level() const -> log_level_t;

  auto set_log_handler(log_handler_t handler) -> void;
  auto get_log_handler() const -> log_handler_t;

  constexpr static auto instance() -> singleton_t & {
    static singleton_t _instance{};
    return _instance;
  }

  template <typename... Args>
  constexpr auto log_fmt(log_level_t level, std::format_string<Args...> fmt,
                         Args &&...args) -> void {
    auto level_guard = std::lock_guard(_log_level_mutex);
    if (std::to_underlying(level) > std::to_underlying(_log_level)) {
      return;
    }

    std::string const msg = std::format(fmt, std::forward<Args>(args)...);
    auto handler_guard = std::lock_guard(_log_handler_mutex);
    _log_handler(level, msg);
  }

private:
  mutable std::mutex _log_handler_mutex;
  mutable std::mutex _log_level_mutex;
  log_handler_t _log_handler{default_log_handler};
  log_level_t _log_level{log_level_t::info};
};

} // namespace alex::global
