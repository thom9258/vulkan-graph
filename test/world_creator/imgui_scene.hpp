#pragma once

#include <alex/core.hpp>
#include <alex/flightframe_array.hpp>
#include <alex/log.hpp>
#include <alex/task_graph.hpp>

#include "../utility/camera.hpp"
#include "../utility/scenestack.hpp"
#include "../utility/sdl.hpp"

#include "entity.hpp"
#include "glm_transform_hierarchy.hpp"
#include "imgui.h"
#include "resource_loader.hpp"
#include "static_mesh_entity.hpp"
#include "resources.hpp"
#include "static_render.hpp"
#include "ui/entity_hierarchy.hpp"
#include "ui/game_manager.hpp"
#include "ui/imgui_context.hpp"
#include "ui/level_settings.hpp"
#include "utility/transform_hierarchy.hpp"
#include "world.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <print>

namespace game {

struct imgui_scene_info_t {
  alex::core_t *core{nullptr};
  alex::presenter_t *presenter{nullptr};
  sdl::window_t *window{nullptr};
  static_render_t *static_render{nullptr};
  debugui_rendering_t *debugui_rendering{nullptr};
  imgui_context_t *imgui_context{nullptr};
};

class imgui_scene : public scene::scene_t {
public:
  imgui_scene(imgui_scene_info_t &info);

  constexpr ~imgui_scene() override;

  constexpr auto load() -> void override;

  constexpr auto tick() -> scene::status_t override;

  constexpr auto unload() -> void override;

  constexpr auto update_window() -> scene::status_t;

  constexpr auto update_input() -> scene::status_t;

  constexpr auto update_logic() -> scene::status_t;

  constexpr auto update_render() -> scene::status_t;

private:
  alex::core_t *_core{nullptr};
  alex::presenter_t *_presenter{nullptr};
  sdl::window_t *_window{nullptr};
  static_render_t *_static_render{nullptr};
  debugui_rendering_t *_debugui_rendering{nullptr};
  imgui_context_t *_imgui_context{nullptr};

  alex::flightframe_array_t<alex::graph_t> _rendergraphs;
  alex::flightframe_array_t<vk::UniqueSemaphore> _rendergraph_semaphores;

  std::optional<resources_t> _resources;
  std::optional<world_t> _world;

  ui::entity_hierarchy_t _entity_hierarchy;
  ui::game_manager_t _game_manager;
  ui::level_settings_t _level_settings;
};

imgui_scene::imgui_scene(imgui_scene_info_t &info)
    : _core{info.core}, _presenter{info.presenter}, _window{info.window},
      _static_render{info.static_render},
      _debugui_rendering{info.debugui_rendering},
      _imgui_context{info.imgui_context} {

  for (vk::UniqueSemaphore &semaphore : _rendergraph_semaphores) {
    semaphore = _core->create_semaphore();
  }

  _resources.emplace(_core, "../asset_manifest.json");

  _world.emplace(_window->window_extent(), _core, _static_render,
                 &_resources.value(), _imgui_context);

  _entity_hierarchy =
      ui::entity_hierarchy_t(&_world.value(), &_world->transform_hierarchy(),
                             &(_resources.value()), _core, _static_render);


  _level_settings = ui::level_settings_t(&_world.value());
}

constexpr imgui_scene::~imgui_scene() {}

constexpr auto imgui_scene::load() -> void {}

constexpr auto imgui_scene::tick() -> scene::status_t {
  scene::status_t status;
  status = update_window();
  if (status != scene::status_t::ok) {
    return status;
  }

  status = update_input();
  if (status != scene::status_t::ok) {
    return status;
  }

  status = update_logic();
  if (status != scene::status_t::ok) {
    return status;
  }

  status = update_render();
  if (status != scene::status_t::ok) {
    return status;
  }

  return scene::status_t::ok;
}

constexpr auto imgui_scene::unload() -> void {}

constexpr auto imgui_scene::update_window() -> scene::status_t {
  _window->mark_next_frame();
  return scene::status_t::ok;
}

constexpr auto imgui_scene::update_input() -> scene::status_t {
  std::span<SDL_Event> events = _window->get_events();

  for (SDL_Event event : events) {
    _imgui_context->process_event(&event);
    switch (event.type) {
    case SDL_QUIT:
      return scene::status_t::shutdown;
    }
  }

  _game_manager.update_input(events);
  if (_game_manager.should_close()) {
    return scene::status_t::shutdown;
  }

  _entity_hierarchy.update_input(events);

  _world->update_input(events);

  return scene::status_t::ok;
}

constexpr auto imgui_scene::update_logic() -> scene::status_t {
  double const deltatime = _window->deltatime_seconds();
  _world->update_logic(deltatime);
  return scene::status_t::ok;
}

constexpr auto imgui_scene::update_render() -> scene::status_t {
  alex::next_frame_info_t next_frame_info =
      _presenter->wait_for_next_frame(_core->device());

  _rendergraphs[next_frame_info.flightframe] = alex::graph_t();
  alex::graph_t &graph = _rendergraphs[next_frame_info.flightframe];

  alex::task_id_t constexpr const upload_task_id(0);
  alex::task_id_t constexpr const geometry_task_id(1);
  alex::task_id_t constexpr const debugui_task_id(2);
  alex::task_id_t constexpr const blit_geometry_to_debugui_task_id(3);

  graph.add_dependency(alex::dependency_info_t{.device = _core->device(),
                                               .parent = upload_task_id,
                                               .child = geometry_task_id});

  graph.add_dependency(
      alex::dependency_info_t{.device = _core->device(),
                              .parent = geometry_task_id,
                              .child = blit_geometry_to_debugui_task_id});

  graph.add_dependency(
      alex::dependency_info_t{.device = _core->device(),
                              .parent = blit_geometry_to_debugui_task_id,
                              .child = debugui_task_id});

  graph.set_end(debugui_task_id);

  graph.add_task(
      upload_task_id,
      std::make_unique<alex::simple_task_t>(
          "upload", [&](vk::CommandBuffer commandbuffer) {
            for (entity_t &entity : _world->entities()) {
              if (auto *static_mesh = entity.get<static_mesh_entity_t>()) {
                static_mesh_entity_update_info_t update_info;
                update_info.physical_device = _core->physical_device();
                update_info.device = _core->device();
                update_info.commandbuffer = commandbuffer;
                update_info.flightframe = next_frame_info.flightframe;
                update_info.transform_hierarchy =
                    &_world->transform_hierarchy();
                update_info.camera_view = _world->camera().view();
                update_info.camera_projection = _world->camera().projection();
                static_mesh->resource_update(update_info);
              }
            }
          }));

  graph.add_task(
      blit_geometry_to_debugui_task_id,
      std::make_unique<alex::simple_task_t>(
          "blit geometry to ui texture", [&](vk::CommandBuffer commandbuffer) {
            // Transition ui texture image to transfer dst
            {
              auto range = vk::ImageSubresourceRange{}
                               .setAspectMask(vk::ImageAspectFlagBits::eColor)
                               .setBaseMipLevel(0)
                               .setLevelCount(1)
                               .setBaseArrayLayer(0)
                               .setLayerCount(1);

              auto barrier =
                  vk::ImageMemoryBarrier{}
                      .setImage(_debugui_rendering->attachments
                                    .color[next_frame_info.flightframe]
                                    .image())
                      .setSubresourceRange(range)
                      .setOldLayout(vk::ImageLayout::eUndefined)
                      .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                      .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                      .setDstAccessMask(vk::AccessFlags())
                      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

              commandbuffer.pipelineBarrier(
                  vk::PipelineStageFlagBits::eTransfer,
                  vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
                  nullptr, nullptr, barrier);
            }

            // Transition geometry rendertarget to transfer src
            {
              auto range = vk::ImageSubresourceRange{}
                               .setAspectMask(vk::ImageAspectFlagBits::eColor)
                               .setBaseMipLevel(0)
                               .setLevelCount(1)
                               .setBaseArrayLayer(0)
                               .setLayerCount(1);

              auto barrier =
                  vk::ImageMemoryBarrier{}
                      .setImage(
                          _static_render
                              ->color_attachments()[next_frame_info.flightframe]
                              .image())
                      .setSubresourceRange(range)
                      .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                      .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                      .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                      .setDstAccessMask(vk::AccessFlags())
                      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

              commandbuffer.pipelineBarrier(
                  vk::PipelineStageFlagBits::eTransfer,
                  vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
                  nullptr, nullptr, barrier);
            }

            // Blit geometry texture to ui texture
            {
              auto src_subresource =
                  vk::ImageSubresourceLayers{}
                      .setAspectMask(vk::ImageAspectFlagBits::eColor)
                      .setBaseArrayLayer(0)
                      .setLayerCount(1)
                      .setMipLevel(0);
              const std::array<vk::Offset3D, 2> src_offsets{
                  vk::Offset3D{0, 0, 0},
                  vk::Offset3D{
                      static_cast<std::int32_t>(_static_render->extent().width),
                      static_cast<std::int32_t>(
                          _static_render->extent().height),
                      1}};

              auto dst_subresource =
                  vk::ImageSubresourceLayers{}
                      .setAspectMask(vk::ImageAspectFlagBits::eColor)
                      .setBaseArrayLayer(0)
                      .setLayerCount(1)
                      .setMipLevel(0);

              const std::array<vk::Offset3D, 2> dst_offsets{
                  vk::Offset3D{0, 0, 0},
                  vk::Offset3D{static_cast<std::int32_t>(
                                   _debugui_rendering->_extent.width),
                               static_cast<std::int32_t>(
                                   _debugui_rendering->_extent.height),
                               1}};

              auto image_blit = vk::ImageBlit{}
                                    .setSrcOffsets(src_offsets)
                                    .setSrcSubresource(src_subresource)
                                    .setDstOffsets(dst_offsets)
                                    .setDstSubresource(dst_subresource);

              commandbuffer.blitImage(
                  _static_render
                      ->color_attachments()[next_frame_info.flightframe]
                      .image(),
                  vk::ImageLayout::eTransferSrcOptimal,
                  _debugui_rendering->attachments
                      .color[next_frame_info.flightframe]
                      .image(),
                  vk::ImageLayout::eTransferDstOptimal, image_blit,
                  vk::Filter::eNearest);
            }
          }));

  graph.add_task(
      geometry_task_id,
      std::make_unique<alex::simple_task_t>("geometry", [&](vk::CommandBuffer
                                                                commandbuffer) {
        const auto render_area =
            vk::Rect2D{}
                .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                .setExtent(vk::Extent2D(_static_render->extent().width,
                                        _static_render->extent().height));

        auto clearvalues = _static_render->renderpass().clearvalues();

        const auto renderpass_begin_info =
            vk::RenderPassBeginInfo{}
                .setRenderPass(_static_render->renderpass().renderpass())
                .setFramebuffer(_static_render->renderpass().framebuffer(
                    next_frame_info.flightframe))
                .setRenderArea(render_area)
                .setClearValues(clearvalues);

        commandbuffer.beginRenderPass(renderpass_begin_info,
                                      vk::SubpassContents::eInline);

        auto viewport =
            vk::Viewport{}
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(_static_render->extent().width))
                .setHeight(static_cast<float>(_static_render->extent().height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
            {_static_render->extent().width, _static_render->extent().height});

        commandbuffer.setViewport(0, viewport);
        commandbuffer.setScissor(0, scissor);
        commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   _static_render->pipeline().pipeline());

        for (entity_t &entity : _world->entities()) {
          if (auto *static_mesh = entity.get<static_mesh_entity_t>()) {
            static_mesh_entity_draw_info_t info;
            info.commandbuffer = commandbuffer;
            info.flightframe = next_frame_info.flightframe;
            info.geometry_pipeline_layout = _static_render->pipeline().layout();
            static_mesh->draw(info);
          }
        }
        commandbuffer.endRenderPass();
      }));

  graph.add_task(
      debugui_task_id,
      std::make_unique<alex::simple_task_t>(
          "debugui", [&](vk::CommandBuffer commandbuffer) {
            const auto render_area =
                vk::Rect2D{}
                    .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                    .setExtent(
                        vk::Extent2D(_debugui_rendering->_extent.width,
                                     _debugui_rendering->_extent.height));

            const auto renderpass_begin_info =
                vk::RenderPassBeginInfo{}
                    .setRenderPass(_debugui_rendering->renderpass->renderpass())
                    .setFramebuffer(_debugui_rendering->renderpass->framebuffer(
                        next_frame_info.flightframe))
                    .setRenderArea(render_area);

            commandbuffer.beginRenderPass(renderpass_begin_info,
                                          vk::SubpassContents::eInline);

            auto viewport =
                vk::Viewport{}
                    .setX(0)
                    .setY(0)
                    .setWidth(
                        static_cast<float>(_debugui_rendering->_extent.width))
                    .setHeight(
                        static_cast<float>(_debugui_rendering->_extent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);

            auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
                {_debugui_rendering->_extent.width,
                 _debugui_rendering->_extent.height});

            commandbuffer.setViewport(0, viewport);
            commandbuffer.setScissor(0, scissor);

            _game_manager.draw();

            if (_game_manager.show_entity_hierarchy()) {
              _entity_hierarchy.draw(_world.value());
            }

            if (_game_manager.show_level_settings()) {
              _level_settings.draw();
              _static_render->renderpass().set_color_clearvalue(
                  _world->background_color().r(),
                  _world->background_color().g(),
                  _world->background_color().b());
            }


            ImGui::Render();
            ImDrawData *draw_data = ImGui::GetDrawData();
            _imgui_context->render_draw_data(draw_data, commandbuffer);

            commandbuffer.endRenderPass();
          }));

  vk::Semaphore graph_finished_semaphore =
      _rendergraph_semaphores[next_frame_info.flightframe].get();

  _imgui_context->new_frame();

  auto graph_evaluate_info = alex::graph_evaluate_info_t{};
  graph_evaluate_info.device = _core->device();
  graph_evaluate_info.commandpool = _core->commandpool();
  graph_evaluate_info.queue = _core->queue();
  graph_evaluate_info.sync_semaphore = graph_finished_semaphore;
  graph.evaluate(graph_evaluate_info);

  alex::presentation_info_t presentation_info;
  presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
  presentation_info.source_offset_end = vk::Offset3D{
      static_cast<std::int32_t>(_debugui_rendering->_extent.width),
      static_cast<std::int32_t>(_debugui_rendering->_extent.height), 1};

  presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
  presentation_info.destination_offset_end = vk::Offset3D{
      static_cast<std::int32_t>(_presenter->window_extent.width),
      static_cast<std::int32_t>(_presenter->window_extent.height), 1};

  presentation_info.blit_filter = vk::Filter::eNearest;
  presentation_info.image =
      _debugui_rendering->attachments.color[next_frame_info.flightframe]
          .image();
  presentation_info.layout = vk::ImageLayout::eColorAttachmentOptimal;
  presentation_info.queue = _core->queue();
  presentation_info.wait_semaphore = graph_finished_semaphore;
  _presenter->present(presentation_info);

  return scene::status_t::ok;
}

} // namespace game
