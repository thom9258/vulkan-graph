#pragma once

#include "ensure.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace alex {

namespace detail {

using id_value_t = std::int64_t;
static constexpr id_value_t invalid_id_value{
    std::numeric_limits<id_value_t>::max()};

} // namespace detail

struct invalid_id_t {
  explicit constexpr invalid_id_t(int ignored) : ignored{ignored} {}
  int ignored{0};
};

static constexpr invalid_id_t invalid_id{1};

template <typename t_tag> struct id_t {
  using tag_t = std::remove_cvref_t<t_tag>;

  explicit constexpr id_t(invalid_id_t) noexcept
      : value{detail::invalid_id_value} {}
  constexpr id_t(detail::id_value_t value) noexcept : value{value} {}

  constexpr auto get() const noexcept -> std::uint64_t { return value; }
  constexpr auto invalid() const noexcept -> bool {
    return value == detail::invalid_id_value;
  }

  constexpr auto operator==(id_t<tag_t> other) { return value == other.get(); }
  constexpr auto operator<(id_t<tag_t> other) { return value < other.get(); }

private:
  detail::id_value_t value;
};

template <typename t_tag> struct id_generator_t {
  using tag_t = std::remove_cvref_t<t_tag>;

  id_generator_t() = default;
  ~id_generator_t() = default;
  id_generator_t(id_generator_t &&) = default;
  id_generator_t &operator=(id_generator_t &&) = default;
  id_generator_t(const id_generator_t &) = delete;
  id_generator_t &operator=(const id_generator_t &) = delete;

  constexpr auto generate() noexcept -> id_t<tag_t> {
    auto constexpr id = id_t<tag_t>(next_id);
    ENSURE_NOT(id.invalid(), "Could not generate next ID")
    next_id++;
    return id;
  }

private:
  detail::id_value_t next_id{0};
};

} // namespace alex
