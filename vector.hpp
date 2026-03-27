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

  vector_t(memory::arena &arena, std::size_t initial_element_count) {
    initial_element_count =
        initial_element_count == 0 ? 1 : initial_element_count;
    m_elements = arena.allocate<element_t>(initial_element_count);
  }

  void maybe_grow(memory::arena &resize_arena) {
    if (m_current_element_count >= m_elements.size()-1) {
      std::size_t const new_count = (m_elements.size() * m_growth_factor) + 1;
      std::span<element_t> new_elements =
          resize_arena.allocate<element_t>(new_count);

      for (std::size_t i = 0; i < m_elements.size(); i++) {
        std::swap(new_elements[i], m_elements[i]);
      }

      m_elements = new_elements;
    }
  }

  element_reference_t operator[](std::size_t i) { return m_elements[i]; }

  element_pointer_t data() { return m_elements.data(); }

  std::size_t length() { return m_current_element_count; }

  std::size_t capacity() { return m_elements.size(); }

  element_pointer_t last() { return &m_elements[m_current_element_count - 1]; }

  element_pointer_t put(memory::arena &resize_arena,
                        element_rvalue_reference_t element) {
    maybe_grow(resize_arena);
    m_elements[m_current_element_count] = std::move(element);
    m_current_element_count++;
    return last();
  }

  element_pointer_t put(memory::arena &resize_arena,
                        element_const_reference_t element) {
    maybe_grow(resize_arena);
    m_elements[m_current_element_count] = element;
    m_current_element_count++;
    return last();
  }

private:
  std::span<element_t> m_elements;
  std::size_t m_current_element_count{0};
  static float constexpr m_growth_factor = 1.5f;
};

} // namespace alex
