#pragma once

#include "vulkan_include.hpp"

#include <concepts>
#include <functional>
#include <span>
#include <type_traits>

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

  template <typename F>
    requires requires(F f, vk::CommandBuffer cmdb) { f(cmdb); }
  constexpr auto immediate_evaluate(F &&f) -> vk::Result;

private:
  vk::PhysicalDevice _physical_device;
  vk::UniqueDevice _device;
  vk::Queue _queue;
  std::uint32_t _queuefamily_index;
  vk::UniqueCommandPool _commandpool;
};

template <typename F>
  requires requires(F f, vk::CommandBuffer cmdb) { f(cmdb); }
constexpr auto core_t::immediate_evaluate(F &&f) -> vk::Result {

  vk::UniqueCommandBuffer commandbuffer = create_commandbuffer();
  commandbuffer->begin(vk::CommandBufferBeginInfo{});
  std::invoke(std::forward<F>(f), commandbuffer.get());

  commandbuffer->end();
  auto commandbuffer_submit_info =
      vk::SubmitInfo{}.setCommandBuffers(commandbuffer.get());

  vk::UniqueFence fence = create_fence();
  queue().submit(commandbuffer_submit_info, fence.get());
  const auto max_wait = std::numeric_limits<unsigned int>::max();
  return device().waitForFences(fence.get(), true, max_wait);
}

} // namespace alex
