#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/overlaypass_builder.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture_storage.hpp>

#include <alex/log.hpp>
#include <alex/task_graph.hpp>

#include "alex/flightframe_array.hpp"

#include "../utility/button.hpp"
#include "../utility/scenestack.hpp"
#include "../utility/sdl.hpp"
#include "../utility/timer.hpp"

#include "bitmap.hpp"
#include "imgui_scene.hpp"
#include "ui/imgui_context.hpp"
#include "world_creator/renderer.hpp"

#include <chrono>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

using namespace std::literals;

int main() {
  utility::timer_t engine_init_timer;

  sdl::window_info_t window_info{};
  window_info.name = "world-builder";
  window_info.x = -1;
  window_info.y = -1;
  window_info.width = 320 * 4;
  window_info.height = 240 * 4;

  sdl::window_t window(window_info);
  sdl::window_extent_t window_extent = window.window_extent();

  std::vector<const char *> window_extensions = window.instance_extensions();

  alex::context_info_t context_info;
  context_info.instance_extensions = window_extensions;
  context_info.enable_validation = true;

  alex::context_t context(context_info);

  vk::UniqueSurfaceKHR window_surface =
      window.create_window_surface(context.instance());

  alex::core_info_t core_info;
  core_info.surface = window_surface.get();
  core_info.instance = context.instance();
  alex::core_t core(core_info);

  /* ****************************************
   * Setup presentation context
   */
  alex::presenter_info_t presenter_info;
  presenter_info.physical_device = core.physical_device();
  presenter_info.device = core.device();
  presenter_info.commandpool = core.commandpool();
  presenter_info.enable_vsync = false;
  presenter_info.window_surface = window_surface.get();
  presenter_info.window_extent.width = window_extent.width;
  presenter_info.window_extent.height = window_extent.height;
  alex::presenter_t presenter(presenter_info);

  auto const render_extent =
      vk::Extent2D(static_cast<std::int32_t>(window_extent.width),
                   static_cast<std::int32_t>(window_extent.height));
  auto const display_extent =
      vk::Extent2D(static_cast<std::int32_t>(window_extent.width),
                   static_cast<std::int32_t>(window_extent.height));

  game::renderer_info_t renderer_info;
  renderer_info.physical_device = core.physical_device();
  renderer_info.device = core.device();
  renderer_info.render_extent = render_extent;
  renderer_info.display_extent = display_extent;
  game::renderer_t renderer(renderer_info);

// game::static_render_t static_render(core, render_extent);
// game::debugui_rendering_t debugui_rendering(
//     core, vk::Extent3D(static_cast<std::int32_t>(window_extent.width),
//                        static_cast<std::int32_t>(window_extent.height), 1));

  ALEX_INFO("Engine load time: {}ms", engine_init_timer.elapsed_ms());

  /* ****************************************
   * Create Scenes
   */
  scene::scenestack_t scenestack;

  utility::timer_t imgui_scene_init_timer;
  game::imgui_context_info_t imgui_context_info;
  imgui_context_info.context = &context;
  imgui_context_info.ui_scale = 0.75f;
  imgui_context_info.core = &core;
  imgui_context_info.presenter = &presenter;
  imgui_context_info.window = &window;
  imgui_context_info.renderpass = renderer.debugui_renderpass();

  game::imgui_context_t imgui_context(imgui_context_info);

  game::imgui_scene_info_t imgui_scene_info;
  imgui_scene_info.core = &core;
  imgui_scene_info.renderer = &renderer;
  imgui_scene_info.presenter = &presenter;
  imgui_scene_info.imgui_context = &imgui_context;
  imgui_scene_info.window = &window;

  scenestack.put(std::make_unique<game::imgui_scene>(imgui_scene_info));

  ALEX_INFO("Imgui Scene init time: {}ms", imgui_scene_init_timer.elapsed_ms());

  bool running = true;
  while (running) {
    scene::status_t status = scenestack.tick();
    if (status != scene::status_t::ok) {
      running = false;
    }
  }

  core.device().waitIdle();
  return 0;
}
