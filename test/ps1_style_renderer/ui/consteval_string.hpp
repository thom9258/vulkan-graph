#pragma once

#include <cstdint>

template <std::size_t N> struct consteval_string {
  static constexpr std::size_t length_v{N};

  constexpr consteval_string() = default;

  constexpr consteval_string(const char (&str)[N]) {
    for (std::size_t i = 0; i < N; ++i)
      _buf[i] = str[i];
  }

  constexpr auto c_str() const -> const char * { return _buf; }

  char _buf[N]{};
};

template <std::size_t s1, std::size_t s2>
constexpr auto operator+(consteval_string<s1> lhs, consteval_string<s2> rhs) {
  constexpr std::size_t size = s1 - 1 + s2;
  consteval_string<size> result;

  std::size_t index = 0;

  for (std::size_t i = 0; i < s1 - 1; ++i) {
    result._buf[index++] = lhs._buf[i];
  }

  for (std::size_t i = 0; i < s2; ++i) {
    result._buf[index++] = rhs._buf[i];
  }

  return result;
}
