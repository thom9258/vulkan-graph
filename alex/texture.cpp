#include "texture.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

void texture_t::init(texture_info_t& info)
{
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

  image = info.device.createImage(imageCreateInfo);

  vk::PhysicalDeviceMemoryProperties memProperties =
      info.physical_device.getMemoryProperties();

  vk::MemoryRequirements memRequirements =
      info.device.getImageMemoryRequirements(image);

  const auto memoryTypeIndex = find_memory_type(
      memProperties, memRequirements.memoryTypeBits, info.property_flags);

  auto allocInfo = vk::MemoryAllocateInfo{}
                       .setAllocationSize(memRequirements.size)
                       .setMemoryTypeIndex(memoryTypeIndex);
  memory = info.device.allocateMemory(allocInfo, nullptr);

  info.device.bindImageMemory(image, memory, 0);

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
                                 .setImage(image);

  view = info.device.createImageView(imageViewCreateInfo);
}

void texture_t::cleanup(vk::Device device) {
  device.destroyImage(image);
  device.freeMemory(memory);
}

} // namespace alex
