#include "arena.hpp"

namespace alex::memory {

constexpr arena_checkpoint::arena_checkpoint(pointer_t ptr) noexcept
    : m_top{ptr} {}

constexpr auto arena_checkpoint::value() const noexcept -> pointer_t {
  return m_top;
}

arena::arena(memoryspan_t memory) noexcept
    : m_memory{memory}, m_top{m_memory.data()} {}

auto arena::allocate_bytes(alignment alignment, memory_size_t n) noexcept
    -> memoryspan_t {

  if (n < 1) {
    return {};
  }

  memory_size_t const aligned_allocation_size = alignment.value() + n;
  memory_pointer_t allocation_start =
      alignforward<memory_pointer_t>(alignment, m_top);
  memory_pointer_t allocation_end = allocation_start + aligned_allocation_size;
  if (allocation_end >= m_memory.data() + m_memory.size())
    return {};

  m_top = allocation_end;
  return memoryspan_t(allocation_start, n);
}

auto arena::allocate_bytes(no_alignment_t, memory_size_t n) noexcept
    -> memoryspan_t {
  return allocate_bytes(alignment{0}, n);
}

auto arena::allocate_bytes(memory_size_t n) noexcept -> memoryspan_t {
  return allocate_bytes(alignment{trait<byte_t>::alignment}, n);
}

void arena::reset() { m_top = m_memory.data(); }

auto arena::total_memory() const noexcept -> memory_size_t {
  return m_memory.size();
}

auto arena::used_memory() const noexcept -> memory_size_t {
  return m_top - m_memory.data();
}

auto arena::available_memory() const noexcept -> memory_size_t {
  return total_memory() - used_memory();
}

auto arena::revert(arena_checkpoint checkpoint) noexcept -> bool {

  if (checkpoint.value() > m_memory.data() &&
      checkpoint.value() < m_memory.data() + m_memory.size()) {
    m_top = checkpoint.value();
    return true;
  }

  return false;
}

auto arena::top_ptr() const noexcept -> memory_pointer_t {
  return m_top;
}

}; // namespace alex::memory
