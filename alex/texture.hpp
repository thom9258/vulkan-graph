#pragma once

#include "core.hpp"
#include <string_view>

namespace alex {

struct texture_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::Extent2D extent;
  vk::Format format;
  vk::ImageTiling tiling;
  vk::ImageAspectFlags aspect_flags;
  vk::MemoryPropertyFlags property_flags;
  vk::ImageUsageFlags usage;
};

struct texture_t {
  vk::Image image;
  vk::ImageView view;
  vk::DeviceMemory memory;

  void init(texture_info_t& info);
  void cleanup(vk::Device device);
};

} // namespace alex
