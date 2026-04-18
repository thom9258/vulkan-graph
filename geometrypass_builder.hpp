#pragma once

#include "core.hpp"

namespace alex::graph {

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

struct geometrypass_t {
  geometrypass_t() = default;
  geometrypass_t(geometrypass_info_t &info);
  vk::RenderPass renderpass;
  vk::Extent2D extent;
  std::array<vk::ClearValue, 2> clearvalues;
  flightframe_array_t<vk::Framebuffer> framebuffers;
};

} // namespace alex::graph
