#include "core.hpp"
#include "geometry_pipeline.hpp"
#include "memory_buffer.hpp"
#include "presentation_context.hpp"
#include "renderpass.hpp"
#include "texture_storage.hpp"

#include "geometry_primitives.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <SDL_video.h>
#include <chrono>
#include <ranges>
#include <span>
#include <string_view>
#include <thread>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

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

  alex::texture_storage_info_t texture_storage_info;
  texture_storage_info.capacity = 128;
  alex::texture_storage_t texture_storage;
  texture_storage.init(texture_storage_info, init_arena);

  alex::presentation_context_info_t presentation_context_info;
  presentation_context_info.core = &core;
  presentation_context_info.surface = surface;
  int width{0};
  int height{0};
  SDL_GetWindowSize(window, &width, &height);
  presentation_context_info.window_extent.width = width;
  presentation_context_info.window_extent.height = height;
  alex::presentation_context_t presentation_context;
  presentation_context.init(presentation_context_info, init_arena);

  alex::renderpass_info_t renderpass_info;
  renderpass_info.core = &core;
  renderpass_info.extent = presentation_context_info.window_extent;
  // flightframe_array_t<std::span<vk::ImageView>> attachments;

  alex::texture_info_t color_attachment_info;
  color_attachment_info.physical_device = core.physical_device;
  color_attachment_info.device = core.device;
  color_attachment_info.format = vk::Format::eR8G8B8A8Srgb;
  color_attachment_info.tiling = vk::ImageTiling::eOptimal;
  color_attachment_info.extent = presentation_context_info.window_extent;
  color_attachment_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
  color_attachment_info.property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  color_attachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                                vk::ImageUsageFlagBits::eTransferSrc |
                                vk::ImageUsageFlagBits::eSampled |
                                vk::ImageUsageFlagBits::eColorAttachment;

  alex::flightframe_array_t<alex::texture_t> color_attachments;
  color_attachments[0].init(color_attachment_info);
  color_attachments[1].init(color_attachment_info);

  alex::texture_info_t depth_attachment_info;
  depth_attachment_info.physical_device = core.physical_device;
  depth_attachment_info.device = core.device;
  depth_attachment_info.format = vk::Format::eD32Sfloat;
  depth_attachment_info.tiling = vk::ImageTiling::eOptimal;
  depth_attachment_info.extent = presentation_context_info.window_extent;
  depth_attachment_info.aspect_flags = vk::ImageAspectFlagBits::eDepth;
  depth_attachment_info.property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  depth_attachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                                vk::ImageUsageFlagBits::eTransferSrc |
                                vk::ImageUsageFlagBits::eSampled |
                                vk::ImageUsageFlagBits::eDepthStencilAttachment;

  alex::flightframe_array_t<alex::texture_t> depth_attachments;
  depth_attachments[0].init(depth_attachment_info);
  depth_attachments[1].init(depth_attachment_info);

  auto attachment0 = init_arena.allocate<vk::ImageView>(2);
  ENSURE_NOT(attachment0.empty(), "out of memory")
  attachment0[0] = color_attachments[0].view;
  attachment0[1] = depth_attachments[0].view;
  renderpass_info.attachments[0] = attachment0;

  auto attachment1 = init_arena.allocate<vk::ImageView>(2);
  ENSURE_NOT(attachment1.empty(), "out of memory")
  attachment1[0] = color_attachments[1].view;
  attachment1[1] = depth_attachments[1].view;
  renderpass_info.attachments[1] = attachment1;

  alex::renderpass_t geometry_renderpass;
  geometry_renderpass.init(renderpass_info);

  alex::geometry_pipeline_info_t geometry_pipeline_info;
  geometry_pipeline_info.core = &core;
  geometry_pipeline_info.renderpass = &geometry_renderpass;
  geometry_pipeline_info.extent = presentation_context_info.window_extent;
  geometry_pipeline_info.vertex_program_path = "./geometry.vert.spv";
  geometry_pipeline_info.fragment_program_path = "./geometry.frag.spv";

  alex::geometry_pipeline_t geometry_pipeline;
  geometry_pipeline.init(geometry_pipeline_info, init_arena);

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

  /* ****************************************
   * DescriptorSets for Uniforms Setup
   */
  std::array<vk::DescriptorPoolSize, 2> sizes{
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(10),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageBuffer)
          .setDescriptorCount(10),
  };

  const auto pool_info =
      vk::DescriptorPoolCreateInfo{}
          .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
          .setMaxSets(10)
          .setPoolSizes(sizes);

  vk::DescriptorPool descriptor_pool =
      core.device.createDescriptorPool(pool_info, nullptr);

  std::array<vk::DescriptorSetLayout, 2> set_layouts{
      geometry_pipeline.setlayout, geometry_pipeline.setlayout};

  const auto descriptorset_allocate_info =
      vk::DescriptorSetAllocateInfo{}
          .setDescriptorPool(descriptor_pool)
          .setDescriptorSetCount(2)
          .setSetLayouts(set_layouts);

  std::vector<vk::DescriptorSet> uniform_sets =
      core.device.allocateDescriptorSets(descriptorset_allocate_info);
  ENSURE(uniform_sets.size() == 2, "could not allocate descriptor sets")

  for (auto [i, uniform_set] : uniform_sets | std::views::enumerate) {
    const auto descriptor_buffer_info = vk::DescriptorBufferInfo{}
                                            .setBuffer(uniforms[i].buffer)
                                            .setOffset(0)
                                            .setRange(uniforms[i].memory_size);

    const auto write_descriptor =
        vk::WriteDescriptorSet{}
            .setDstBinding(0)
            .setDstSet(uniform_sets[i])
            .setDstArrayElement(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            // here images can be set aswell
            .setBufferInfo(descriptor_buffer_info);

    const uint32_t write_count = 1;
    const uint32_t copy_count = 0;
    core.device.updateDescriptorSets(write_count, &write_descriptor, copy_count,
                                     nullptr);
  }

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
    next_frame_info.presentation_commandbuffer.begin(
        vk::CommandBufferBeginInfo{});

    const auto render_area =
        vk::Rect2D{}
            .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
            .setExtent(geometry_pipeline_info.extent);

    float constexpr clearcolor = static_cast<float>(0x20) / 255;
    std::array<vk::ClearValue, 2> clearvalues{
        vk::ClearValue{}.setColor({clearcolor, clearcolor, clearcolor, 1.0f}),
        vk::ClearValue{}.setDepthStencil({1.0f, 0}),
    };

    const auto renderpass_begin_info =
        vk::RenderPassBeginInfo{}
            .setRenderPass(geometry_renderpass.renderpass)
            .setFramebuffer(
                geometry_renderpass.framebuffers[next_frame_info.flightframe])
            .setRenderArea(render_area)
            .setClearValues(clearvalues);

    next_frame_info.presentation_commandbuffer.beginRenderPass(
        renderpass_begin_info, vk::SubpassContents::eInline);
    next_frame_info.presentation_commandbuffer.bindPipeline(
        vk::PipelineBindPoint::eGraphics, geometry_pipeline.pipeline);

    next_frame_info.presentation_commandbuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, geometry_pipeline.layout, 0, 1,
        &(uniform_sets.at(next_frame_info.flightframe)), 0, nullptr);

    uint32_t constexpr first_binding{0};
    std::array<vk::Buffer, 1> const buffers{cube_buffer.buffer};
    std::array<vk::DeviceSize, 1> constexpr offsets{0};
    next_frame_info.presentation_commandbuffer.bindVertexBuffers(
        first_binding, buffers, offsets);

    next_frame_info.presentation_commandbuffer.draw(
        cube_draw_info.vertices_count, cube_draw_info.instance_count,
        cube_draw_info.first_vertex, cube_draw_info.first_instance);

    next_frame_info.presentation_commandbuffer.endRenderPass();

    // Here we transfer the color attachment of the renderpass into
    // transfersrc so we can blit it to the swapchain
    auto range = vk::ImageSubresourceRange{}
                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                     .setBaseMipLevel(0)
                     .setLevelCount(1)
                     .setBaseArrayLayer(0)
                     .setLayerCount(1);

    auto barrier =
        vk::ImageMemoryBarrier{}
            .setImage(color_attachments[next_frame_info.flightframe].image)
            .setSubresourceRange(range)
            .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
            .setDstAccessMask(vk::AccessFlags())
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    next_frame_info.presentation_commandbuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr,
        nullptr, barrier);

    alex::presentation_info_t presentation_info;
    presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.source_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(renderpass_info.extent.width),
        static_cast<std::int32_t>(renderpass_info.extent.height), 1};

    presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.destination_offset_end =
        vk::Offset3D{static_cast<std::int32_t>(
                         presentation_context_info.window_extent.width),
                     static_cast<std::int32_t>(
                         presentation_context_info.window_extent.height),
                     1};

    presentation_info.blit_filter = vk::Filter::eLinear;
    presentation_info.image =
        color_attachments[next_frame_info.flightframe].image;
    presentation_info.queue = core.queue;
    presentation_info.commandbuffer =
        next_frame_info.presentation_commandbuffer;

    presentation_context.present(presentation_info);
  }

  {
    std::println("Shutdown Memory footprint:");
    std::println("  Used {} bytes", init_arena.used_memory());
    std::println("  Available {} bytes", init_arena.available_memory());
    std::println("  Total {} bytes", init_arena.total_memory());
  }

  return 0;
}
