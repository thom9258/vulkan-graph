#pragma once

#include "memory_utility.hpp"

#include <span>

namespace alex::memory {

class arena_checkpoint {
public:
  using pointer_t = std::uint8_t *;
  explicit constexpr arena_checkpoint(pointer_t ptr) noexcept;
  constexpr arena_checkpoint(std::nullptr_t) = delete;
  constexpr ~arena_checkpoint() = default;
  constexpr arena_checkpoint(const arena_checkpoint &) = default;
  constexpr arena_checkpoint(arena_checkpoint &&) = default;
  constexpr arena_checkpoint &operator=(const arena_checkpoint &) = default;
  constexpr arena_checkpoint &operator=(arena_checkpoint &&) = default;
  constexpr auto value() const noexcept -> pointer_t;

private:
  pointer_t m_top;
};

// TODO: you REALLY need to test this implementation before starting to use it!
class arena {
public:
  using byte_t = std::uint8_t;
  using memoryspan_t = std::span<byte_t>;
  using memory_pointer_t = memoryspan_t::pointer;
  using memory_size_t = memoryspan_t::size_type;
  using element_count_t = std::size_t;

  static_assert(std::is_same_v<arena_checkpoint::pointer_t, memory_pointer_t>);

  arena(memoryspan_t memory) noexcept;
  constexpr arena(const arena &) = delete;
  constexpr arena &operator=(arena) = delete;
  constexpr ~arena() noexcept = default;

  auto allocate_bytes(alignment alignment, memory_size_t n) noexcept
      -> memoryspan_t;

  auto allocate_bytes(no_alignment_t, memory_size_t n) noexcept -> memoryspan_t;

  auto allocate_bytes(memory_size_t n) noexcept -> memoryspan_t;

  template <typename Ti, typename T = trait<Ti>::notref_t>
  auto allocate(alignment alignment, element_count_t n) noexcept -> std::span<T>
    requires std::is_trivially_destructible_v<T>
  {
    return as_element_memoryspan<T>(allocate_bytes(alignment, n * sizeof(T)),
                                    n);
  }

  template <typename Ti, typename T = trait<Ti>::notref_t>
  auto allocate(no_alignment_t, element_count_t n) noexcept -> std::span<T>
    requires std::is_trivially_destructible_v<T>
  {
    return as_element_memoryspan<T>(allocate_bytes(no_alignment, n * sizeof(T)),
                                    n);
  }

  template <typename Ti, typename T = trait<Ti>::notref_t>
  auto allocate(element_count_t n) noexcept -> std::span<T>
    requires std::is_trivially_destructible_v<T>
  {
    constexpr alignment alignment{trait<T>::alignment};
    const memory_size_t total_size{n * trait<T>::size};
    return as_element_memoryspan<T>(allocate_bytes(alignment, total_size), n);
  }

  void reset();
  auto total_memory() const noexcept -> memory_size_t;
  auto available_memory() const noexcept -> memory_size_t;
  auto used_memory() const noexcept -> memory_size_t;
  auto revert(arena_checkpoint checkpoint) noexcept -> bool;

  // DEBUGGING
  auto top_ptr() const noexcept -> memory_pointer_t;

private:
  template <typename Ti, typename T = trait<Ti>::notref_t>
  static constexpr auto as_element_memoryspan(memoryspan_t memory,
                                              element_count_t n) noexcept
      -> std::span<T> {
    if (memory.empty())
      return {};
    return std::span<T>(reinterpret_cast<T *>(memory.data()), n);
  }

  memoryspan_t m_memory;
  memory_pointer_t m_top;
};

} // namespace alex::memory
