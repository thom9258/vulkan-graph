#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/overlaypass_builder.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture_storage.hpp>

#include <alex/task_graph.hpp>

#include "alex/flightframe_array.hpp"

#include "../utility/button.hpp"
#include "../utility/scenestack.hpp"
#include "../utility/sdl.hpp"

#include "bitmap.hpp"
// #include "orbit_chest_scene.hpp"
#include "imgui_scene.hpp"
#include "ps1_style_renderer/imgui_context.hpp"
#include "ps1_style_renderer/static_resources.hpp"
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

std::size_t constexpr mb = 1'000'000;

int main() {
  constexpr std::size_t total_memory{10 * mb};
  std::vector<std::uint8_t> memory(total_memory);
  alex::memory::arena init_arena(memory);

  auto program_start_time = std::chrono::high_resolution_clock::now();

  sdl::window_info_t window_info{};
  window_info.name = "ps1_game";
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
  vk::Extent3D render_extent(static_cast<std::int32_t>(window_extent.width / 4),
                             static_cast<std::int32_t>(window_extent.height / 4),
                             1);

  alex::core_t core(core_info);

  /* ****************************************
   * Setup presentation context
   */
  alex::presenter_info_t presenter_info;
  presenter_info.physical_device = core.physical_device();
  presenter_info.device = core.device();
  presenter_info.commandpool = core.commandpool();
  presenter_info.enable_vsync = true;
  presenter_info.window_surface = window_surface.get();
  presenter_info.window_extent.width = window_extent.width;
  presenter_info.window_extent.height = window_extent.height;
  alex::presenter_t presenter(presenter_info);

  game::geometry_rendering_t geometry_rendering(core, render_extent);
  game::debugui_rendering_t debugui_rendering(
      core, vk::Extent3D(static_cast<std::int32_t>(window_extent.width),
                         static_cast<std::int32_t>(window_extent.height), 1));

  game::static_resources_t static_resources(&core);

  /* ****************************************
   * Create Scenes
   */
  scene::scenestack_t scenestack;

  // game::orbit_chest_scene_info_t chest_scene_info;
  // chest_scene_info.name = "Chest scene 1";
  // chest_scene_info.core = &core;
  // chest_scene_info.geometry_rendering = &geometry_rendering;
  // chest_scene_info.presenter = &presenter;
  // chest_scene_info.window = &window;
  // scenestack.put(std::make_unique<game::orbit_chest_scene>(chest_scene_info));

  game::imgui_context_info_t imgui_context_info;
  imgui_context_info.context = &context;
  imgui_context_info.core = &core;
  imgui_context_info.presenter = &presenter;
  imgui_context_info.window = &window;
  imgui_context_info.debugui_rendering = &debugui_rendering;

  game::imgui_context_t imgui_context(imgui_context_info);

  game::imgui_scene_info_t imgui_scene_info;
  imgui_scene_info.core = &core;
  imgui_scene_info.geometry_rendering = &geometry_rendering;
  imgui_scene_info.debugui_rendering = &debugui_rendering;
  imgui_scene_info.presenter = &presenter;
  imgui_scene_info.imgui_context = &imgui_context;
  imgui_scene_info.window = &window;
  imgui_scene_info.static_resources = &static_resources;

  scenestack.put(std::make_unique<game::imgui_scene>(imgui_scene_info));

  {
    auto now = std::chrono::high_resolution_clock::now();
    auto initialization_time =
        std::chrono::duration_cast<std::chrono::duration<double>>(
            now - program_start_time);
    std::println("Initialization time: {}", initialization_time);
  }

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
