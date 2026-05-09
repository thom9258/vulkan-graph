#pragma once

#include "vulkan_include.hpp"

#include <span>

namespace alex {

struct context_info_t {
  std::string_view name{""};
  std::span<const char *> instance_extensions;
  bool enable_validation{true};
};

class context_t {
public:
  explicit context_t(context_info_t &info);
  auto instance() -> vk::Instance;

private:
  vk::UniqueInstance _instance;
};

struct core_info_t {
  vk::Instance instance;
  vk::Extent2D render_extent;
  vk::SurfaceKHR surface;
};

class core_t {
public:
  explicit core_t(core_info_t &info);
  auto physical_device() -> vk::PhysicalDevice;
  auto device() -> vk::Device;
  auto queue() -> vk::Queue;
  auto queuefamily_index() -> std::uint32_t;
  auto commandpool() -> vk::CommandPool;

  auto create_descriptorpool(vk::DescriptorPoolCreateInfo info)
      -> vk::UniqueDescriptorPool;

  auto create_commandbuffer() -> vk::UniqueCommandBuffer;
  auto create_fence() -> vk::UniqueFence;
  auto create_fence_signaled() -> vk::UniqueFence;

private:
  vk::PhysicalDevice _physical_device;
  vk::UniqueDevice _device;
  vk::Queue _queue;
  std::uint32_t _queuefamily_index;
  vk::UniqueCommandPool _commandpool;
};

} // namespace alex
