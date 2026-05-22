#pragma once

#include "graph.hpp"
#include "vulkan_include.hpp"
#include <algorithm>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include <concepts>
#include <functional>
#include <map>
#include <print>
#include <ranges>
#include <type_traits>

namespace alex2 {

class task_id_t {
  friend class graph_t;
  friend class dependency_t;

public:
  using value_t = std::uint64_t;
  constexpr static value_t invalid_id_v = std::numeric_limits<value_t>::max();

  constexpr explicit task_id_t(value_t id) : _id{id} {}
  constexpr explicit task_id_t() : _id{invalid_id_v} {}
  constexpr task_id_t(const task_id_t &) = default;
  constexpr task_id_t(task_id_t &&) = default;
  constexpr task_id_t &operator=(const task_id_t &) = default;
  constexpr task_id_t &operator=(task_id_t &&) = default;
  constexpr ~task_id_t() = default;

  constexpr auto get() const -> value_t { return _id; }

  constexpr auto valid() const -> bool { return _id != invalid_id_v; }

  constexpr auto operator<(const task_id_t &other) const -> bool {
    return _id < other.get();
  }

  constexpr auto operator==(const task_id_t &other) const -> bool {
    return _id == other.get();
  }

private:
  value_t _id;
};

struct dependency_info_t {
  vk::Device device;
  task_id_t parent;
  task_id_t child;
};

class task_t {
public:
  constexpr virtual auto name() const -> std::string_view {
    return "<unnamed-task>";
  }
  constexpr virtual auto evaluate(vk::CommandBuffer commandbuffer) -> void {}

  constexpr virtual ~task_t() = default;
};

class simple_task_t : public task_t {
public:
  using fn_t = std::function<void(vk::CommandBuffer)>;
  explicit constexpr simple_task_t(std::string_view name, fn_t fn)
      : _name{name}, _fn{fn} {}

  constexpr auto name() const -> std::string_view override { return _name; }

  constexpr auto evaluate(vk::CommandBuffer commandbuffer) -> void override {
    _fn(commandbuffer);
  }

private:
  std::string _name;
  fn_t _fn;
};

class graph_t {

  class dependency_t {
  public:
    explicit constexpr dependency_t(dependency_info_t info) {
      _parent = info.parent;
      _child = info.child;
      const auto create_info = vk::SemaphoreCreateInfo{};
      _semaphore = info.device.createSemaphoreUnique(create_info);
    }

    constexpr auto parent() const -> task_id_t { return _parent; }
    constexpr auto child() const -> task_id_t { return _child; }
    constexpr auto semaphore() const -> vk::Semaphore {
      return _semaphore.get();
    }

  private:
    task_id_t _parent;
    task_id_t _child;
    vk::UniqueSemaphore _semaphore;
  };

  class job_t {
  public:
    explicit constexpr job_t(task_id_t id, std::unique_ptr<task_t> task)
        : _id{id}, _task{std::move(task)} {}

    constexpr auto task() const -> task_t * { return _task.get(); }
    constexpr auto id() const -> task_id_t { return _id; }

  private:
    task_id_t _id;
    std::unique_ptr<task_t> _task;
  };

public:
  constexpr auto set_end(task_id_t id) -> void { _ending_task_id = id; }

  constexpr auto add_task(task_id_t id, std::unique_ptr<task_t> task) -> void {
    _jobs.emplace_back(id, std::move(task));
  }

  constexpr auto get_job(task_id_t id) -> job_t * {
    auto const has_id = [id](job_t &task) -> bool { return task.id() == id; };

    auto found = std::ranges::find_if(_jobs, has_id);
    if (found == std::ranges::end(_jobs)) {
      return nullptr;
    }

    return &(*found);
  }

  constexpr auto add_dependency(dependency_info_t info) -> void {
    _dependencies.push_back(std::make_unique<dependency_t>(info));
  }

  constexpr auto get_dependencies_for_child(task_id_t child)
      -> std::vector<dependency_t *> {
    auto const is_parent =
        [&](std::unique_ptr<dependency_t> &dependency) -> bool {
      if (dependency == nullptr) {
        return false;
      }
      return dependency->child() == child;
    };
    auto const get_dependency =
        [](std::unique_ptr<dependency_t> &dependency) -> dependency_t * {
      return dependency.get();
    };

    return _dependencies | std::views::filter(is_parent) |
           std::views::transform(get_dependency) |
           std::ranges::to<std::vector>();
  }

  constexpr auto evaluate(vk::Semaphore signal) -> void {

    if (!_ending_task_id.valid()) {
      return;
    }

    auto const recursive_eval = [&](auto &self, job_t *job) -> void {
      if (job == nullptr) {
        return;
      }

      std::vector<dependency_t *> const parent_dependencies =
          get_dependencies_for_child(job->id());

      for (dependency_t *dependency : parent_dependencies) {
        self(self, get_job(dependency->parent()));
      }

      job->task()->evaluate({});
    };

    recursive_eval(recursive_eval, get_job(_ending_task_id));
  }

private:
  std::vector<job_t> _jobs;
  std::vector<std::unique_ptr<dependency_t>> _dependencies;
  task_id_t _ending_task_id;
};

} // namespace alex2
