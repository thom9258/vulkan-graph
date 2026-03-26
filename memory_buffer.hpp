#pragma once

#include "core.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

enum class BufferType {
  Basic,
  Uniform,
  Vertices,
};    

struct direct_memory_buffer_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  BufferType buffer_type{BufferType::Basic};
  std::size_t memory_size{1};
};

struct direct_memory_buffer_t {
  vk::Buffer buffer;
  vk::DeviceMemory memory;
  size_t memory_size{0};
  void *memory_ptr{nullptr};

  void init(direct_memory_buffer_info_t &info);
  void cleanup(vk::Device device);
};

struct memory_buffer_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  BufferType buffer_type{BufferType::Basic};
  size_t memory_size{0};
};

struct memory_buffer_write_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  direct_memory_buffer_t *memory;
  size_t write_size{0};
  size_t source_offset{0};
  size_t destination_offset{0};
  vk::CommandBuffer commandbuffer;
};

struct memory_buffer_t {
  vk::Buffer buffer;
  vk::DeviceMemory memory;
  size_t memory_size{0};

  void init(memory_buffer_info_t &info);
  void record_write(memory_buffer_write_info_t &info);
  void cleanup(vk::Device device);
};

} // namespace alex
