#include "static_resources.hpp"
#include "alex/texture.hpp"

#include <exception>

namespace game {

static_resources_t::static_resources_t(alex::core_t *core) : _core{core} {}

auto static_resources_t::chest_texture() -> alex::texture_t * {
  load_chest_texture();

  return &_chest.diffuse_texture.value();
}

auto static_resources_t::load_chest_texture() -> void {
  if (_chest.diffuse_texture.has_value()) {
    return;
  }

  auto chest_diffuse_bitmap = game::bitmap_t::create(
      "/home/th/Assets/ChestWowStyle/diffuse.tga", game::bitmap_format_t::rgb);

  if (!chest_diffuse_bitmap.has_value()) {
    throw std::runtime_error("Could not load chest diffuse bitmap");
  }

  alex::direct_memory_buffer_t chest_diffuse_buffer =
      chest_diffuse_bitmap->make_direct_buffer(_core->physical_device(),
                                               _core->device());

  const vk::Format chest_diffuse_texture_format =
      game::to_vk_format(chest_diffuse_bitmap->format());

  alex::texture_info_t chest_diffuse_texture_info;
  chest_diffuse_texture_info.physical_device = _core->physical_device();
  chest_diffuse_texture_info.device = _core->device();
  chest_diffuse_texture_info.extent =
      vk::Extent2D{static_cast<std::uint32_t>(chest_diffuse_bitmap->width()),
                   static_cast<std::uint32_t>(chest_diffuse_bitmap->height())};
  chest_diffuse_texture_info.format = chest_diffuse_texture_format;
  chest_diffuse_texture_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
  chest_diffuse_texture_info.usage =
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

  _chest.diffuse_texture.emplace(chest_diffuse_texture_info);

  _core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
    // Transition image to color override
    {
      auto range = vk::ImageSubresourceRange{}
                       .setAspectMask(vk::ImageAspectFlagBits::eColor)
                       .setBaseMipLevel(0)
                       .setLevelCount(1)
                       .setBaseArrayLayer(0)
                       .setLayerCount(1);

      auto barrier = vk::ImageMemoryBarrier{}
                         .setOldLayout(vk::ImageLayout::eUndefined)
                         .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                         .setImage(_chest.diffuse_texture->image())
                         .setSubresourceRange(range)
                         .setSrcAccessMask(vk::AccessFlags())
                         .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }

    // Copy buffer into image
    {
      auto layer = vk::ImageSubresourceLayers{}
                       .setAspectMask(vk::ImageAspectFlagBits::eColor)
                       .setMipLevel(0)
                       .setBaseArrayLayer(0)
                       .setLayerCount(1);

      const auto offset = vk::Offset3D{}.setX(0).setY(0).setZ(0);

      const auto extent = vk::Extent3D{}
                              .setWidth(chest_diffuse_bitmap->width())
                              .setHeight(chest_diffuse_bitmap->height())
                              .setDepth(1);

      auto region = vk::BufferImageCopy{}
                        .setBufferOffset(0)
                        .setBufferRowLength(0)
                        .setBufferImageHeight(0)
                        .setImageSubresource(layer)
                        .setImageOffset(offset)
                        .setImageExtent(extent);

      commandbuffer.copyBufferToImage(
          chest_diffuse_buffer.buffer(), _chest.diffuse_texture->image(),
          vk::ImageLayout::eTransferDstOptimal, region);
    }

    // Transfer image to shader readonly optimal
    {
      const auto source_range =
          vk::ImageSubresourceRange{}
              .setAspectMask(vk::ImageAspectFlagBits::eColor)
              .setBaseMipLevel(0)
              .setLevelCount(1)
              .setBaseArrayLayer(0)
              .setLayerCount(1);

      auto barrier = vk::ImageMemoryBarrier{}
                         .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                         .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                         .setImage(_chest.diffuse_texture->image())
                         .setSubresourceRange(source_range)
                         .setSrcAccessMask(vk::AccessFlags())
                         .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }
  });
}

auto static_resources_t::load_chest_model() -> void {
  if (_chest.model.has_value()) {
    return;
  }

  model_load_info_t chest_load_info;
  chest_load_info.core = _core;
  chest_load_info.path = "/home/th/Assets/ChestWowStyle/Chest.obj";
  auto chest = model_source_t::create(chest_load_info);
  if (!chest.has_value()) {
    throw std::runtime_error(std::format(
        "static resource load error: [code: {}] {}",
        game::to_string(chest.error().code()), chest.error().error()));
  }

  _chest.model = std::move(chest.value());
}

auto static_resources_t::chest_model() -> model_source_t * {
  load_chest_model();
  return &_chest.model.value();
}

auto static_resources_t::load_chest_texture_sampler() -> void {
  if (_chest.diffuse_texture_sampler.has_value()) {
    return;
  }

  const auto features = _core->physical_device().getFeatures();
  const auto properties = _core->physical_device().getProperties();
  const auto max_anisotropy =
      features.samplerAnisotropy
          ? std::min(4.0f, properties.limits.maxSamplerAnisotropy)
          : 1.0f;

  const vk::Filter filter = vk::Filter::eNearest;
  const auto sampler_info =
      vk::SamplerCreateInfo{}
          .setMagFilter(filter)
          .setMinFilter(filter)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(features.samplerAnisotropy)
          .setMaxAnisotropy(max_anisotropy)
          .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
          .setUnnormalizedCoordinates(false)
          .setCompareEnable(false)
          .setCompareOp(vk::CompareOp::eAlways)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setMipLodBias(0.0f)
          .setMinLod(0.0f)
          .setMaxLod(0.0f);

  _chest.diffuse_texture_sampler =
      _core->device().createSamplerUnique(sampler_info);
}

auto static_resources_t::chest_texture_sampler() -> vk::Sampler {
  load_chest_texture();
  load_chest_texture_sampler();
  return _chest.diffuse_texture_sampler.value().get();
}

} // namespace game
