#pragma once

#include "core.hpp"

namespace alex {

struct renderpass_info_t {
  vk::Device device;
  vk::Extent2D extent;

  struct attachments_t {
    vk::ImageView color;
    vk::ImageView depth;
  };

  flightframe_array_t<attachments_t> attachments;
  vk::AttachmentLoadOp load_op;
  std::array<vk::ClearValue, 2> clearvalues;
};

struct renderpass_t {
  vk::RenderPass renderpass;
  vk::Extent2D extent;
  flightframe_array_t<vk::Framebuffer> framebuffers;

  void init(renderpass_info_t &info);
};

} // namespace alex
