#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture_storage.hpp>

#include <alex/task_graph.hpp>

#include "alex/flightframe_array.hpp"

#include "../utility/button.hpp"
#include "../utility/scenestack.hpp"
#include "../utility/sdl.hpp"

#include "bitmap.hpp"
#include "ecs_chest_scene.hpp"
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
  window_info.width = 320 * 2;
  window_info.height = 240 * 2;

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
  vk::Extent3D render_extent(
      static_cast<std::int32_t>(window_extent.width / 2),
      static_cast<std::int32_t>(window_extent.height / 2), 1);

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

  game::rendering_t rendering(core, render_extent);

  /* ****************************************
   * Create Scenes
   */
  scene::scenestack_t scenestack;

  game::ecs_chest_scene_info_t chest_scene_info;
  chest_scene_info.name = "Chest scene 1";
  chest_scene_info.core = &core;
  chest_scene_info.rendering = &rendering;
  chest_scene_info.presenter = &presenter;
  chest_scene_info.window = &window;
  scenestack.put(std::make_unique<game::ecs_chest_scene>(chest_scene_info));

#if 0
  /* ****************************************
   * Create DescriptorSet Layout for geometry pipeline
   */
  std::vector<vk::DescriptorSetLayoutBinding> frame_uniform_bindings({
      vk::DescriptorSetLayoutBinding{}
          .setStageFlags(vk::ShaderStageFlagBits::eVertex)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setBinding(0)
          .setDescriptorCount(1),
  });

  const auto frame_uniform_setinfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setFlags(vk::DescriptorSetLayoutCreateFlags())
          .setBindings(frame_uniform_bindings);

  vk::UniqueDescriptorSetLayout geometry_pipeline_frame_uniform_setlayout =
      core.device().createDescriptorSetLayoutUnique(frame_uniform_setinfo,
                                                    nullptr);

  std::vector<vk::DescriptorSetLayoutBinding> diffuse_bindings(
      {vk::DescriptorSetLayoutBinding{}
           .setStageFlags(vk::ShaderStageFlagBits::eFragment)
           .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
           .setBinding(0)

           .setDescriptorCount(1)});
  const auto diffuse_setinfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setFlags(vk::DescriptorSetLayoutCreateFlags())
          .setBindings(diffuse_bindings);

  vk::UniqueDescriptorSetLayout geometry_pipeline_diffuse_setlayout =
      core.device().createDescriptorSetLayoutUnique(diffuse_setinfo, nullptr);

  /* ****************************************
   * Setup rendering
   */

  alex::texture_info_t colorattachment_info;
  colorattachment_info.physical_device = core.physical_device();
  colorattachment_info.device = core.device();
  colorattachment_info.extent.setWidth(render_extent.width)
      .setHeight(render_extent.height);

  colorattachment_info.format = vk::Format::eR8G8B8A8Srgb;
  colorattachment_info.tiling = vk::ImageTiling::eOptimal;
  colorattachment_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
  colorattachment_info.property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  colorattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                               vk::ImageUsageFlagBits::eTransferSrc |
                               vk::ImageUsageFlagBits::eSampled |
                               vk::ImageUsageFlagBits::eColorAttachment;

  std::vector<alex::texture_t> colorattachments;
  for (auto _ :
       std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
    colorattachments.emplace_back(colorattachment_info);
  }

  alex::texture_info_t depthattachment_info;
  depthattachment_info.physical_device = core.physical_device();
  depthattachment_info.device = core.device();
  depthattachment_info.extent.setWidth(render_extent.width)
      .setHeight(render_extent.height);

  depthattachment_info.format = vk::Format::eD32Sfloat;
  depthattachment_info.tiling = vk::ImageTiling::eOptimal;
  depthattachment_info.aspect_flags = vk::ImageAspectFlagBits::eDepth;
  depthattachment_info.property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;

  depthattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                               vk::ImageUsageFlagBits::eTransferSrc |
                               vk::ImageUsageFlagBits::eSampled |
                               vk::ImageUsageFlagBits::eDepthStencilAttachment;

  std::vector<alex::texture_t> depthattachments;
  for (auto _ :
       std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
    depthattachments.emplace_back(depthattachment_info);
  }

  alex::flightframe_array_t<vk::ImageView> colorattachment_views;
  for (auto [i, view] : colorattachment_views | std::views::enumerate) {
    view = colorattachments[i].view();
  }

  alex::flightframe_array_t<vk::ImageView> depthattachment_views;
  for (auto [i, view] : depthattachment_views | std::views::enumerate) {
    view = depthattachments[i].view();
  }

  auto geometrypass_info = alex::geometrypass_info_t(core.device())
                               .set_color_attachments(colorattachment_views)
                               .set_color_format(vk::Format::eR8G8B8A8Srgb)
                               .set_color_clearvalue(0.0f, 0.0f, 0.0f, 1.0f)
                               .set_depth_format(vk::Format::eD32Sfloat)
                               .set_depth_attachments(depthattachment_views)
                               .set_depth_clearvalue(1.0f)
                               .set_extent(render_extent)
                               .set_loadop(vk::AttachmentLoadOp::eClear);

  alex::geometrypass_t geometry_pass(geometrypass_info);

  auto geometry_pipeline_info =
      alex::pipeline_info_t(core.device())
          .set_extent(render_extent)
          .set_polygon_mode(vk::PolygonMode::eFill)
          .set_cull_mode(vk::CullModeFlagBits::eBack)
          .set_front_face(vk::FrontFace::eCounterClockwise)
          .set_renderpass(geometry_pass.renderpass())
          .set_vertex_program_path("./geometry.vert.spv")
          .set_fragment_program_path("./geometry.frag.spv")
          .add_setlayout(geometry_pipeline_frame_uniform_setlayout.get())
          .add_setlayout(geometry_pipeline_diffuse_setlayout.get())
          .add_vertex_input_binding(
              vk::VertexInputBindingDescription{}
                  .setBinding(0)
                  .setStride(sizeof(game::simple_vertex_t))
                  .setInputRate(vk::VertexInputRate::eVertex))
          .add_vertex_input_attribute(
              vk::VertexInputAttributeDescription{}
                  .setBinding(0)
                  .setLocation(0)
                  .setFormat(vk::Format::eR32G32B32Sfloat)
                  .setOffset(offsetof(game::simple_vertex_t, position)))
          .add_vertex_input_attribute(
              vk::VertexInputAttributeDescription{}
                  .setBinding(0)
                  .setLocation(1)
                  .setFormat(vk::Format::eR32G32B32Sfloat)
                  .setOffset(offsetof(game::simple_vertex_t, normal)))
          .add_vertex_input_attribute(
              vk::VertexInputAttributeDescription{}
                  .setBinding(0)
                  .setLocation(2)
                  .setFormat(vk::Format::eR32G32B32Sfloat)
                  .setOffset(offsetof(game::simple_vertex_t, color)))
          .add_vertex_input_attribute(
              vk::VertexInputAttributeDescription{}
                  .setBinding(0)
                  .setLocation(3)
                  .setFormat(vk::Format::eR32G32Sfloat)
                  .setOffset(offsetof(game::simple_vertex_t, texcoord)));

  alex::pipeline_t geometry_pipeline(geometry_pipeline_info, init_arena);
  alex::flightframe_array_t<vk::UniqueSemaphore> taskgraph_semaphores;
  for (vk::UniqueSemaphore &semaphore : taskgraph_semaphores) {
    semaphore = core.create_semaphore();
  }

  alex::flightframe_array_t<alex::graph_t> taskgraphs;
#endif

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

#if 0  
    window.mark_next_frame();
    std::span<SDL_Event> events = window.get_events();
    double const deltatime = window.deltatime_seconds();

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
        break;
      }
    }
	
	ecs::system::orbit_camera_update_info_t orbit_camera_update_info;
    orbit_camera_update_info.manager = &manager;
    orbit_camera_update_info.sdl_events = events;
    orbit_camera_update_info.deltatime = deltatime;
    ecs::system::orbit_camera_update(orbit_camera_update_info);

    alex::next_frame_info_t next_frame_info =
        presenter.wait_for_next_frame(core.device());

    auto upload_task_id = alex::task_id_t(0);
    auto geometry_task_id = alex::task_id_t(1);

    taskgraphs[next_frame_info.flightframe] = alex::graph_t();
    alex::graph_t &graph = taskgraphs[next_frame_info.flightframe];

    graph.add_task(upload_task_id,
                   std::make_unique<alex::simple_task_t>(
                       "upload", [&](vk::CommandBuffer commandbuffer) {
                         ecs::system::models_upload_info_t system_upload_info;
                         system_upload_info.manager = &manager;
                         system_upload_info.physical_device =
                             core.physical_device();
                         system_upload_info.device = core.device();
                         system_upload_info.commandbuffer = commandbuffer;
                         system_upload_info.flightframe =
                             next_frame_info.flightframe;

                         ecs::system::models_upload(system_upload_info);
                       }));

    graph.add_task(
        geometry_task_id,
        std::make_unique<alex::simple_task_t>(
            "geometry", [&](vk::CommandBuffer commandbuffer) {
              const auto render_area =
                  vk::Rect2D{}
                      .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                      .setExtent(vk::Extent2D(render_extent.width,
                                              render_extent.height));

              auto clearvalues = geometry_pass.clearvalues();

              const auto renderpass_begin_info =
                  vk::RenderPassBeginInfo{}
                      .setRenderPass(geometry_pass.renderpass())
                      .setFramebuffer(geometry_pass.framebuffer(
                          next_frame_info.flightframe))
                      .setRenderArea(render_area)
                      .setClearValues(clearvalues);

              commandbuffer.beginRenderPass(renderpass_begin_info,
                                            vk::SubpassContents::eInline);

              auto viewport =
                  vk::Viewport{}
                      .setX(0)
                      .setY(0)
                      .setWidth(static_cast<float>(render_extent.width))
                      .setHeight(static_cast<float>(render_extent.height))
                      .setMinDepth(0.0f)
                      .setMaxDepth(1.0f);

              auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
                  {render_extent.width, render_extent.height});

              commandbuffer.setViewport(0, viewport);
              commandbuffer.setScissor(0, scissor);
              commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                         geometry_pipeline.pipeline());

              ecs::system::models_draw_info_t system_draw_info;
              system_draw_info.manager = &manager;
              system_draw_info.commandbuffer = commandbuffer;
              system_draw_info.flightframe = next_frame_info.flightframe;
              system_draw_info.geometry_pipeline_layout =
                  geometry_pipeline.layout();
              ecs::system::models_draw(system_draw_info);

              commandbuffer.endRenderPass();
            }));

    graph.add_dependency(alex::dependency_info_t{.device = core.device(),
                                                 .parent = upload_task_id,
                                                 .child = geometry_task_id});

    graph.set_end(geometry_task_id);

    vk::Semaphore graph_finished_semaphore =
        taskgraph_semaphores[next_frame_info.flightframe].get();

    auto graph_evaluate_info = alex::graph_evaluate_info_t{};
    graph_evaluate_info.device = core.device();
    graph_evaluate_info.commandpool = core.commandpool();
    graph_evaluate_info.queue = core.queue();
    graph_evaluate_info.sync_semaphore = graph_finished_semaphore;
    graph.evaluate(graph_evaluate_info);

    alex::presentation_info_t presentation_info;
    presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.source_offset_end =
        vk::Offset3D{static_cast<std::int32_t>(render_extent.width),
                     static_cast<std::int32_t>(render_extent.height), 1};

    presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.destination_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(presenter.window_extent.width),
        static_cast<std::int32_t>(presenter.window_extent.height), 1};

    presentation_info.blit_filter = vk::Filter::eNearest;
    presentation_info.image =
        colorattachments[next_frame_info.flightframe].image();
    presentation_info.layout = vk::ImageLayout::eColorAttachmentOptimal;
    presentation_info.queue = core.queue();
    presentation_info.wait_semaphore = graph_finished_semaphore;
    presenter.present(presentation_info);
  }
#endif

  core.device().waitIdle();
  return 0;
}
