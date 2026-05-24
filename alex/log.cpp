#include "log.hpp"

namespace alex::global {

auto default_log_handler(log_level_t level, std::string_view msg) -> void {
  std::stringstream time;
  time << std::chrono::system_clock::now();
  std::println("[{}] ({})  {}", to_string_view(level), time.str(), msg);
}

auto singleton_t::set_log_level(log_level_t level) -> void {
  auto guard = std::lock_guard(_log_level_mutex);
  _log_level = level;
}

auto singleton_t::get_log_level() const -> log_level_t {
  auto guard = std::lock_guard(_log_level_mutex);
  return _log_level;
}

auto singleton_t::set_log_handler(log_handler_t handler) -> void {
  auto guard = std::lock_guard(_log_handler_mutex);
  _log_handler = handler;
}

auto singleton_t::get_log_handler() const -> log_handler_t {
  auto guard = std::lock_guard(_log_handler_mutex);
  return _log_handler;
}

} // namespace alex::global
