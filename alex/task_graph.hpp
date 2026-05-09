#pragma once

#include "graph.hpp"
#include "vulkan_include.hpp"

#include <concepts>
#include <functional>
#include <map>
#include <ranges>
#include <type_traits>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace alex2::task {

class id_t {
  friend class graph_t;
  friend class dependency_t;

public:
  using value_t = std::uint64_t;
  constexpr static value_t invalid_id_v = std::numeric_limits<value_t>::max();

  constexpr explicit id_t() : _id{invalid_id_v} {}
  constexpr id_t(const id_t &) = default;
  constexpr id_t(id_t &&) = default;
  constexpr id_t &operator=(const id_t &) = default;
  constexpr id_t &operator=(id_t &&) = default;
  constexpr ~id_t() = default;

  constexpr auto get() const -> value_t { return _id; }

  constexpr auto operator<(id_t &other) const -> bool {
    return _id < other.get();
  }

  constexpr auto operator==(id_t &other) const -> bool {
    return _id == other.get();
  }

private:
  explicit id_t(value_t id);
  value_t _id;
};

struct dependency_info_t {
  vk::Device device;
  id_t parent;
  id_t child;
};

class dependency_t {
public:
  explicit constexpr dependency_t(dependency_info_t info) {
    _parent = info.parent;
    _child = info.child;
    const auto create_info = vk::SemaphoreCreateInfo{};
    _semaphore = info.device.createSemaphoreUnique(create_info);
  }

  constexpr auto parent() const -> id_t { return _parent; }
  constexpr auto child() const -> id_t { return _child; }
  constexpr auto semaphore() const -> vk::Semaphore { return _semaphore.get(); }

private:
  id_t _parent;
  id_t _child;
  vk::UniqueSemaphore _semaphore;
};

class task_t {
public:
  constexpr auto name() const -> std::string_view { return _name; }
  constexpr virtual auto evaluate(vk::CommandBuffer commandbuffer) -> void {}

private:
  std::string _name{""};
  vk::CommandBuffer _commandbuffer;
};

class graph_t {
public:
  constexpr auto add_task(std::unique_ptr<task_t> &&task) -> id_t {
    const auto id = id_t(_next_id.get());
    _next_id = id_t(_next_id.get() + 1);
    _tasks.emplace(id, std::move(task));
    return id;
  }

  constexpr auto task(id_t id) -> task_t * {
    auto it = _tasks.find(id);
    if (it == _tasks.end()) {
      return nullptr;
    }

    return it->second.get();
  }

  constexpr auto add_dependency(dependency_t &&dependency) -> void {
    _dependencies.emplace_back(std::move(dependency));
  }

  constexpr auto dependency(id_t parent, id_t child) -> dependency_t * {
    auto const has_parent_child = [&](dependency_t &dependency) -> bool {
      return (dependency.parent() == parent) && (dependency.child() == child);
    };

    auto dependencies = _dependencies | std::views::filter(has_parent_child);
    return &dependencies.front();
  }

private:
  id_t _next_id{0};
  std::map<id_t, std::unique_ptr<task_t>> _tasks;
  std::vector<dependency_t> _dependencies;
};

} // namespace alex2::task
