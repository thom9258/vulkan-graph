#include "uniform_descriptorsets.hpp"
#include "log.hpp"

#include <vulkan/vulkan_structs.hpp>

#include <ranges>

namespace alex {
#if 0

uniform_descriptorsets_t::uniform_descriptorsets_t(
    uniform_descriptorsets_info_t &info) {

  auto layouts = std::views::repeat(info.layout, info.set_count) |
                 std::ranges::to<std::vector>();

  auto alloc_info = vk::DescriptorSetAllocateInfo{}
                        .setDescriptorPool(info.pool)
                        .setSetLayouts(layouts);

  sets = info.device.allocateDescriptorSetsUnique(alloc_info);
  ALEX_ERROR_IF(sets.size() != info.set_count,
                "could not allocate {} descriptor sets got {} instead",
                info.set_count, sets.size())
}

void uniform_descriptorsets_t::update(
    uniform_descriptorsets_update_info_t &info) {

  ALEX_ERROR_IF(info.set_index >= sets.size(),
                "Set index {} is invalid for sets of size {}", info.set_index,
                sets.size())

  const auto buffer_info = vk::DescriptorBufferInfo{}
                               .setBuffer(info.buffer->buffer())
                               .setOffset(info.buffer_offset)
                               .setRange(info.buffer_size);

  const std::array<vk::WriteDescriptorSet, 1> writes{
      vk::WriteDescriptorSet{}
          .setDstBinding(0)
          .setDstArrayElement(0)
          .setDstSet(sets.at(info.set_index).get())
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setBufferInfo(buffer_info)};

  info.device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
}

vk::DescriptorSet uniform_descriptorsets_t::get_set(std::size_t i) {
  ALEX_ERROR_IF(i >= sets.size(), "Set index {} is invalid for sets of size {}",
                i, sets.size())
  return sets[i].get();
}
#endif

} // namespace alex
