#pragma once

#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/overlaypass_builder.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture.hpp>

#include <ranges>

namespace game {

struct debugui_rendering_t {
  vk::Extent3D _extent;

  struct attachments_t {
    std::vector<alex::texture_t> color;
  } attachments;

  std::optional<alex::overlaypass_t> renderpass;

  constexpr debugui_rendering_t(alex::core_t &core, vk::Extent3D extent)
      : _extent{extent} {
    alex::texture_info_t colorattachment_info;
    colorattachment_info.physical_device = core.physical_device();
    colorattachment_info.device = core.device();
    colorattachment_info.extent.setWidth(_extent.width)
        .setHeight(_extent.height);

    colorattachment_info.format = vk::Format::eR8G8B8A8Srgb;
    colorattachment_info.tiling = vk::ImageTiling::eOptimal;
    colorattachment_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
    colorattachment_info.property_flags =
        vk::MemoryPropertyFlagBits::eDeviceLocal;
    colorattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                                 vk::ImageUsageFlagBits::eTransferSrc |
                                 vk::ImageUsageFlagBits::eSampled |
                                 vk::ImageUsageFlagBits::eColorAttachment;

    for (auto _ :
         std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
      attachments.color.emplace_back(colorattachment_info);
    }

    auto const get_view = [](alex::texture_t &texture) {
      return texture.view();
    };

    auto color_views = attachments.color | std::views::transform(get_view) |
                       std::ranges::to<std::vector>();

    alex::flightframe_array_t<vk::ImageView> colorattachment_views;
    for (auto [i, view] : colorattachment_views | std::views::enumerate) {
      view = attachments.color[i].view();
    }

    auto renderpass_info = alex::overlaypass_info_t(core.device())
                               .set_color_attachments(colorattachment_views)
                               .set_color_format(vk::Format::eR8G8B8A8Srgb)
                               .set_extent(_extent)
                               .set_loadop(vk::AttachmentLoadOp::eDontCare);

    renderpass.emplace(renderpass_info);
  }
};

} // namespace game
