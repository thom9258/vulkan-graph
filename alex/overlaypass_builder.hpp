#pragma once

#include "core.hpp"
#include "flightframe_array.hpp"

namespace alex {

struct overlaypass_info_t {
  overlaypass_info_t(vk::Device device);

  overlaypass_info_t &
  set_color_attachments(flightframe_array_t<vk::ImageView> attachments);
  overlaypass_info_t &set_color_format(vk::Format format);
  overlaypass_info_t &set_loadop(vk::AttachmentLoadOp op);
  overlaypass_info_t &set_extent(vk::Extent3D extent);

  vk::Device device;
  vk::Extent3D extent;
  flightframe_array_t<vk::ImageView> color_attachments;
  vk::Format color_format;
  vk::AttachmentLoadOp load_op;
};

class overlaypass_t {
public:
  overlaypass_t(overlaypass_info_t &info);

  auto renderpass() -> vk::RenderPass;
  auto framebuffer(std::uint32_t frame_in_flight) -> vk::Framebuffer;
  auto extent() -> vk::Extent3D;

private:
  vk::UniqueRenderPass _renderpass;
  flightframe_array_t<vk::UniqueFramebuffer> _framebuffers;
  vk::Extent3D _extent;
};

} // namespace alex
