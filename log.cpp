#include "log.hpp"

std::function<void(LogLevel, std::string_view)> g_log_handler =
    ::default_log_handler;

LogLevel g_log_level = LogLevel::Info;


namespace global {

void set_log_level(LogLevel level) {
	g_log_level = level;
}

LogLevel log_level() {
	return g_log_level;
}

void set_log_handler(LogHandler handler) {
	g_log_handler = handler;
}

LogHandler& log_handler() {
	return g_log_handler;
}

}
