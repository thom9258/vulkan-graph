#pragma once

#include "core.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

enum class memory_buffer_type_t {
  basic,
  uniform,
  vertices,
};

struct direct_memory_buffer_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  memory_buffer_type_t buffer_type{memory_buffer_type_t::basic};
  std::size_t memory_size{1};
};

class direct_memory_buffer_t {
public:
  explicit direct_memory_buffer_t(direct_memory_buffer_info_t &info);
  direct_memory_buffer_t(const direct_memory_buffer_t&) = delete;
  direct_memory_buffer_t(direct_memory_buffer_t&&) = default;
  direct_memory_buffer_t& operator=(const direct_memory_buffer_t&) = delete;
  direct_memory_buffer_t& operator=(direct_memory_buffer_t&&) = default;
  ~direct_memory_buffer_t();

  auto buffer() -> vk::Buffer;
  auto memory_size() -> std::size_t;
  auto memory_ptr() -> void*;

private:
  vk::Device _device;
  vk::UniqueBuffer _buffer;
  vk::UniqueDeviceMemory _memory;
  size_t _memory_size{0};
  void *_memory_ptr{nullptr};
};

struct memory_buffer_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  memory_buffer_type_t buffer_type{memory_buffer_type_t::basic};
  size_t memory_size{0};
};

struct memory_buffer_write_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  direct_memory_buffer_t *direct;
  size_t write_size{0};
  size_t source_offset{0};
  size_t destination_offset{0};
  vk::CommandBuffer commandbuffer;
};

class memory_buffer_t {
public:
  explicit memory_buffer_t(memory_buffer_info_t &info);
  memory_buffer_t(const memory_buffer_t&) = delete;
  memory_buffer_t(memory_buffer_t&&) = default;
  memory_buffer_t& operator=(const memory_buffer_t&) = delete;
  memory_buffer_t& operator=(memory_buffer_t&&) = default;
  ~memory_buffer_t() = default;

  auto record_write(memory_buffer_write_info_t &info) -> void;
  auto buffer() -> vk::Buffer;
  auto memory_size() -> std::size_t;

private:
  vk::Device _device;
  vk::UniqueBuffer _buffer;
  vk::UniqueDeviceMemory _memory;
  size_t _memory_size{0};
};

} // namespace alex
