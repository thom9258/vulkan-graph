#pragma once

#include "core.hpp"
#include "flightframe_array.hpp"

namespace alex {

struct geometrypass_info_t {
  geometrypass_info_t(vk::Device device);

  geometrypass_info_t &
  set_color_attachments(flightframe_array_t<vk::ImageView> attachments);
  geometrypass_info_t &
  set_depth_attachments(flightframe_array_t<vk::ImageView> attachments);

  geometrypass_info_t &set_color_format(vk::Format format);
  geometrypass_info_t &set_depth_format(vk::Format format);

  geometrypass_info_t &set_color_clearvalue(float r, float g, float b, float a);
  geometrypass_info_t &set_depth_clearvalue(float d);
  geometrypass_info_t &set_loadop(vk::AttachmentLoadOp op);
  geometrypass_info_t &set_extent(vk::Extent3D extent);

  vk::Device device;
  vk::Extent3D extent;
  flightframe_array_t<vk::ImageView> color_attachments;
  flightframe_array_t<vk::ImageView> depth_attachments;
  vk::Format color_format;
  vk::Format depth_format;
  vk::AttachmentLoadOp load_op;
  std::array<vk::ClearValue, 2> clearvalues;
};

class geometrypass_t {
public:
  geometrypass_t(geometrypass_info_t &info);

  auto renderpass() -> vk::RenderPass;
  auto framebuffer(std::uint32_t frame_in_flight) -> vk::Framebuffer;
  auto extent() -> vk::Extent3D;
  auto clearvalues() -> std::array<vk::ClearValue, 2>;

  auto set_color_clearvalue(float r, float g, float b) -> void;

private:
  vk::UniqueRenderPass _renderpass;
  flightframe_array_t<vk::UniqueFramebuffer> _framebuffers;
  vk::Extent3D _extent;
  std::array<vk::ClearValue, 2> _clearvalues;
};

} // namespace alex
