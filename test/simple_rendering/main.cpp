#include <alex/core.hpp>
#include <alex/drawing.hpp>
#include <alex/ensure.hpp>
// #include <alex/graph.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture_storage.hpp>

#include <alex/task_graph.hpp>

#include "alex/flightframe_array.hpp"
#include "ecs.hpp"

#include "../utility/deltaclock.hpp"
#include "button.hpp"
#include "cube_prefab.hpp"
#include "draw_info_uniform.hpp"
#include "glm.hpp"
#include "orbit_camera.hpp"

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

  std::vector<const char *> window_extensions(window_extensions_count);
  SDL_Vulkan_GetInstanceExtensions(window, &window_extensions_count,
                                   window_extensions.data());

  alex::context_info_t context_info;
  context_info.instance_extensions = window_extensions;
  context_info.enable_validation = true;

  alex::context_t context(context_info);

  vk::UniqueSurfaceKHR surface;
  {
    VkSurfaceKHR tmp;
    SDL_Vulkan_CreateSurface(window, context.instance(), &tmp);
    surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(tmp), context.instance());
  }

  alex::core_info_t core_info;
  core_info.surface = surface.get();
  core_info.instance = context.instance();
  vk::Extent3D render_extent(static_cast<std::int32_t>(window_info.width / 4),
                             static_cast<std::int32_t>(window_info.height / 4),
                             1);

  alex::core_t core(core_info);

#if 0  
  std::vector<vk::DescriptorPoolSize> pool_sizes{
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eSampler)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eCombinedImageSampler)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eSampledImage)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageImage)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformTexelBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageTexelBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformBufferDynamic)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageBufferDynamic)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eInputAttachment)
          .setDescriptorCount(1000),
  };

  auto const pool_info =
      vk::DescriptorPoolCreateInfo{}
          .setPoolSizes(pool_sizes)
          .setMaxSets(1000)
          .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

  vk::UniqueDescriptorPool descriptor_pool =
      core.create_descriptorpool(pool_info);
#endif

  /* ****************************************
   * Create DescriptorSet Layout for geometry pipeline
   */
  int width{0};
  int height{0};
  SDL_GetWindowSize(window, &width, &height);

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

  vk::UniqueDescriptorSetLayout geometry_pipeline_info_setlayout =
      core.device().createDescriptorSetLayoutUnique(uniform_setinfo, nullptr);

  /* ****************************************
   * Setup our meshes
   */

  std::array<ecs::manager_t::entity_t, max_entities> entity_memory;
  std::array<component_mesh_t, max_entities> mesh_components;
  std::array<component_transform_t, max_entities> transform_components;
  ecs::manager_t manager(entity_memory, mesh_components, transform_components);

  cube_prefab_info_t cube_prefab_info;
  cube_prefab_info.manager = &manager;
  cube_prefab_info.core = &core;
  cube_prefab_info.set_layout = geometry_pipeline_info_setlayout.get();
  cube_prefab_info.arena = &init_arena;
  cube_prefab_info.r = 1.0f;
  cube_prefab_info.g = 0.0f;
  cube_prefab_info.b = 0.0f;
  cube_prefab_info.transform =
      glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

  ecs::entity_id_t x_dir = add_cube_prefab(cube_prefab_info);

  cube_prefab_info.r = 0.0f;
  cube_prefab_info.g = 1.0f;
  cube_prefab_info.b = 0.0f;
  cube_prefab_info.transform =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

  ecs::entity_id_t y_dir = add_cube_prefab(cube_prefab_info);

  cube_prefab_info.r = 0.0f;
  cube_prefab_info.g = 0.0f;
  cube_prefab_info.b = 1.0f;
  cube_prefab_info.transform =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

  ecs::entity_id_t z_dir = add_cube_prefab(cube_prefab_info);

  cube_prefab_info.r = 1.0f;
  cube_prefab_info.g = 1.0f;
  cube_prefab_info.b = 1.0f;
  cube_prefab_info.transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

  ecs::entity_id_t center = add_cube_prefab(cube_prefab_info);

  /* ****************************************
   * Setup presentation context
   */
  alex::presenter_info_t presenter_info;
  presenter_info.physical_device = core.physical_device();
  presenter_info.device = core.device();
  presenter_info.commandpool = core.commandpool();
  presenter_info.surface = surface.get();
  presenter_info.enable_vsync = false;
  presenter_info.window_extent.width = width;
  presenter_info.window_extent.height = height;
  alex::presenter_t presenter(presenter_info);

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

  auto geometrypass_info = alex::graph::geometrypass_info_t(core.device())
                               .set_color_attachments(colorattachment_views)
                               .set_color_format(vk::Format::eR8G8B8A8Srgb)
                               .set_color_clearvalue(0.5f, 0.5f, 0.5f, 1.0f)
                               .set_depth_format(vk::Format::eD32Sfloat)
                               .set_depth_attachments(depthattachment_views)
                               .set_depth_clearvalue(1.0f)
                               .set_extent(render_extent)
                               .set_loadop(vk::AttachmentLoadOp::eClear);

  alex::graph::geometrypass_t geometry_pass(geometrypass_info);

  auto geometry_pipeline_info =
      alex::graph::pipeline_info_t(core.device())
          .set_extent(render_extent)
          .set_renderpass(geometry_pass.renderpass())
          .set_vertex_program_path("./geometry.vert.spv")
          .set_fragment_program_path("./geometry.frag.spv")
          .add_setlayout(geometry_pipeline_info_setlayout.get());

  alex::graph::pipeline_t geometry_pipeline(geometry_pipeline_info, init_arena);

#if 0
  /* ****************************************
   * Setup render graph
   */
  auto graph_info = alex::graph::graph_info_t(
      core.physical_device(), core.device(), core.commandpool());

  graph_info.add_attachment("geom-color", alex::graph::attachment_type_t::color)
      .set_format(vk::Format::eR8G8B8A8Srgb)
      .set_extent(render_extent)
      .set_aspect_flags(vk::ImageAspectFlagBits::eColor);

  graph_info.add_attachment("geom-depth", alex::graph::attachment_type_t::depth)
      .set_format(vk::Format::eD32Sfloat)
      .set_extent(render_extent)
      .set_aspect_flags(vk::ImageAspectFlagBits::eDepth);

  graph_info.add_framepass("geometry-pass")
      .add_dependency("upload")
      .set_color_attachment("geom-color")
      .set_depth_attachment("geom-depth")
      .set_extent(render_extent)
      .set_vertex_program_path("./geometry.vert.spv")
      .set_fragment_program_path("./geometry.frag.spv");

  graph_info.add_uploadpass("upload");

  alex::graph::graph_t graph(graph_info);
  std::println("======================");
  graph.print_execution_order(std::cout);
  std::println("======================");
  graph.print_graphviz(std::cout);
  std::println("======================");
  
  /* ****************************************
   * Pipeline Setup using the created graph renderpasses
   */

  alex::graph::node_t *geometry_renderpass_node =
      graph.find_node("geometry-pass");
  ENSURE(geometry_renderpass_node != nullptr, "could not find geometry-pass")

  alex::graph::renderpass_node_t *geometry_renderpass =
      std::get_if<alex::graph::renderpass_node_t>(geometry_renderpass_node);

  ENSURE(geometry_renderpass != nullptr, "could not find geometry-pass")

  auto geometry_pipeline_info =
      alex::graph::pipeline_info_t(core.device())
          .set_extent(render_extent)
          .set_renderpass(geometry_renderpass->geometry_pass.renderpass)
          .set_vertex_program_path("./geometry.vert.spv")
          .set_fragment_program_path("./geometry.frag.spv")
          .add_setlayout(geometry_pipeline_info_setlayout);

  alex::graph::pipeline_t geometry_pipeline(geometry_pipeline_info, init_arena);

#endif

  alex::flightframe_array_t<vk::UniqueSemaphore> taskgraph_semaphores;
  for (vk::UniqueSemaphore &semaphore : taskgraph_semaphores) {
    semaphore = core.create_semaphore();
  }

  DeltaClock deltaclock;

  float const camera_radius = 4.0f;
  glm::vec3 const target(0.0, 0.0, 0);
  OrbitCamera camera(target, camera_radius);

  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const float near_plane = 1.0f, far_plane = 20.0f;
  glm::mat4 const projection = std::invoke([&]() {
    glm::mat4 p =
        glm::perspective(glm::radians(70.f), aspect, near_plane, far_plane);
    p[1][1] *= -1.0f;
    return p;
  });

  {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto time_ns = end_time - start_time;
    auto time =
        std::chrono::duration_cast<std::chrono::duration<double>>(time_ns);
    std::println("Initialization Time: {}", time);
  }

  struct buttons_t {
    button_t w;
    button_t a;
    button_t s;
    button_t d;
    button_t e;
    button_t q;
  } buttons;

  bool running = true;
  while (running) {
    auto const deltatime = deltaclock.deltatime_ms();
    float const movespeed = 5.0f * deltatime;
    std::vector<SDL_Event> events = poll_all_sdl_events();
    for (SDL_Event event : events) {
      switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;

      case SDL_KEYUP:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          running = false;
          break;
        case SDLK_w:
          buttons.w.release();
          break;
        case SDLK_s:
          buttons.s.release();
          break;
        case SDLK_a:
          buttons.a.release();
          break;
        case SDLK_d:
          buttons.d.release();
          break;
        case SDLK_e:
          buttons.e.release();
          break;
        case SDLK_q:
          buttons.q.release();
          break;
        }
        break;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          running = false;
          break;
        case SDLK_w:
          buttons.w.press();
          break;
        case SDLK_s:
          buttons.s.press();
          break;
        case SDLK_a:
          buttons.a.press();
          break;
        case SDLK_d:
          buttons.d.press();
          break;
        case SDLK_e:
          buttons.e.press();
          break;
        case SDLK_q:
          buttons.q.press();
          break;
        }
        break;
      }
    }

    if (buttons.w.is_pressed()) {
      camera.add_rotation(movespeed, 0.0f);
    }
    if (buttons.a.is_pressed()) {
      camera.add_rotation(0.0f, movespeed);
    }
    if (buttons.s.is_pressed()) {
      camera.add_rotation(-movespeed, 0.0f);
    }
    if (buttons.d.is_pressed()) {
      camera.add_rotation(0.0f, -movespeed);
    }
    if (buttons.e.is_pressed()) {
      camera.set_radius(camera.radius() + movespeed);
    }
    if (buttons.q.is_pressed()) {
      camera.set_radius(camera.radius() - movespeed);
    }

    alex::next_frame_info_t next_frame_info =
        presenter.wait_for_next_frame(core.device());

    auto upload_task_id = alex2::task_id_t(0);
    auto geometry_task_id = alex2::task_id_t(1);
    auto present_task_id = alex2::task_id_t(2);

    auto graph = alex2::graph_t();

    graph.add_task(
        upload_task_id,
        std::make_unique<alex2::simple_task_t>(
            "upload", [&](vk::CommandBuffer commandbuffer) {
              std::array<ecs::entity_id_t, max_entities> entities;
              std::size_t entity_count = manager.get_entities(entities);
              entity_count = manager.filter_inplace<component_mesh_t>(
                  std::span(entities).subspan(0, entity_count));
              entity_count = manager.filter_inplace<component_transform_t>(
                  std::span(entities).subspan(0, entity_count));

              for (ecs::entity_id_t entity :
                   entities | std::views::take(entity_count)) {
                auto *mesh = manager.get_component<component_mesh_t>(entity);
                auto *transform =
                    manager.get_component<component_transform_t>(entity);

                draw_info_t draw_info;
                draw_info.view = camera.view();
                draw_info.projection = projection;
                draw_info.model = transform->mat;
                std::memcpy(mesh->direct_uniforms[next_frame_info.flightframe]
                                ->memory_ptr(),
                            &draw_info, sizeof(draw_info));

                alex::memory_buffer_write_info_t write_info;
                write_info.physical_device = core.physical_device();
                write_info.device = core.device();
                write_info.direct =
                    &mesh->direct_uniforms[next_frame_info.flightframe].value();
                write_info.write_size =
                    mesh->uniforms[next_frame_info.flightframe]->memory_size();
                write_info.commandbuffer = commandbuffer;
                mesh->uniforms[next_frame_info.flightframe]->record_write(
                    write_info);
              }

              std::println("Recorded upload task");
            }));

    graph.add_task(
        geometry_task_id,
        std::make_unique<alex2::simple_task_t>(
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

              std::array<ecs::entity_id_t, max_entities> entities;
              std::size_t entity_count = manager.get_entities(entities);
              entity_count = manager.filter_inplace<component_mesh_t>(
                  std::span(entities).subspan(0, entity_count));
              entity_count = manager.filter_inplace<component_transform_t>(
                  std::span(entities).subspan(0, entity_count));

              for (ecs::entity_id_t entity :
                   entities | std::views::take(entity_count)) {
                auto *mesh = manager.get_component<component_mesh_t>(entity);
                auto *transform =
                    manager.get_component<component_transform_t>(entity);

                vk::DescriptorSet set =
                    mesh->descriptorsets->get_set(next_frame_info.flightframe);

                commandbuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    geometry_pipeline.layout(), 0, 1, &set, 0, nullptr);

                std::vector<vk::DeviceSize> offsets = {0};
                std::vector<vk::Buffer> buffers = {mesh->vertices->buffer()};
                commandbuffer.bindVertexBuffers(0, 1, buffers.data(),
                                                offsets.data());

                commandbuffer.draw(mesh->vertices_length, 1, 0, 0);
              }

              commandbuffer.endRenderPass();
              std::println("Recorded geometry task");
            }));

    graph.add_task(present_task_id,
                   std::make_unique<alex2::simple_task_t>(
                       "present", [](vk::CommandBuffer commandbuffer) {
                         std::println("Recorded present task");
                       }));

    graph.add_dependency(alex2::dependency_info_t{.device = core.device(),
                                                  .parent = upload_task_id,
                                                  .child = geometry_task_id});

    graph.add_dependency(alex2::dependency_info_t{.device = core.device(),
                                                  .parent = geometry_task_id,
                                                  .child = present_task_id});

    graph.set_end(present_task_id);

    vk::Semaphore graph_finished_semaphore =
        taskgraph_semaphores[next_frame_info.flightframe].get();

    auto graph_evaluate_info = alex2::graph_evaluate_info_t{};
    graph_evaluate_info.device = core.device();
    graph_evaluate_info.commandpool = core.commandpool();
    graph_evaluate_info.queue = core.queue();
    graph_evaluate_info.sync_semaphore = graph_finished_semaphore;
    graph.evaluate(graph_evaluate_info);

#if 0
    std::vector<alex::graph::uploadpass_command_t> upload_commands;
    std::vector<alex::graph::renderpass_command_t> geometry_pass_commands;

    alex::graph::command::set_viewport_t viewport{
        .x = 0.0f,
        .y = 0.0f,
        .w = static_cast<float>(render_extent.width),
        .h = static_cast<float>(render_extent.height)};

    alex::graph::command::set_scissor_t scissor{
        .offset = {0, 0},
        .extent = {render_extent.width, render_extent.height}};

    alex::graph::command::bind_pipeline_t bind_pipeline{
        .pipeline = geometry_pipeline.pipeline};

    geometry_pass_commands.push_back(viewport);
    geometry_pass_commands.push_back(scissor);
    geometry_pass_commands.push_back(bind_pipeline);

    std::array<ecs::entity_id_t, max_entities> entities;
    std::size_t entity_count = manager.get_entities(entities);

    entity_count = manager.filter_inplace<component_mesh_t>(
        std::span(entities).subspan(0, entity_count));

    entity_count = manager.filter_inplace<component_transform_t>(
        std::span(entities).subspan(0, entity_count));

    for (ecs::entity_id_t entity : entities | std::views::take(entity_count)) {
      auto *mesh = manager.get_component<component_mesh_t>(entity);
      auto *transform = manager.get_component<component_transform_t>(entity);

      draw_info_t draw_info;
      draw_info.view = camera.view();
      draw_info.projection = projection;
      draw_info.model = transform->mat;
      std::memcpy(
          mesh->direct_uniforms[next_frame_info.flightframe]->memory_ptr(),
          &draw_info, sizeof(draw_info));

      alex::graph::command::buffer_upload_t upload{
          .physical_device = core.physical_device(),
          .device = core.device(),
          .direct_buffer =
              &mesh->direct_uniforms[next_frame_info.flightframe].value(),
          .buffer = &mesh->uniforms[next_frame_info.flightframe].value()};

      upload_commands.push_back(upload);

      alex::graph::command::bind_descriptorsets_t bind_descriptorsets{
          .layout = geometry_pipeline.layout,
          .sets = {mesh->descriptorsets->get_set(next_frame_info.flightframe)},
      };

      alex::graph::command::bind_vertexbuffer_t bind_vertexbuffer{
          .first_binding = 0,
          .binding_offsets = {0},
          .first_buffer = 0,
          .buffers = {mesh->vertices->buffer()}};

      alex::graph::command::draw_t draw{.instance_count = 1,
                                        .first_instance = 0,
                                        .vertex_count = mesh->vertices_length,
                                        .first_vertex = 0};

      geometry_pass_commands.push_back(bind_descriptorsets);
      geometry_pass_commands.push_back(bind_vertexbuffer);
      geometry_pass_commands.push_back(draw);
    }

    alex::graph::evaluate_info_t evaluate_info;
    evaluate_info.flightframe = next_frame_info.flightframe;
    evaluate_info.queue = core.queue();
    evaluate_info.uploadpass_commands.emplace_back("upload", upload_commands);
    evaluate_info.renderpass_commands.emplace_back("geometry-pass",
                                                   geometry_pass_commands);
    graph.evaluate(evaluate_info);
#endif

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

    //   presentation_info.commandbuffer =
    //       next_frame_info.presentation_commandbuffer;

    presenter.present(presentation_info);
    deltaclock.tick();
  }

  core.device().waitIdle();
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
