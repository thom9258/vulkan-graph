#pragma once

#include <alex/core.hpp>
#include <alex/presentation_context.hpp>

#include <imgui.h>
#include <vulkan/vulkan_to_string.hpp>

#include "../utility/sdl.hpp"
#include "rendering.hpp"

namespace game {

struct imgui_context_info_t {
  float ui_scale{1.0f};
  alex::context_t *context{nullptr};
  alex::core_t *core{nullptr};
  alex::presenter_t *presenter{nullptr};
  sdl::window_t *window{nullptr};
  debugui_rendering_t *debugui_rendering{nullptr};
};

class imgui_context_t {

public:
  imgui_context_t(imgui_context_info_t &info);
  ~imgui_context_t();

  imgui_context_t(const imgui_context_t &info) = delete;
  imgui_context_t(imgui_context_t &&info) = delete;
  imgui_context_t &operator=(const imgui_context_t &info) = delete;
  imgui_context_t &operator=(imgui_context_t &&info) = delete;

  auto new_frame() -> void;
  auto process_event(const SDL_Event *e) -> bool;
  auto render_draw_data(ImDrawData *draw_data, VkCommandBuffer command_buffer,
                        VkPipeline pipeline = VK_NULL_HANDLE) -> void;

private:
  vk::UniqueDescriptorPool _descriptor_pool;
};

} // namespace game
