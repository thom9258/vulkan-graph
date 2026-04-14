#include "core.hpp"
#include "drawing.hpp"
#include "ensure.hpp"
#include "geometry_primitives.hpp"
#include "graph.hpp"
#include "memory_buffer.hpp"
#include "pipeline_builder.hpp"
#include "presentation_context.hpp"
#include "renderpass_builder.hpp"
#include "texture_storage.hpp"
#include "uniform_descriptorsets.hpp"

#include "glm.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <SDL_video.h>
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

auto poll_all_sdl_events() -> std::vector<SDL_Event> {
  std::vector<SDL_Event> events;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    events.push_back(event);
  }

  return events;
};

std::size_t constexpr mb = 1'000'000;

int main() {
  global::set_log_level(LogLevel::Info);

  constexpr std::size_t total_memory{10 * mb};
  std::vector<std::uint8_t> memory(total_memory);
  alex::memory::arena init_arena(memory);

  auto start_time = std::chrono::high_resolution_clock::now();

  if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
    LOG_CRITICAL("Could not init sdl!");
  }

  SDL_Vulkan_LoadLibrary(nullptr);

  struct {
    std::string_view name{"graph"};
    int x{-1};
    int y{-1};
    int width{800};
    int height{600};
  } window_info;

  SDL_Window *window = SDL_CreateWindow(
      window_info.name.data(), window_info.x, window_info.y, window_info.width,
      window_info.height, 0 | SDL_WINDOW_VULKAN);

  if (!window) {
    LOG_CRITICAL("Could not create window!");
  }

  uint32_t window_extensions_count{0};
  SDL_Vulkan_GetInstanceExtensions(window, &window_extensions_count, nullptr);
  const uint32_t extensions_count = window_extensions_count;
  auto extensions = init_arena.allocate<const char *>(extensions_count);
  ENSURE_NOT(extensions.empty(), "Out of memory");

  SDL_Vulkan_GetInstanceExtensions(window, &window_extensions_count,
                                   extensions.data());

  alex::context_info_t context_info;
  context_info.instance_extensions = extensions;
  context_info.enable_validation = true;

  alex::context_t context;
  context.init(context_info, init_arena);

  VkSurfaceKHR surface;
  SDL_Vulkan_CreateSurface(window, context.instance, &surface);

  alex::core_info_t core_info;
  core_info.surface = surface;
  core_info.context = &context;
  core_info.render_extent.width = window_info.width;
  core_info.render_extent.height = window_info.height;

  alex::core_t core;
  core.init(core_info, init_arena);

  int width{0};
  int height{0};
  SDL_GetWindowSize(window, &width, &height);

  /* ****************************************
   * Initialization Commandbuffer Setup
   *
   * TODO: everything done by this commandbuffer should be able to be put
   *       into a work graph, where we do not specify the commandbuffer.
   *       This is just a preliminary test implementation.
   */
  auto init_commandbuffer_alloc_info =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(core.commandpool)
          .setLevel(vk::CommandBufferLevel::ePrimary)
          .setCommandBufferCount(1);

  vk::CommandBuffer init_commandbuffer =
      core.device.allocateCommandBuffers(init_commandbuffer_alloc_info).front();

  init_commandbuffer.begin(vk::CommandBufferBeginInfo{});

  /* ****************************************
   * Set up a staging arena for hostvisible GPU memory
   * Using this we can quickly load vertices and indices
   * into proper staging memory.
   */
  alex::direct_memory_buffer_info_t staging_arena_memory_info;
  staging_arena_memory_info.physical_device = core.physical_device;
  staging_arena_memory_info.device = core.device;
  staging_arena_memory_info.buffer_type = alex::memory_buffer_type_t::basic;
  staging_arena_memory_info.memory_size = 10 * mb;
  alex::direct_memory_buffer_t staging_arena_memory;
  staging_arena_memory.init(staging_arena_memory_info);

  alex::memory::arena init_staging_arena(
      {static_cast<uint8_t *>(staging_arena_memory.memory_ptr),
       staging_arena_memory.memory_size});

  /* ****************************************
   * Create DescriptorSet Layout for geometry pipeline
   */
  std::array<vk::DescriptorSetLayoutBinding,
             1> constexpr frame_uniform_bindings{
      vk::DescriptorSetLayoutBinding{}
          .setStageFlags(vk::ShaderStageFlagBits::eVertex)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setBinding(0)
          .setDescriptorCount(1)};

  const auto uniform_setinfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setFlags(vk::DescriptorSetLayoutCreateFlags())
          .setBindings(frame_uniform_bindings);

  vk::DescriptorSetLayout geometry_pipeline_info_setlayout =
      core.device.createDescriptorSetLayout(uniform_setinfo, nullptr);

  /* ****************************************
   * Vertex Buffer Setup
   */
  std::span<alex::vertex_t> cube_vertices = load_cube(init_arena, 0.0f, 1.0f, 0.0f);
  alex::direct_memory_buffer_info_t direct_cube_buffer_info;
  direct_cube_buffer_info.physical_device = core.physical_device;
  direct_cube_buffer_info.device = core.device;
  direct_cube_buffer_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_cube_buffer_info.memory_size =
      sizeof(cube_vertices[0]) * cube_vertices.size();

  alex::direct_memory_buffer_t direct_cube_buffer;
  direct_cube_buffer.init(direct_cube_buffer_info);
  std::memcpy(direct_cube_buffer.memory_ptr, cube_vertices.data(),
              direct_cube_buffer.memory_size);
  LOG_INFO("Created direct vertex buffer");

  alex::memory_buffer_info_t cube_buffer_info;
  cube_buffer_info.physical_device = core.physical_device;
  cube_buffer_info.device = core.device;
  cube_buffer_info.buffer_type = alex::memory_buffer_type_t::vertices;
  cube_buffer_info.memory_size = direct_cube_buffer_info.memory_size;

  alex::memory_buffer_write_info_t cube_buffer_write_info;
  cube_buffer_write_info.physical_device = core.physical_device;
  cube_buffer_write_info.device = core.device;
  cube_buffer_write_info.memory = &direct_cube_buffer;
  cube_buffer_write_info.write_size = direct_cube_buffer.memory_size;
  cube_buffer_write_info.commandbuffer = init_commandbuffer;

  alex::memory_buffer_t cube_buffer;
  cube_buffer.init(cube_buffer_info);
  cube_buffer.record_write(cube_buffer_write_info);
  LOG_INFO("Created cube vertex buffer");

  /* ****************************************
   * Uniform Buffers Setup
   */
  struct draw_info_t {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 model;
  };
  draw_info_t cube_draw_info;
  cube_draw_info.view =
      glm::lookAt(glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));

  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const float near_plane = 1.0f, far_plane = 20.0f;
  cube_draw_info.projection =
      glm::perspective(glm::radians(70.f), aspect, near_plane, far_plane);

  cube_draw_info.model = glm::mat4(1.0f);

  alex::direct_memory_buffer_info_t direct_uniform_info;
  direct_uniform_info.physical_device = core.physical_device;
  direct_uniform_info.device = core.device;
  direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_uniform_info.memory_size = sizeof(cube_draw_info);

  alex::flightframe_array_t<alex::direct_memory_buffer_t> direct_uniforms;
  for (alex::direct_memory_buffer_t &uniform : direct_uniforms) {
    uniform.init(direct_uniform_info);
    std::memcpy(uniform.memory_ptr, &cube_draw_info, sizeof(cube_draw_info));
  };

  alex::flightframe_array_t<alex::memory_buffer_t> uniforms;

  for (auto [i, uniform] : uniforms | std::views::enumerate) {
    alex::memory_buffer_info_t uniform_info;
    uniform_info.physical_device = core.physical_device;
    uniform_info.device = core.device;
    uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
    uniform_info.memory_size = direct_uniform_info.memory_size;
    uniform.init(uniform_info);

    alex::memory_buffer_write_info_t write_info;
    write_info.physical_device = core.physical_device;
    write_info.device = core.device;
    write_info.memory = &direct_uniforms[i];
    write_info.write_size = uniform.memory_size;
    write_info.commandbuffer = init_commandbuffer;
    uniform.record_write(write_info);
  }

  std::vector<vk::DescriptorPoolSize> const pool_sizes{
      vk::DescriptorPoolSize{}.setDescriptorCount(4).setType(
          vk::DescriptorType::eUniformBuffer)};

  auto pool_create_info =
      vk::DescriptorPoolCreateInfo{}.setPoolSizes(pool_sizes).setMaxSets(4);

  vk::DescriptorPool descriptor_pool =
      core.device.createDescriptorPool(pool_create_info);

  alex::uniform_descriptorsets_info_t cube_descriptorsets_info;
  cube_descriptorsets_info.physical_device = core.physical_device;
  cube_descriptorsets_info.device = core.device;
  cube_descriptorsets_info.set_count = 2;
  cube_descriptorsets_info.layout = geometry_pipeline_info_setlayout;
  cube_descriptorsets_info.pool = descriptor_pool;

  alex::uniform_descriptorsets_t cube_descriptorsets;
  cube_descriptorsets.init(cube_descriptorsets_info);

  for (auto [i, uniform] : uniforms | std::views::enumerate) {
    alex::uniform_descriptorsets_update_info_t update_info;
    update_info.device = core.device;
    update_info.set_index = i;
    update_info.buffer = &uniform;
    update_info.buffer_offset = 0;
    update_info.buffer_size = uniforms[i].memory_size;
    cube_descriptorsets.update(update_info);
  };

  /* ****************************************
   * Wait for init commands to finish
   */
  init_commandbuffer.end();

  auto fence_create_info = vk::FenceCreateInfo{};
  vk::Fence init_fence = core.device.createFence(fence_create_info);

  auto commandbuffer_submit_info =
      vk::SubmitInfo{}.setCommandBuffers(init_commandbuffer);

  core.queue.submit(commandbuffer_submit_info, init_fence);

  const auto max_wait = std::numeric_limits<unsigned int>::max();
  vk::Result init_wait_result =
      core.device.waitForFences(init_fence, true, max_wait);
  ENSURE(init_wait_result == vk::Result::eSuccess, "could not wait for queue")

  /* ****************************************
   * Setup presentation context
   */
  alex::presentation_context_info_t presentation_context_info;
  presentation_context_info.core = &core;
  presentation_context_info.surface = surface;
  presentation_context_info.enable_vsync = false;
  presentation_context_info.window_extent.width = width;
  presentation_context_info.window_extent.height = height;
  alex::presentation_context_t presentation_context;
  presentation_context.init(presentation_context_info, init_arena);

  /* ****************************************
   * Setup render graph
   */
  vk::Extent3D render_extent(presentation_context.window_extent.width,
                             presentation_context.window_extent.height, 1);

  auto graph_info =
      alex::graph::graph_info_t(core.physical_device, core.device);

  graph_info.add_attachment("geom-color", alex::graph::attachment_type_t::color)
      .set_format(vk::Format::eR8G8B8A8Srgb)
      .set_extent(render_extent)
      .set_aspect_flags(vk::ImageAspectFlagBits::eColor);

  graph_info.add_attachment("geom-depth", alex::graph::attachment_type_t::depth)
      .set_format(vk::Format::eD32Sfloat)
      .set_extent(render_extent)
      .set_aspect_flags(vk::ImageAspectFlagBits::eDepth);

  graph_info.add_framepass("geometry-pass")
      .set_color_attachment("geom-color")
      .set_depth_attachment("geom-depth")
      .set_extent(render_extent)
      .set_vertex_program_path("./geometry.vert.spv")
      .set_fragment_program_path("./geometry.frag.spv");

  alex::graph::graph_t graph(graph_info);
  std::println("======================");
  graph.print_execution_order(std::cout);
  std::println("======================");
  graph.print_graphviz(std::cout);
  std::println("======================");

  /* ****************************************
   * Pipeline Setup using the created graph renderpasses
   */

  alex::graph::renderpass_node_t *geometry_renderpass =
      graph.find_node("geometry-pass");
  ENSURE(geometry_renderpass != nullptr, "could not find geometry-pass")

  auto geometry_pipeline_info =
      alex::graph::pipeline_info_t(core.device)
          .set_extent(render_extent)
          .set_renderpass(geometry_renderpass->geometry_pass.renderpass)
          .set_vertex_program_path("./geometry.vert.spv")
          .set_fragment_program_path("./geometry.frag.spv")
          .add_setlayout(geometry_pipeline_info_setlayout);

  alex::graph::pipeline_t geometry_pipeline(geometry_pipeline_info, init_arena);

  {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto time_ns = end_time - start_time;
    auto time = std::chrono::duration_cast<
        std::chrono::duration<double /*, std::milli*/>>(time_ns);
    std::println("=============================");
    std::println("Initialization Info:");
    std::println("Execution Time: {}", time);

    std::println("Memory footprint:");
    std::println("  Used {} bytes", init_arena.used_memory());
    std::println("  Available {} bytes", init_arena.available_memory());
    std::println("  Total {} bytes", init_arena.total_memory());
  }

  bool running = true;
  while (running) {
    std::vector<SDL_Event> events = poll_all_sdl_events();

    for (SDL_Event event : events) {
      switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          running = false;
          break;
        }
      }
    }

    alex::next_frame_info_t next_frame_info =
        presentation_context.wait_for_next_frame(core.device);

    alex::graph::record_info_t graph_record_info;
    graph_record_info.commandbuffer =
        next_frame_info.presentation_commandbuffer;
    graph_record_info.flightframe = next_frame_info.flightframe;

    std::vector<alex::graph::renderpass_command_t> geometry_pass_commands{
        alex::graph::command::bind_pipeline_t{geometry_pipeline.pipeline},
        alex::graph::command::bind_descriptorsets_t{
            .layout = geometry_pipeline.layout,
            .sets = {cube_descriptorsets.get_set(next_frame_info.flightframe)},
        },
        alex::graph::command::bind_vertexbuffer_t{
            .first_binding = 0,
            .binding_offsets = {0},
            .first_buffer = 0,
            .buffers = {cube_buffer.buffer}},
        alex::graph::command::draw_t{
            .instance_count = 1,
            .first_instance = 0,
            .vertex_count = static_cast<std::uint32_t>(cube_vertices.size()),
            .first_vertex = 0}};

    graph_record_info.renderpass_commands.emplace_back("geometry-pass",
                                                       geometry_pass_commands);
    graph.record(graph_record_info);

    alex::presentation_info_t presentation_info;
    presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.source_offset_end = vk::Offset3D{width, height, 1};

    presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.destination_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(presentation_context.window_extent.width),
        static_cast<std::int32_t>(presentation_context.window_extent.height),
        1};

    presentation_info.blit_filter = vk::Filter::eLinear;
    std::span<alex::texture_t> final_images =
        graph.m_texture_storage.find("geom-color");
    presentation_info.image = final_images[next_frame_info.flightframe].image;
    presentation_info.layout = vk::ImageLayout::eColorAttachmentOptimal;
    presentation_info.queue = core.queue;
    presentation_info.commandbuffer =
        next_frame_info.presentation_commandbuffer;

    presentation_context.present(presentation_info);
  }

  std::println("Shutdown Memory footprint:");
  std::println("  Used {} bytes", init_arena.used_memory());
  std::println("  Available {} bytes", init_arena.available_memory());
  std::println("  Total {} bytes", init_arena.total_memory());
  core.device.waitIdle();
  return 0;
}
