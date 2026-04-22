#pragma once

#include <cstdint>
#include <type_traits>
#include <concepts>

namespace alex::memory {

template <typename T>
concept arena_allocatable = std::is_trivially_destructible_v<T>;

template <typename T> struct trait {
  using notref_t = std::remove_cvref_t<T>;
  static constexpr std::size_t size = sizeof(notref_t);
  static constexpr std::size_t alignment = std::alignment_of_v<notref_t>;
  static constexpr std::size_t alignedsize = alignment + size;
};

struct alignment {
  explicit constexpr alignment(std::size_t value) : m_value(value) {}

  constexpr auto value() const -> std::size_t { return m_value; }

private:
  std::size_t m_value;
};

struct no_alignment_t {
  explicit constexpr no_alignment_t(int magic) : _magic(magic) {}

private:
  int _magic;
};

[[maybe_unused]] static constexpr no_alignment_t no_alignment(7);

constexpr std::intptr_t alignforward(alignment alignment, std::intptr_t ptr) {
  if (alignment.value() == 0)
    return ptr;
  return ptr + (((~ptr) + 1) & (alignment.value() - 1));
}

template <typename P>
  requires std::is_pointer_v<P>
constexpr P alignforward(alignment alignment, P p) {
  return reinterpret_cast<P>(
      alignforward(alignment, reinterpret_cast<std::intptr_t>(p)));
}

}
