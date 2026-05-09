#pragma once

#include <array>
#include <cstdint>

namespace alex {

static constexpr std::uint32_t frames_in_flight = 2;
template <typename T>
using flightframe_array_t = std::array<T, frames_in_flight>;

#if 0
class flightframe_t {
public:
  constexpr auto next() -> void { _value = (_value + 1) % frames_in_flight; }
  constexpr auto get() -> std::uint32_t { return _value; }

private:
  std::uint32_t _value{0};
};
#endif

} // namespace alex
