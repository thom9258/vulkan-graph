#pragma once

#include "ensure.hpp"
#include "memory_utility.hpp"

namespace alex {

template <typename T> struct fixed_vector_t {
  using element_t = memory::trait<T>::notref_t;
  using element_reference_t = element_t &;
  using element_rvalue_reference_t = element_t &&;
  using element_const_reference_t = const element_t &;
  using element_pointer_t = element_t *;
  using length_t = std::size_t;

  constexpr auto init(std::span<element_t> elements) {
    m_elements = elements;
    m_count = 0;
  }

  constexpr auto at(std::size_t i) -> element_pointer_t {
    if (i < m_count) {
      m_elements[i];
    }

    return nullptr;
  }

  constexpr auto operator[](std::size_t i) -> element_reference_t {
	ENSURE(i < m_count, "invalid access of vector element")
    return m_elements[i];
  }

  constexpr auto last() -> element_pointer_t {
    return &m_elements[m_count - 1];
  }

  constexpr auto span() -> std::span<element_t> {
    return {m_elements, m_count};
  }

  constexpr auto put_empty() -> element_pointer_t {
    m_count++;
    return last();
  }

  constexpr auto put(element_rvalue_reference_t element) -> element_pointer_t {
    element_pointer_t last = put_empty();
    *last = std::move(element);
    return last;
  }

  constexpr auto put(element_const_reference_t element) -> element_pointer_t {
    put_empty();
    *last() = element;
    return last();
  }

  constexpr auto is_initialized() const -> bool { return !m_elements.empty(); }

  constexpr auto empty() const -> bool { return m_count == 0; }

  constexpr auto data() -> element_pointer_t { return m_elements.data(); }

  constexpr auto length() -> length_t { return m_count; }

private:
  std::span<element_t> m_elements{};
  length_t m_count{0};
};

} // namespace alex
