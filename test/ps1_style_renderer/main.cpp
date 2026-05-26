#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture_storage.hpp>

#include <alex/task_graph.hpp>

#include "alex/flightframe_array.hpp"

#include "../utility/button.hpp"
#include "../utility/sdl.hpp"

#include "ecs.hpp"
#include "orbit_camera.hpp"
#include "resource_loader.hpp"

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
  window_info.width = 800;
  window_info.height = 600;

  sdl::window_t window(window_info);

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
  vk::Extent3D render_extent(static_cast<std::int32_t>(window_info.width / 4),
                             static_cast<std::int32_t>(window_info.height / 4),
                             1);

  alex::core_t core(core_info);

  /* ****************************************
   * Create DescriptorSet Layout for geometry pipeline
   */
  sdl::window_extent_t window_extent = window.window_extent();

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
   * Setup ecs
   */
  std::array<ecs::manager_t::entity_t, max_entities> entity_memory;
  std::array<component_mesh_t, max_entities> mesh_components;
  std::array<component_transform_t, max_entities> transform_components;
  ecs::manager_t manager(entity_memory, mesh_components, transform_components);

  /* ****************************************
   * Load resources
   */
  // TODO: we must add the ability to provide a staging scratch buffer
  game::model_load_info_t chest_load_info;
  chest_load_info.core = &core;
  chest_load_info.path = "/home/th/Assets/ChestWowStyle/Chest.obj";
  auto chest = game::model_source_t::create(chest_load_info);

  if (!chest.has_value()) {
    std::println("resource load error: [code: {}] {}",
                 game::to_string(chest.error().code()), chest.error().error());
    return 1;
  }

  std::println("model: (loadtime: {}s) {}", chest.value().loadtime_seconds(),
               chest.value().root().name);

  for (auto &child : chest.value().root().children)
    std::println("  {}", child.name);

  /* ****************************************
   * Setup entities
   */
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
  presenter_info.enable_vsync = true;
  presenter_info.window_surface = window_surface.get();
  presenter_info.window_extent.width = window_extent.width;
  presenter_info.window_extent.height = window_extent.height;
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
          .set_renderpass(geometry_pass.renderpass())
          .set_vertex_program_path("./geometry.vert.spv")
          .set_fragment_program_path("./geometry.frag.spv")
          .add_setlayout(geometry_pipeline_info_setlayout.get())
          .add_vertex_input_binding(
              vk::VertexInputBindingDescription{}
                  .setBinding(0)
                  .setStride(sizeof(vertex_t))
                  .setInputRate(vk::VertexInputRate::eVertex))
          .add_vertex_input_attribute(
              vk::VertexInputAttributeDescription{}
                  .setBinding(0)
                  .setLocation(0)
                  .setFormat(vk::Format::eR32G32B32Sfloat)
                  .setOffset(offsetof(vertex_t, position)))
          .add_vertex_input_attribute(
              vk::VertexInputAttributeDescription{}
                  .setBinding(0)
                  .setLocation(1)
                  .setFormat(vk::Format::eR32G32B32Sfloat)
                  .setOffset(offsetof(vertex_t, color)));

  alex::pipeline_t geometry_pipeline(geometry_pipeline_info, init_arena);

  alex::flightframe_array_t<vk::UniqueSemaphore> taskgraph_semaphores;
  for (vk::UniqueSemaphore &semaphore : taskgraph_semaphores) {
    semaphore = core.create_semaphore();
  }

  alex::flightframe_array_t<alex::graph_t> taskgraphs;

  float const camera_radius = 4.0f;
  glm::vec3 const target(0.0, 0.0, 0);
  OrbitCamera camera(target, camera_radius);

  const float aspect = window_extent.aspect();
  const float near_plane = 1.0f, far_plane = 20.0f;
  glm::mat4 const projection = std::invoke([&]() {
    glm::mat4 p =
        glm::perspective(glm::radians(70.f), aspect, near_plane, far_plane);
    p[1][1] *= -1.0f;
    return p;
  });

  {
    auto now = std::chrono::high_resolution_clock::now();
    auto initialization_time =
        std::chrono::duration_cast<std::chrono::duration<double>>(
            now - program_start_time);
    std::println("Initialization time: {}", initialization_time);
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
    window.mark_next_frame();
    std::span<SDL_Event> events = window.get_events();
    double const deltatime = window.deltatime_seconds();
    double const movespeed = 5.0f * deltatime;

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

    auto upload_task_id = alex::task_id_t(0);
    auto geometry_task_id = alex::task_id_t(1);

    taskgraphs[next_frame_info.flightframe] = alex::graph_t();
    alex::graph_t &graph = taskgraphs[next_frame_info.flightframe];

    graph.add_task(
        upload_task_id,
        std::make_unique<alex::simple_task_t>(
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

              std::array<ecs::entity_id_t, max_entities> entities;
              std::size_t entity_count = manager.get_entities(entities);
              entity_count = manager.filter_inplace<component_mesh_t>(
                  std::span(entities).subspan(0, entity_count));
              entity_count = manager.filter_inplace<component_transform_t>(
                  std::span(entities).subspan(0, entity_count));

              for (ecs::entity_id_t entity :
                   entities | std::views::take(entity_count)) {
                auto *mesh = manager.get_component<component_mesh_t>(entity);

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

  core.device().waitIdle();

  return 0;
}
