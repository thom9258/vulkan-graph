#pragma once

#if 0

#include "log.hpp"

#include <source_location>

constexpr void default_ensure_handler(std::string_view check,
                                      std::source_location loc,
                                      std::string_view msg) {

  std::string logmsg = std::format("ASSERT [{} L{}] failed!  [{}] {}",
                                   loc.file_name(), loc.line(), check, msg);

  LOG_CRITICAL("{}", logmsg);
  std::exit(-1);
}

// TODO: this must be moved to cpp file like i did with logger
namespace global {
static std::function<void(std::string_view, std::source_location,
                          std::string_view)>
    ensure_handler = default_ensure_handler;
}

template <typename... Args>
constexpr void ensure_fmt(bool failed, std::string_view check,
                          std::source_location loc,
                          std::format_string<Args...> fmt, Args &&...args) {

  if (!failed) {
    return;
  }

  std::string msg = std::format(fmt, std::forward<Args>(args)...);
  global::ensure_handler(check, loc, msg);
}

template <typename... Args>
constexpr void unreachable_fmt(std::source_location loc,
                               std::format_string<Args...> fmt,
                               Args &&...args) {

  std::string msg = std::format(fmt, std::forward<Args>(args)...);
  global::ensure_handler("<UNREACHABLE>", loc, msg);
}

#ifdef NO_ENSURE
#define ENSURE(COND, MSG, ...)
#define ENSURE_NOT(COND, MSG, ...)
#define UNREACHABLE(MSG, ...)
#else
#define ENSURE(COND, MSG, ...)                                                 \
  ensure_fmt(not static_cast<bool>(COND), #COND,                               \
             std::source_location::current(), MSG, ##__VA_ARGS__);

#define ENSURE_NOT(COND, MSG, ...)                                             \
  ensure_fmt(static_cast<bool>(COND), #COND, std::source_location::current(),  \
             MSG, ##__VA_ARGS__);

#define UNREACHABLE(MSG, ...)                                                  \
  unreachable_fmt(std::source_location::current(), MSG, ##__VA_ARGS__);
#endif

#endif
