#pragma once

#include "arena.hpp"
#include "memory_utility.hpp"

namespace alex {

template <typename T> struct vector_t {
  using element_t = memory::trait<T>::notref_t;
  using element_reference_t = element_t &;
  using element_rvalue_reference_t = element_t &&;
  using element_const_reference_t = const element_t &;
  using element_pointer_t = element_t *;

  void init(memory::arena *arena, std::size_t initial_count) {
    initial_count = initial_count == 0 ? 1 : initial_count;
    m_arena = arena;
    std::span<element_t> allocated =
        m_arena->allocate<element_t>(initial_count);
    m_elements = allocated.data();
    m_capacity = allocated.size();
  }

  void maybe_grow() {
    if (m_count >= m_capacity - 1) {
      std::size_t const new_count = (m_capacity * m_growth_factor) + 1;
      std::span<element_t> allocated = m_arena->allocate<element_t>(new_count);
      for (std::size_t i = 0; i < m_count; i++) {
        std::swap(allocated[i], m_elements[i]);
      }

      m_elements = allocated.data();
      m_capacity = allocated.size();
    }
  }

  element_reference_t operator[](std::size_t i) { return m_elements[i]; }

  element_pointer_t data() { return m_elements; }

  std::size_t length() { return m_count; }

  std::size_t capacity() { return m_capacity; }

  element_pointer_t last() { return &m_elements[m_count - 1]; }

  std::span<element_t> span() { return {m_elements, m_count}; }

  element_pointer_t put_empty() {
    maybe_grow();
    m_count++;
    return last();
  }

  element_pointer_t put(element_rvalue_reference_t element) {
    put_empty();
    *last() = std::move(element);
    return last();
  }

  element_pointer_t put(element_const_reference_t element) {
    put_empty();
    *last() = element;
    return last();
  }
	
  bool is_initialized() const {
	  return m_arena != nullptr || m_elements == nullptr;
  }

private:
  element_pointer_t m_elements{nullptr};
  std::size_t m_capacity{0};
  std::size_t m_count{0};
  memory::arena *m_arena{nullptr};
  static float constexpr m_growth_factor = 1.5f;
};

} // namespace alex
