#pragma once

#include <functional>
#include <print>
#include <chrono>
#include <utility>

enum class LogLevel {
  Critical = 0,
  Error = 1,
  Warning = 2,
  Info = 3,
};

constexpr auto to_string_view(LogLevel level) -> std::string_view {

  switch (level) {
  case LogLevel::Critical:
    return "Critical";
  case LogLevel::Error:
    return "Error";
  case LogLevel::Warning:
    return "Warning";
  case LogLevel::Info:
    return "Info";
  };

  std::unreachable();
}

constexpr void default_log_handler(LogLevel level, std::string_view msg) {

  std::stringstream time;
  time << std::chrono::system_clock::now();
  std::println("[{}] ({})  {}", to_string_view(level), time.str(), msg);
}

using LogHandler = std::function<void(LogLevel, std::string_view)>;

namespace global {

void set_log_level(LogLevel level);
LogLevel log_level();

void set_log_handler(LogHandler handler);
LogHandler& log_handler();

} // namespace global

template <typename... Args>
constexpr void log_fmt(LogLevel level, std::format_string<Args...> fmt, Args &&...args) {
  if (std::to_underlying(level) > std::to_underlying(global::log_level())) {
    return;
  }

  std::string const msg = std::format(fmt, std::forward<Args>(args)...);
  global::log_handler()(level, msg);
}

#define LOG_CRITICAL(MSG, ...) log_fmt(LogLevel::Critical, MSG, ##__VA_ARGS__);
#define LOG_ERROR(MSG, ...) log_fmt(LogLevel::Error, MSG, ##__VA_ARGS__);
#define LOG_WARN(MSG, ...) log_fmt(LogLevel::Warning, MSG, ##__VA_ARGS__);
#define LOG_INFO(MSG, ...) log_fmt(LogLevel::Info, MSG, ##__VA_ARGS__);
