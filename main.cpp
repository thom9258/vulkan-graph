#include "core.hpp"
#include "drawing.hpp"
#include "geometry_primitives.hpp"
#include "ensure.hpp"
#include "graph.hpp"
#include "memory_buffer.hpp"
#include "presentation_context.hpp"
#include "texture_storage.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <SDL_video.h>
#include <chrono>
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

  alex::presentation_context_info_t presentation_context_info;
  presentation_context_info.core = &core;
  presentation_context_info.surface = surface;
  presentation_context_info.enable_vsync = true;
  int width{0};
  int height{0};
  SDL_GetWindowSize(window, &width, &height);
  presentation_context_info.window_extent.width = width;
  presentation_context_info.window_extent.height = height;
  alex::presentation_context_t presentation_context;
  presentation_context.init(presentation_context_info, init_arena);

  vk::Extent3D render_extent(presentation_context.window_extent.width,
							 presentation_context.window_extent.height, 1);

  auto geometry_color_attachment = alex::graph::resource_info_t{
      .name = "geom-color",
      .type = alex::graph::resource_type_t::attachment,
      .attachment = alex::graph::attachment_resource_t{
          .type = alex::graph::attachment_type_t::color,
          .index = 0,
          .format = vk::Format::eR8G8B8A8Unorm,
          .extent = render_extent,
          .aspect_flags = vk::ImageAspectFlagBits::eColor}};

  auto geometry_depth_attachment = alex::graph::resource_info_t{
      .name = "geom-depth",
      .type = alex::graph::resource_type_t::attachment,
      .attachment = alex::graph::attachment_resource_t{
          .type = alex::graph::attachment_type_t::depth,
          .index = 1,
          .format = vk::Format::eD32Sfloat,
          .extent = render_extent,
          .aspect_flags = vk::ImageAspectFlagBits::eDepth}};

  auto all_resources = std::to_array(
      {&geometry_depth_attachment, &geometry_color_attachment});

  auto geometry_pass_inputs = std::to_array({"geom-color"sv, "geom-depth"sv});

  alex::graph::framepass_info_t geometry_pass;
  geometry_pass.name = "geometry-pass";
  geometry_pass.inputs = geometry_pass_inputs;
  geometry_pass.outputs = {};
  geometry_pass.extent = render_extent;
  float constexpr clearcolor = static_cast<float>(0x20) / 255;
  geometry_pass.clearvalues = {
      vk::ClearValue{}.setColor({clearcolor, clearcolor, clearcolor, 1.0f}),
      vk::ClearValue{}.setDepthStencil({1.0f, 0}),
  };

  geometry_pass.load_op = vk::AttachmentLoadOp::eClear;
  geometry_pass.vertex_program_path = "./geometry.vert.spv";
  geometry_pass.fragment_program_path = "./geometry.frag.spv";

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

  std::array<vk::DescriptorSetLayout, 1> setlayouts;
  setlayouts[0] =
      core.device.createDescriptorSetLayout(uniform_setinfo, nullptr);

  geometry_pass.set_layouts = setlayouts;

  auto all_framepasses = std::to_array({&geometry_pass});

  alex::texture_storage_info_t texture_storage_info;
  texture_storage_info.capacity = 25;

  alex::texture_storage_t texture_storage;
  texture_storage.init(texture_storage_info, init_arena);

  alex::graph::graph_info_t graph_info;
  graph_info.physical_device = core.physical_device;
  graph_info.device = core.device;
  graph_info.framepass_infos = all_framepasses;
  graph_info.resource_infos = all_resources;
  graph_info.arena = &init_arena;
  graph_info.texture_storage = &texture_storage;

  alex::graph::graph_t graph;
  graph.init(graph_info);
  graph.debug_print();
  graph.debug_graphviz();

  ENSURE_NOT(texture_storage.find("geom-color").empty(), "could not find image in storage")
  ENSURE_NOT(texture_storage.find("geom-depth").empty(), "could not find image in storage")

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
   * Vertex Buffer Setup
   */

  // TODO: funnily enough i think we can set up a direct memory buffer
  // to be the backend of an arena allocator, that way we can just make a
  // big direct buffer and make temporary allocations that are then freed
  // once the init_commandbuffer is finished.
  std::span<alex::vertex_t> cube_vertices = load_cube(init_arena);
  alex::direct_memory_buffer_info_t direct_cube_buffer_info;
  direct_cube_buffer_info.physical_device = core.physical_device;
  direct_cube_buffer_info.device = core.device;
  direct_cube_buffer_info.buffer_type = alex::BufferType::Basic;
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
  cube_buffer_info.buffer_type = alex::BufferType::Vertices;
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

  alex::draw_info_t cube_draw_info;
  cube_draw_info.vertices_count = cube_vertices.size();
  cube_draw_info.instance_count = 1;
  cube_draw_info.first_vertex = 0;
  cube_draw_info.first_instance = 0;
  LOG_INFO("Created cube vertex buffer");

  /* ****************************************
   * Uniform Buffers Setup
   */
  alex::direct_memory_buffer_info_t direct_uniform_info;
  direct_uniform_info.physical_device = core.physical_device;
  direct_uniform_info.device = core.device;
  direct_uniform_info.buffer_type = alex::BufferType::Basic;
  direct_uniform_info.memory_size = 128;

  alex::flightframe_array_t<alex::direct_memory_buffer_t> direct_uniforms;
  for (alex::direct_memory_buffer_t &uniform : direct_uniforms) {
    uniform.init(direct_uniform_info);
  };
  LOG_INFO("Created direct memory uniforms");

  alex::flightframe_array_t<alex::memory_buffer_t> uniforms;
  for (auto [i, uniform] : uniforms | std::views::enumerate) {
    alex::memory_buffer_info_t uniform_info;
    uniform_info.physical_device = core.physical_device;
    uniform_info.device = core.device;
    uniform_info.buffer_type = alex::BufferType::Uniform;
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

    graph.record(next_frame_info);

    alex::presentation_info_t presentation_info;
    presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.source_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(geometry_color_attachment.attachment.extent.width),
        static_cast<std::int32_t>(geometry_color_attachment.attachment.extent.height), 1};

    presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.destination_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(presentation_context.window_extent.width),
        static_cast<std::int32_t>(presentation_context.window_extent.height),
        1};

    presentation_info.blit_filter = vk::Filter::eLinear;

    std::span<alex::texture_t> final_images =
        texture_storage.find("geom-color");

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
