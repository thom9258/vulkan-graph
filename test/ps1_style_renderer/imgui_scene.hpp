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
#include "imgui_context.hpp"
#include "ps1_style_renderer/resource_loader.hpp"
#include "ps1_style_renderer/static_mesh_entity.hpp"
#include "rendering.hpp"
#include "resources.hpp"
#include "static_resources.hpp"
#include "ui_game_manager.hpp"
#include "ui_level_editor.hpp"
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
  geometry_rendering_t *geometry_rendering{nullptr};
  debugui_rendering_t *debugui_rendering{nullptr};
  imgui_context_t *imgui_context{nullptr};
  static_resources_t *static_resources{nullptr};
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

  constexpr auto create_chest(std::string_view name,
                              transform_hierarchy::transform_id_t transform_id)
      -> static_mesh_entity_t;

private:
  alex::core_t *_core{nullptr};
  alex::presenter_t *_presenter{nullptr};
  sdl::window_t *_window{nullptr};
  geometry_rendering_t *_geometry_rendering{nullptr};
  debugui_rendering_t *_debugui_rendering{nullptr};
  imgui_context_t *_imgui_context{nullptr};
  static_resources_t *_static_resources{nullptr};

  alex::flightframe_array_t<alex::graph_t> _rendergraphs;
  alex::flightframe_array_t<vk::UniqueSemaphore> _rendergraph_semaphores;
  std::optional<world_t> _world;
  game::ui_level_editor_t _ui_level_editor;
  game::ui_game_manager_t _ui_game_manager;

  std::optional<resources_t> _resources;
  std::optional<model_source_t> _fox;
};

constexpr auto
imgui_scene::create_chest(std::string_view name,
                          transform_hierarchy::transform_id_t transform_id)
    -> static_mesh_entity_t {
  static_mesh_entity_t chest(name, transform_id);

  // TODO: this is not a good way to do things
  chest._static_model_ref.emplace();
  chest._static_model_ref->meshes.emplace_back();
  detail::static_mesh_ref_t &static_mesh =
      chest._static_model_ref->meshes.back();

  static_mesh.vertices = &_static_resources->chest_model()
                              ->root()
                              .children[0]
                              .meshes[0]
                              .vertices.value();

  static_mesh.vertices_length = _static_resources->chest_model()
                                    ->root()
                                    .children[0]
                                    .meshes[0]
                                    .vertices_length;

  static_mesh.indices = &_static_resources->chest_model()
                             ->root()
                             .children[0]
                             .meshes[0]
                             .indices.value();

  static_mesh.indices_length = _static_resources->chest_model()
                                   ->root()
                                   .children[0]
                                   .meshes[0]
                                   .indices_length;

  _core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) -> void {
    draw_info_t draw_info;

    alex::direct_memory_buffer_info_t direct_uniform_info;
    direct_uniform_info.physical_device = _core->physical_device();
    direct_uniform_info.device = _core->device();
    direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
    direct_uniform_info.memory_size = sizeof(draw_info);

    for (std::size_t i = 0; i < alex::frames_in_flight; i++) {
      static_mesh.direct_uniforms.emplace_back(direct_uniform_info);
      std::memcpy(static_mesh.direct_uniforms.back().memory_ptr(), &draw_info,
                  sizeof(draw_info));
    }

    for (std::size_t i = 0; i < alex::frames_in_flight; i++) {
      alex::memory_buffer_info_t uniform_info;
      uniform_info.physical_device = _core->physical_device();
      uniform_info.device = _core->device();
      uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
      uniform_info.memory_size = direct_uniform_info.memory_size;
      static_mesh.uniforms.emplace_back(uniform_info);

      alex::memory_buffer_write_info_t write_info;
      write_info.physical_device = _core->physical_device();
      write_info.device = _core->device();
      write_info.direct = &static_mesh.direct_uniforms[i];
      write_info.write_size = static_mesh.uniforms.back().memory_size();
      write_info.commandbuffer = commandbuffer;
      static_mesh.uniforms.back().record_write(write_info);
    }

    auto allocated_frame_uniform_descriptorsets =
        _core->allocate_repeated_descriptorsets(
            _geometry_rendering->setlayout.frame_uniform.get(),
            vk::DescriptorType::eUniformBuffer, 2);

    static_mesh.uniform_descriptor_pool =
        std::move(allocated_frame_uniform_descriptorsets.pool);
    static_mesh.uniform_descriptorsets =
        std::move(allocated_frame_uniform_descriptorsets.sets);

    for (auto [i, uniform] : static_mesh.uniforms | std::views::enumerate) {
      const auto buffer_info = vk::DescriptorBufferInfo{}
                                   .setBuffer(uniform.buffer())
                                   .setOffset(0)
                                   .setRange(uniform.memory_size());

      const std::array<vk::WriteDescriptorSet, 1> writes{
          vk::WriteDescriptorSet{}
              .setDstBinding(0)
              .setDstArrayElement(0)
              .setDstSet(static_mesh.uniform_descriptorsets[i].get())
              .setDescriptorCount(1)
              .setDescriptorType(vk::DescriptorType::eUniformBuffer)
              .setBufferInfo(buffer_info)};

      _core->device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                           nullptr);
    }

    auto allocated_diffuse_descriptorsets =
        _core->allocate_repeated_descriptorsets(
            _geometry_rendering->setlayout.diffuse.get(),
            vk::DescriptorType::eCombinedImageSampler, 2);

    static_mesh.diffuse_descriptor_pool =
        std::move(allocated_diffuse_descriptorsets.pool);
    static_mesh.diffuse_descriptorsets =
        std::move(allocated_diffuse_descriptorsets.sets);

    for (vk::UniqueDescriptorSet &diffuse_set :
         static_mesh.diffuse_descriptorsets) {
      const auto image_info =
          vk::DescriptorImageInfo{}
              .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
              .setSampler(_static_resources->chest_texture_sampler())
              .setImageView(_static_resources->chest_texture()->view());

      const std::array<vk::WriteDescriptorSet, 1> writes{
          vk::WriteDescriptorSet{}
              .setDstBinding(0)
              .setDstArrayElement(0)
              .setDstSet(diffuse_set.get())
              .setDescriptorCount(1)
              .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
              .setImageInfo(image_info)};

      _core->device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                           nullptr);
    }
  });

  return chest;
}

imgui_scene::imgui_scene(imgui_scene_info_t &info)
    : _core{info.core}, _presenter{info.presenter}, _window{info.window},
      _geometry_rendering{info.geometry_rendering},
      _debugui_rendering{info.debugui_rendering},
      _imgui_context{info.imgui_context},
      _static_resources{info.static_resources} {

  for (vk::UniqueSemaphore &semaphore : _rendergraph_semaphores) {
    semaphore = _core->create_semaphore();
  }

  _world.emplace(_window->window_extent());

  _ui_level_editor = ui_level_editor_t(&_world->transform_hierarchy());

  _resources.emplace(_core, "../resource_manifest.json");

  model_load_info_t fox_info;
  fox_info.core = _core;
  fox_info.path = "/home/th/Assets/Fox/glTF/Fox.gltf";
  fox_info.texture.filter = vk::Filter::eLinear;
  auto fox = model_source_t::create(fox_info);
  if (fox.has_value()) {
    _fox = std::move(fox.value());
  } else {
    ALEX_ERROR("ERROR loading fox: {}:{}", to_string(fox.error().code()),
               fox.error().error());
  }

  glm::mat4 translation =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
  glm::mat4 model_matrix = glm::scale(translation, glm::vec3(0.02f));
  auto id = _world->transform_hierarchy().add(model_matrix);

  _world->add_entity(create_chest("chest 0", id.value()));

  translation = glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, 0.0f, 0.0f));
  model_matrix = glm::scale(translation, glm::vec3(0.02f));
  id = _world->transform_hierarchy().add(model_matrix);
  _world->add_entity(create_chest("chest 1", id.value()));

  translation = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, -2.5f));
  model_matrix = glm::scale(translation, glm::vec3(0.02f));
  id = _world->transform_hierarchy().add_child_global_location(
      model_matrix, _world->entities().back().transform_id());
  _world->add_entity(create_chest("chest 2", id.value()));

  translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.0f, -2.0f));
  model_matrix = glm::scale(translation, glm::vec3(0.05f));
  id = _world->transform_hierarchy().add(model_matrix);
  auto fox_entity = static_mesh_entity_t("fox", id.value());
  fox_entity.set_model_source(_core, _geometry_rendering, _fox.value());
  _world->add_entity(std::move(fox_entity));
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

  _ui_game_manager.update_input(events);
  if (_ui_game_manager.should_close()) {
    return scene::status_t::shutdown;
  }

  _ui_level_editor.update_input(events);

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
                      .setImage(_geometry_rendering->attachments
                                    .color[next_frame_info.flightframe]
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
                  vk::Offset3D{static_cast<std::int32_t>(
                                   _geometry_rendering->_extent.width),
                               static_cast<std::int32_t>(
                                   _geometry_rendering->_extent.height),
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

              commandbuffer.blitImage(_geometry_rendering->attachments
                                          .color[next_frame_info.flightframe]
                                          .image(),
                                      vk::ImageLayout::eTransferSrcOptimal,
                                      _debugui_rendering->attachments
                                          .color[next_frame_info.flightframe]
                                          .image(),
                                      vk::ImageLayout::eTransferDstOptimal,
                                      image_blit, vk::Filter::eNearest);
            }
          }));

  graph.add_task(
      geometry_task_id,
      std::make_unique<alex::simple_task_t>("geometry", [&](vk::CommandBuffer
                                                                commandbuffer) {
        const auto render_area =
            vk::Rect2D{}
                .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                .setExtent(vk::Extent2D(_geometry_rendering->_extent.width,
                                        _geometry_rendering->_extent.height));

        auto clearvalues = _geometry_rendering->renderpass->clearvalues();

        const auto renderpass_begin_info =
            vk::RenderPassBeginInfo{}
                .setRenderPass(_geometry_rendering->renderpass->renderpass())
                .setFramebuffer(_geometry_rendering->renderpass->framebuffer(
                    next_frame_info.flightframe))
                .setRenderArea(render_area)
                .setClearValues(clearvalues);

        commandbuffer.beginRenderPass(renderpass_begin_info,
                                      vk::SubpassContents::eInline);

        auto viewport =
            vk::Viewport{}
                .setX(0)
                .setY(0)
                .setWidth(
                    static_cast<float>(_geometry_rendering->_extent.width))
                .setHeight(
                    static_cast<float>(_geometry_rendering->_extent.height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
            {_geometry_rendering->_extent.width,
             _geometry_rendering->_extent.height});

        commandbuffer.setViewport(0, viewport);
        commandbuffer.setScissor(0, scissor);
        commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   _geometry_rendering->pipeline->pipeline());

        for (entity_t &entity : _world->entities()) {
          if (auto *static_mesh = entity.get<static_mesh_entity_t>()) {
            static_mesh_entity_draw_info_t info;
            info.commandbuffer = commandbuffer;
            info.flightframe = next_frame_info.flightframe;
            info.geometry_pipeline_layout =
                _geometry_rendering->pipeline->layout();
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

            _ui_game_manager.draw();

            if (_ui_game_manager.show_level_editor()) {
              _ui_level_editor.draw(_world.value());
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
