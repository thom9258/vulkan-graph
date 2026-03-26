#pragma once

#include "core.hpp"
#include <string_view>

namespace alex {

uint32_t
findMemoryType(vk::PhysicalDeviceMemoryProperties const &memoryProperties,
               uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask);

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

struct stored_texture_t {
  std::string_view name{""};
  texture_t texture;
};

struct texture_storage_info_t {
  std::size_t capacity{16};
};

struct texture_storage_t {
  void init(texture_storage_info_t &info, memory::arena &allocator);
  auto add(std::string_view name, texture_t texture) -> texture_t *;
  auto find(std::string_view name) -> texture_t *;
  [[nodiscard]]
  auto remove(std::string_view name) -> std::optional<texture_t>;

  std::span<std::optional<stored_texture_t>> textures;
};

} // namespace alex
