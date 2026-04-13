#include "memory_buffer.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

void direct_memory_buffer_t::init(direct_memory_buffer_info_t &info) {
  vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eTransferSrc;

  if (info.buffer_type == memory_buffer_type_t::uniform) {
    usage_flags |= vk::BufferUsageFlagBits::eUniformBuffer;
  }
  else if (info.buffer_type == memory_buffer_type_t::vertices) {
    usage_flags |= vk::BufferUsageFlagBits::eVertexBuffer;
  }

  vk::MemoryPropertyFlags constexpr property_flags =
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent;

  const auto bufferInfo = vk::BufferCreateInfo{}
                              .setSize(info.memory_size)
                              .setUsage(usage_flags)
                              .setSharingMode(vk::SharingMode::eExclusive);

  buffer = info.device.createBuffer(bufferInfo, nullptr);

  vk::PhysicalDeviceMemoryProperties memProperties =
      info.physical_device.getMemoryProperties();
  vk::MemoryRequirements memRequirements =
      info.device.getBufferMemoryRequirements(buffer);

  const auto memoryTypeIndex = find_memory_type(
      memProperties, memRequirements.memoryTypeBits, property_flags);

  auto allocInfo = vk::MemoryAllocateInfo{}
                       .setAllocationSize(memRequirements.size)
                       .setMemoryTypeIndex(memoryTypeIndex);

  memory = info.device.allocateMemory(allocInfo, nullptr);
  info.device.bindBufferMemory(buffer, memory, 0);
  memory_size = info.memory_size;
  memory_ptr =
      info.device.mapMemory(memory, 0, memory_size, vk::MemoryMapFlags());
}

void direct_memory_buffer_t::cleanup(vk::Device device) {
  device.unmapMemory(memory);
  device.destroyBuffer(buffer);
  device.freeMemory(memory);
}

void memory_buffer_t::init(memory_buffer_info_t &info) {

  vk::BufferUsageFlags usage_flags = vk::BufferUsageFlagBits::eTransferDst;
  if (info.buffer_type == memory_buffer_type_t::uniform) {
    usage_flags |= vk::BufferUsageFlagBits::eUniformBuffer;
  }
  else if (info.buffer_type == memory_buffer_type_t::vertices) {
    usage_flags |= vk::BufferUsageFlagBits::eVertexBuffer;
  }

  vk::MemoryPropertyFlags constexpr property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;

  const auto bufferInfo = vk::BufferCreateInfo{}
                              .setSize(info.memory_size)
                              .setUsage(usage_flags)
                              .setSharingMode(vk::SharingMode::eExclusive);

  buffer = info.device.createBuffer(bufferInfo, nullptr);

  vk::PhysicalDeviceMemoryProperties memProperties =
      info.physical_device.getMemoryProperties();
  vk::MemoryRequirements memRequirements =
      info.device.getBufferMemoryRequirements(buffer);

  const auto memoryTypeIndex = find_memory_type(
      memProperties, memRequirements.memoryTypeBits, property_flags);

  auto allocInfo = vk::MemoryAllocateInfo{}
                       .setAllocationSize(memRequirements.size)
                       .setMemoryTypeIndex(memoryTypeIndex);

  memory = info.device.allocateMemory(allocInfo, nullptr);
  info.device.bindBufferMemory(buffer, memory, 0);
  memory_size = info.memory_size;
}

void memory_buffer_t::record_write(memory_buffer_write_info_t &info) {
  auto buffercopy = vk::BufferCopy{}
                        .setSrcOffset(info.source_offset)
                        .setDstOffset(info.destination_offset)
                        .setSize(info.write_size);

  info.commandbuffer.copyBuffer(info.memory->buffer, buffer, {buffercopy});
}

} // namespace alex
