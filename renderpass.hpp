#pragma once

#include "core.hpp"

namespace alex {

struct renderpass_info_t {
  core_t *core;
  vk::Extent2D extent;
  flightframe_array_t<std::span<vk::ImageView>> attachments;
};

struct renderpass_t {
  vk::RenderPass renderpass;
  flightframe_array_t<vk::Framebuffer> framebuffers;

  void init(renderpass_info_t &info);
};

} // namespace alex
