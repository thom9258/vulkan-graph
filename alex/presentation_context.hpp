#pragma once

#include "core.hpp"
#include "flightframe_array.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace alex {

struct presenter_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::CommandPool commandpool;
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

struct presenter_t {
  explicit presenter_t(presenter_info_t &info);
  [[nodiscard]]
  next_frame_info_t wait_for_next_frame(vk::Device device);
  void present(presentation_info_t &info);

  vk::Extent2D window_extent;
  vk::SurfaceFormatKHR format;
  vk::UniqueSwapchainKHR _swapchain;
  std::vector<vk::Image> _images;
  std::vector<vk::UniqueImageView> _imageviews;

  struct {
    std::vector<vk::UniqueSemaphore> render_finished;
    flightframe_array_t<vk::UniqueSemaphore> image_available;
    flightframe_array_t<vk::UniqueFence> in_flight;
    flightframe_array_t<vk::UniqueCommandBuffer> commandbuffers;

    uint32_t flightframe{0};
    uint32_t image_index{0};
  } _sync;
};

} // namespace alex
