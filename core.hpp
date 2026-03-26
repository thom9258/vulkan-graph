#pragma once

#include <vulkan/vulkan.hpp>

#include "arena.hpp"

namespace alex {

static constexpr std::uint32_t frames_in_flight = 2;
template <typename T> using flightframe_array_t = std::array<T, frames_in_flight>;

struct context_info_t {
  std::string_view name{""};
  std::span<const char *> instance_extensions;
  bool enable_validation{true};
};

struct context_t {
  vk::Instance instance;
  void init(context_info_t &info, memory::arena &allocator);
};

struct core_info_t {
  context_t *context{nullptr};
  vk::Extent2D render_extent;
  vk::SurfaceKHR surface;
};

struct core_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::Queue queue;
  std::uint32_t queuefamily_index;
  vk::CommandPool commandpool;

  void init(core_info_t &info, memory::arena &allocator);
};

}
