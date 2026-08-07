#pragma once

#include <chrono>

namespace utility {

class timer_t {
public:
	constexpr timer_t();
	constexpr auto elapsed_ms() const -> double;

private:	
    std::chrono::time_point<std::chrono::steady_clock> m_start;
};

constexpr timer_t::timer_t()
	: m_start(std::chrono::steady_clock::now())
{
}
	
constexpr auto timer_t::elapsed_ms() const -> double
{
    const auto now = std::chrono::steady_clock::now();
	return std::chrono::duration<double>(now - m_start).count();
}

}
