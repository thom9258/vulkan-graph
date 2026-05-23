#include "texture.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>

#include <print>

namespace alex {

texture_t::texture_t(texture_info_t &info) {
  const auto imageCreateInfo =
      vk::ImageCreateInfo{}
          .setImageType(vk::ImageType::e2D)
          .setFormat(info.format)
          .setExtent(vk::Extent3D(info.extent, 1))
          .setMipLevels(1)
          .setArrayLayers(1)
          .setTiling(info.tiling)
          .setUsage(info.usage)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setSamples(vk::SampleCountFlagBits::e1);

  _image = info.device.createImageUnique(imageCreateInfo);

  vk::PhysicalDeviceMemoryProperties memProperties =
      info.physical_device.getMemoryProperties();

  vk::MemoryRequirements memRequirements =
      info.device.getImageMemoryRequirements(_image.get());

  const auto memoryTypeIndex = find_memory_type(
      memProperties, memRequirements.memoryTypeBits, info.property_flags);

  auto allocInfo = vk::MemoryAllocateInfo{}
                       .setAllocationSize(memRequirements.size)
                       .setMemoryTypeIndex(memoryTypeIndex);
  _memory = info.device.allocateMemoryUnique(allocInfo, nullptr);

  info.device.bindImageMemory(_image.get(), _memory.get(), 0);

  auto subresourceRange = vk::ImageSubresourceRange{}
                              .setAspectMask(info.aspect_flags)
                              .setBaseMipLevel(0)
                              .setLevelCount(1)
                              .setBaseArrayLayer(0)
                              .setLayerCount(1);

  auto componentMapping = vk::ComponentMapping{}
                              .setR(vk::ComponentSwizzle::eIdentity)
                              .setG(vk::ComponentSwizzle::eIdentity)
                              .setB(vk::ComponentSwizzle::eIdentity)
                              .setA(vk::ComponentSwizzle::eIdentity);

  auto imageViewCreateInfo = vk::ImageViewCreateInfo{}
                                 .setSubresourceRange(subresourceRange)
                                 .setViewType(vk::ImageViewType::e2D)
                                 .setFormat(info.format)
                                 .setComponents(componentMapping)
                                 .setImage(_image.get());

  _view = info.device.createImageViewUnique(imageViewCreateInfo);
}

auto texture_t::memory() -> vk::DeviceMemory { return _memory.get(); }
auto texture_t::view() -> vk::ImageView { return _view.get(); }
auto texture_t::image() -> vk::Image { return _image.get(); }

} // namespace alex
