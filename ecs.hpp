#pragma once

#include <bitset>
#include <concepts>
#include <cstdint>
#include <span>
#include <tuple>
#include <type_traits>

#include <print>

namespace alex::ecs {

namespace helper {

template <class T, class... Ts>
constexpr std::size_t index_of(const std::tuple<Ts...> &) {
  int found{}, count{};
  ((!found ? (++count, found = std::is_same_v<T, Ts>) : 0), ...);
  return found ? count - 1 : count;
}

} // namespace helper

template <typename t_component, std::uint32_t v_max_count>
struct component_policy_t {
  using component_t = std::remove_cvref_t<t_component>;
  static constexpr std::uint32_t max_count{v_max_count};
};

template <typename t_policy>
concept component_policy = requires(t_policy p) {
  { std::is_destructible_v<typename t_policy::component_t> };
  { t_policy::max_count } -> std::convertible_to<std::uint32_t>;
};

static_assert(component_policy<component_policy_t<char, 50>>);

template <typename... component_policies> struct manager_t {
  using entity_t = std::uint64_t;
  using component_index_plus1_t = std::uint32_t;
  using component_indices_t =
      std::array<component_index_plus1_t, sizeof...(component_policies)>;

  static constexpr entity_t invalid_entity_v{0};
  static constexpr component_index_plus1_t invalid_component_index_v{0};
  struct initialize_t {};
  struct correlation_t {
    entity_t entity{invalid_entity_v};
    component_indices_t indices;
  };
  using correlation_memory_t = std::span<correlation_t>;
  using component_memories_t =
      std::tuple<std::span<typename component_policies::component_t>...>;

  using component_memory_activeflags_t =
      std::tuple<std::bitset<component_policies::max_count>...>;

  explicit constexpr manager_t(
      correlation_memory_t correlation_memory,
      std::span<typename component_policies::component_t>... memories)
      : correlation_memory{correlation_memory},
        memories{std::make_tuple(memories...)} {}

  entity_t generate_next_entity() {
    entity_t entity = next_entity;
    next_entity++;
    return entity;
  }

  [[nodiscard]]
  constexpr auto new_entity() noexcept -> entity_t {
    for (correlation_t &correlation : correlation_memory) {
      if (correlation.entity == invalid_entity_v) {
        correlation.entity = generate_next_entity();
        for (component_index_plus1_t &index : correlation.indices) {
          index = invalid_component_index_v;
        }

        return correlation.entity;
      }
    }
    return invalid_entity_v;
  }

  constexpr auto find_component_indices(entity_t entity)
      -> component_indices_t * {

    if (entity == invalid_entity_v) {
      return nullptr;
    }

    for (correlation_t &correlation : correlation_memory) {
      if (correlation.entity == entity) {
        return &correlation.indices;
      }
    }

    return nullptr;
  }

  template <typename t_component>
  constexpr auto has_component(entity_t entity) -> bool {
    std::size_t const component_index =
        helper::index_of<std::span<t_component>>(memories);

    component_indices_t *indices = find_component_indices(entity);
    if (!indices) {
      return false;
    }

    if ((*indices)[component_index] == invalid_component_index_v) {
      return false;
    }

    return true;
  }

  template <typename t_component>
  constexpr auto get_component(entity_t entity) -> t_component * {
    using component_memory_t = std::span<t_component>;
    std::size_t const component_index =
        helper::index_of<component_memory_t>(memories);

    component_indices_t *indices = find_component_indices(entity);
    if (!indices) {
      return nullptr;
    }

    component_index_plus1_t memory_index = (*indices)[component_index];
    if (memory_index == invalid_component_index_v) {
      return nullptr;
    }

    return &(std::get<component_index>(memories)[memory_index - 1]);
  }

  template <typename t_component>
  constexpr auto add_component(entity_t entity) -> t_component * {
    using component_memory_t = std::span<t_component>;
    std::size_t const component_index =
        helper::index_of<component_memory_t>(memories);

    component_indices_t *indices = find_component_indices(entity);
    if (!indices) {
      return nullptr;
    }

    component_index_plus1_t memory_index = (*indices)[component_index];
    if (memory_index != invalid_component_index_v) {
      return &(std::get<component_index>(memories)[memory_index - 1]);
    }

    auto max_components = std::get<component_index>(memories).size();
    for (component_index_plus1_t i = 0; i < max_components; i++) {
      if (std::get<component_index>(activeflags)[i] == 0) {
        std::get<component_index>(activeflags).set(i);
        (*indices)[component_index] = i + 1;
        return &(std::get<component_index>(memories)[i]);
      }
    }

    return nullptr;
  }

#if 0
  template <typename t_component>
  [[nodiscard]]
  constexpr auto add_component(entity_t entity) noexcept -> t_component * {

    std::size_t const component_index =
        helper::index_of<std::span<t_component>>(memories);

    component_indices_t *indices = find_component_indices(entity);
    if (!indices) {
      return nullptr;
    }

    std::size_t const memory_index = (*indices)[component_index];
    std::println("component index {} memory index {}", component_index,
                 memory_index);

    t_component *component = &component_memories[component_index][memory_index];
    return component;
  }
#endif

  entity_t next_entity{1};
  correlation_memory_t correlation_memory;
  component_memories_t memories;
  component_memory_activeflags_t activeflags;
};

} // namespace alex::ecs
