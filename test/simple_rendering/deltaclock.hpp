#pragma once

#include <chrono>

class DeltaClock {
public:
	DeltaClock();
	void tick();
	double deltatime_ms();
	double totaltime_ms();

private:	
    std::chrono::time_point<std::chrono::steady_clock> m_start;
    std::chrono::time_point<std::chrono::steady_clock> m_last;
    double m_deltatime = 0;
};

DeltaClock::DeltaClock()
	: m_start(std::chrono::steady_clock::now())
	, m_last(std::chrono::steady_clock::now())
{
}
	
void DeltaClock::tick()
{
    const auto current = std::chrono::steady_clock::now();
	m_deltatime = std::chrono::duration<double>(current - m_last).count();
	m_last = current;
}

double DeltaClock::deltatime_ms()
{
	return m_deltatime;
}
	
double DeltaClock::totaltime_ms()
{
    const auto current = std::chrono::steady_clock::now();
	return std::chrono::duration<double>(current - m_start).count();
}
