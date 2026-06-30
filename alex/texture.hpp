#pragma once

#include "core.hpp"
#include "memory_buffer.hpp"
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

class texture_t {
public:
  texture_t() = default;
  explicit texture_t(texture_info_t &info);
  auto memory() -> vk::DeviceMemory;
  auto view() -> vk::ImageView;
  auto image() -> vk::Image;

private:
  vk::UniqueImage _image;
  vk::UniqueImageView _view;
  vk::UniqueDeviceMemory _memory;
};

} // namespace alex
