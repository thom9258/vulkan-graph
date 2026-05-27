#include "memory_buffer.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

constexpr inline auto get_type_flag(memory_buffer_type_t type)
    -> vk::BufferUsageFlagBits {
  if (type == memory_buffer_type_t::uniform) {
    return vk::BufferUsageFlagBits::eUniformBuffer;
  } else if (type == memory_buffer_type_t::vertices) {
    return vk::BufferUsageFlagBits::eVertexBuffer;
  } else if (type == memory_buffer_type_t::indices) {
    return vk::BufferUsageFlagBits::eIndexBuffer;
  }

  return vk::BufferUsageFlagBits::eUniformBuffer;
}

direct_memory_buffer_t::direct_memory_buffer_t(
    direct_memory_buffer_info_t &info)
    : _device{info.device} {

  vk::BufferUsageFlags const usage_flags =
      get_type_flag(info.buffer_type) | vk::BufferUsageFlagBits::eTransferSrc |
      vk::BufferUsageFlagBits::eTransferDst;

  vk::MemoryPropertyFlags constexpr property_flags =
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent;

  const auto bufferInfo = vk::BufferCreateInfo{}
                              .setSize(info.memory_size)
                              .setUsage(usage_flags)
                              .setSharingMode(vk::SharingMode::eExclusive);

  _buffer = info.device.createBufferUnique(bufferInfo, nullptr);

  vk::PhysicalDeviceMemoryProperties memProperties =
      info.physical_device.getMemoryProperties();
  vk::MemoryRequirements memRequirements =
      info.device.getBufferMemoryRequirements(_buffer.get());

  const auto memoryTypeIndex = find_memory_type(
      memProperties, memRequirements.memoryTypeBits, property_flags);

  auto allocInfo = vk::MemoryAllocateInfo{}
                       .setAllocationSize(memRequirements.size)
                       .setMemoryTypeIndex(memoryTypeIndex);

  _memory = info.device.allocateMemoryUnique(allocInfo, nullptr);
  info.device.bindBufferMemory(_buffer.get(), _memory.get(), 0);
  _memory_size = info.memory_size;
  _memory_ptr = info.device.mapMemory(_memory.get(), 0, _memory_size,
                                      vk::MemoryMapFlags());
}

direct_memory_buffer_t::~direct_memory_buffer_t() {}

auto direct_memory_buffer_t::buffer() -> vk::Buffer { return _buffer.get(); }
auto direct_memory_buffer_t::memory_size() -> std::size_t {
  return _memory_size;
}

auto direct_memory_buffer_t::memory_ptr() -> void * { return _memory_ptr; }

memory_buffer_t::memory_buffer_t(memory_buffer_info_t &info)
    : _device{info.device} {

  vk::BufferUsageFlags const usage_flags =
      get_type_flag(info.buffer_type) | vk::BufferUsageFlagBits::eTransferDst;

  vk::MemoryPropertyFlags constexpr property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;

  const auto bufferInfo = vk::BufferCreateInfo{}
                              .setSize(info.memory_size)
                              .setUsage(usage_flags)
                              .setSharingMode(vk::SharingMode::eExclusive);

  _buffer = info.device.createBufferUnique(bufferInfo, nullptr);

  vk::PhysicalDeviceMemoryProperties memProperties =
      info.physical_device.getMemoryProperties();
  vk::MemoryRequirements memRequirements =
      info.device.getBufferMemoryRequirements(_buffer.get());

  const auto memoryTypeIndex = find_memory_type(
      memProperties, memRequirements.memoryTypeBits, property_flags);

  auto allocInfo = vk::MemoryAllocateInfo{}
                       .setAllocationSize(memRequirements.size)
                       .setMemoryTypeIndex(memoryTypeIndex);

  _memory = info.device.allocateMemoryUnique(allocInfo, nullptr);
  info.device.bindBufferMemory(_buffer.get(), _memory.get(), 0);
  _memory_size = info.memory_size;
}

auto memory_buffer_t::record_write(memory_buffer_write_info_t &info) -> void {
  auto buffercopy = vk::BufferCopy{}
                        .setSrcOffset(info.source_offset)
                        .setDstOffset(info.destination_offset)
                        .setSize(info.write_size);

  info.commandbuffer.copyBuffer(info.direct->buffer(), _buffer.get(),
                                {buffercopy});
}

auto memory_buffer_t::buffer() -> vk::Buffer { return _buffer.get(); }
auto memory_buffer_t::memory_size() -> std::size_t { return _memory_size; }

} // namespace alex
