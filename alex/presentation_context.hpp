#pragma once

#include "core.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace alex {

struct presentation_context_info_t {
  core_t *core{nullptr};
  vk::SurfaceKHR surface;
  vk::Extent2D window_extent;
  bool enable_vsync{true};
};

struct next_frame_info_t {
  vk::CommandBuffer presentation_commandbuffer;
  uint32_t flightframe{0};
};

struct presentation_info_t {
  vk::Offset3D source_offset_start{0, 0, 0};
  vk::Offset3D source_offset_end{0, 0, 0};

  // TODO: if we can safely assume that the dst offset is from 0.0.0 to w.h.1
  //       then we can remore start and make end a vk::Extent2D
  vk::Offset3D destination_offset_start{0, 0, 0};
  vk::Offset3D destination_offset_end{0, 0, 0};
  vk::Filter blit_filter{vk::Filter::eLinear};
  vk::Image image;
  vk::ImageLayout layout;
  vk::Queue queue;
  vk::CommandBuffer commandbuffer;
};

struct presentation_context_t {
  vk::Extent2D window_extent;
  vk::SurfaceFormatKHR format;
  vk::SwapchainKHR swapchain;
  std::span<vk::Image> images;
  std::span<vk::ImageView> imageviews;

  struct {
    flightframe_array_t<vk::Semaphore> image_available;
    flightframe_array_t<vk::Fence> in_flight;
    flightframe_array_t<vk::CommandBuffer> commandbuffers;

    std::span<vk::Semaphore> render_finished;
    uint32_t flightframe{0};
    uint32_t image_index{0};
  } sync;

  void init(presentation_context_info_t &info, memory::arena &allocator);
  [[nodiscard]]
  next_frame_info_t wait_for_next_frame(vk::Device device);
  void present(presentation_info_t& info);
};

} // namespace alex
