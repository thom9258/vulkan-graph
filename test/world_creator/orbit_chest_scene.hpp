#pragma once

#include <alex/core.hpp>
#include <alex/flightframe_array.hpp>
#include <alex/task_graph.hpp>

#include "../utility/scenestack.hpp"
#include "../utility/button.hpp"
#include "../utility/orbit_camera.hpp"
#include "../utility/sdl.hpp"

#include "glm_transform_hierarchy.hpp"
#include "rendering.hpp"
#include "resource_loader.hpp"
#include "static_object.hpp"

#include <functional>
#include <print>

namespace game {

auto constexpr print_model_names(int indent, model_t &model) -> void {
  for (int i = 0; i < indent; i++) {
    std::print(" ");
  }

  std::println("{}", model.name);
  for (model_t &child : model.children) {
    print_model_names(indent++, child);
  }
}

struct player_update_info_t {
  std::span<SDL_Event> sdl_events;
  double deltatime;
};

struct player_t {
  button_t _w;
  button_t _a;
  button_t _s;
  button_t _d;
  button_t _e;
  button_t _q;

  OrbitCamera _camera;
  glm::mat4 _projection;
  double _rotatespeed = 5.0f;
  double _zoomspeed = 8.0f;

  constexpr auto update(player_update_info_t &info) -> void;
};

constexpr auto player_t::update(player_update_info_t &info) -> void {
  for (SDL_Event event : info.sdl_events) {
    switch (event.type) {
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
      case SDLK_w:
        _w.release();
        break;
      case SDLK_s:
        _s.release();
        break;
      case SDLK_a:
        _a.release();
        break;
      case SDLK_d:
        _d.release();
        break;
      case SDLK_e:
        _e.release();
        break;
      case SDLK_q:
        _q.release();
        break;
      }
      break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_w:
        _w.press();
        break;
      case SDLK_s:
        _s.press();
        break;
      case SDLK_a:
        _a.press();
        break;
      case SDLK_d:
        _d.press();
        break;
      case SDLK_e:
        _e.press();
        break;
      case SDLK_q:
        _q.press();
        break;
      }
    }
  }

  const auto rotatespeed = _rotatespeed * info.deltatime;
  const auto zoomspeed = _zoomspeed * info.deltatime;

  if (_w.is_pressed()) {
    _camera.add_rotation(rotatespeed, 0.0f);
  }
  if (_a.is_pressed()) {
    _camera.add_rotation(0.0f, rotatespeed);
  }
  if (_s.is_pressed()) {
    _camera.add_rotation(-rotatespeed, 0.0f);
  }
  if (_d.is_pressed()) {
    _camera.add_rotation(0.0f, -rotatespeed);
  }
  if (_e.is_pressed()) {
    _camera.set_radius(_camera.radius() + zoomspeed);
  }
  if (_q.is_pressed()) {
    _camera.set_radius(_camera.radius() - zoomspeed);
  }
}

struct orbit_chest_scene_info_t {
  std::string name{""};
  alex::core_t *core{nullptr};
  alex::presenter_t *presenter{nullptr};
  rendering_t *rendering{nullptr};
  sdl::window_t *window{nullptr};
};

class orbit_chest_scene : public scene::scene_t {
public:
  constexpr orbit_chest_scene(orbit_chest_scene_info_t &info)
      : _name{info.name}, _core{info.core}, _rendering{info.rendering},
        _presenter{info.presenter}, _window{info.window} {};

  constexpr ~orbit_chest_scene() override {}

  constexpr auto load() -> void override {
    std::println("{} Loaded", _name);

    /* ****************************************
     * Load resources
     */
    // TODO: we must add the ability to provide a staging scratch buffer
    //       for optimal memory usage
    model_load_info_t chest_load_info;
    chest_load_info.core = _core;
    chest_load_info.path = "/home/th/Assets/ChestWowStyle/Chest.obj";
    {
      auto chest = model_source_t::create(chest_load_info);
      if (!chest.has_value()) {
        std::println("resource load error: [code: {}] {}",
                     game::to_string(chest.error().code()),
                     chest.error().error());
        return;
      }

      _chest = std::move(chest.value());
    }

    std::println("model: (loadtime: {}s) {}", _chest.value().loadtime_seconds(),
                 _chest.value().root().name);

    print_model_names(1, _chest.value().root());

    auto chest_diffuse_bitmap =
        game::bitmap_t::create("/home/th/Assets/ChestWowStyle/diffuse.tga",
                               game::bitmap_format_t::rgb);

    if (!chest_diffuse_bitmap.has_value()) {
      std::println("Could not load chest diffuse bitmap");
      return;
    }

    alex::direct_memory_buffer_t chest_diffuse_buffer =
        chest_diffuse_bitmap->make_direct_buffer(_core->physical_device(),
                                                 _core->device());

    const vk::Format chest_diffuse_texture_format =
        game::to_vk_format(chest_diffuse_bitmap->format());

    alex::texture_info_t chest_diffuse_texture_info;
    chest_diffuse_texture_info.physical_device = _core->physical_device();
    chest_diffuse_texture_info.device = _core->device();
    chest_diffuse_texture_info.extent = vk::Extent2D{
        static_cast<std::uint32_t>(chest_diffuse_bitmap->width()),
        static_cast<std::uint32_t>(chest_diffuse_bitmap->height())};
    chest_diffuse_texture_info.format = chest_diffuse_texture_format;

    chest_diffuse_texture_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
    chest_diffuse_texture_info.usage =
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

    _chest_diffuse_texture.emplace(chest_diffuse_texture_info);

    _core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
      // Transition image to color override
      {
        auto range = vk::ImageSubresourceRange{}
                         .setAspectMask(vk::ImageAspectFlagBits::eColor)
                         .setBaseMipLevel(0)
                         .setLevelCount(1)
                         .setBaseArrayLayer(0)
                         .setLayerCount(1);

        auto barrier =
            vk::ImageMemoryBarrier{}
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setImage(_chest_diffuse_texture->image())
                .setSubresourceRange(range)
                .setSrcAccessMask(vk::AccessFlags())
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

        commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::DependencyFlags(), nullptr, nullptr,
                                      barrier);
      }

      // Copy buffer into image
      {
        auto layer = vk::ImageSubresourceLayers{}
                         .setAspectMask(vk::ImageAspectFlagBits::eColor)
                         .setMipLevel(0)
                         .setBaseArrayLayer(0)
                         .setLayerCount(1);

        const auto offset = vk::Offset3D{}.setX(0).setY(0).setZ(0);

        const auto extent = vk::Extent3D{}
                                .setWidth(chest_diffuse_bitmap->width())
                                .setHeight(chest_diffuse_bitmap->height())
                                .setDepth(1);

        auto region = vk::BufferImageCopy{}
                          .setBufferOffset(0)
                          .setBufferRowLength(0)
                          .setBufferImageHeight(0)
                          .setImageSubresource(layer)
                          .setImageOffset(offset)
                          .setImageExtent(extent);

        commandbuffer.copyBufferToImage(
            chest_diffuse_buffer.buffer(), _chest_diffuse_texture->image(),
            vk::ImageLayout::eTransferDstOptimal, region);
      }

      // Transfer image to shader readonly optimal
      {
        const auto source_range =
            vk::ImageSubresourceRange{}
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto barrier =
            vk::ImageMemoryBarrier{}
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImage(_chest_diffuse_texture->image())
                .setSubresourceRange(source_range)
                .setSrcAccessMask(vk::AccessFlags())
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

        commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::DependencyFlags(), nullptr, nullptr,
                                      barrier);
      }
    });

    // Construct a sampler for the texture
    const auto features = _core->physical_device().getFeatures();
    const auto properties = _core->physical_device().getProperties();
    const auto max_anisotropy =
        features.samplerAnisotropy
            ? std::min(4.0f, properties.limits.maxSamplerAnisotropy)
            : 1.0f;

    const vk::Filter filter = vk::Filter::eNearest;
    const auto sampler_info =
        vk::SamplerCreateInfo{}
            .setMagFilter(filter)
            .setMinFilter(filter)
            .setAddressModeU(vk::SamplerAddressMode::eRepeat)
            .setAddressModeV(vk::SamplerAddressMode::eRepeat)
            .setAddressModeW(vk::SamplerAddressMode::eRepeat)
            .setAnisotropyEnable(features.samplerAnisotropy)
            .setMaxAnisotropy(max_anisotropy)
            .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
            .setUnnormalizedCoordinates(false)
            .setCompareEnable(false)
            .setCompareOp(vk::CompareOp::eAlways)
            .setMipmapMode(vk::SamplerMipmapMode::eLinear)
            .setMipLodBias(0.0f)
            .setMinLod(0.0f)
            .setMaxLod(0.0f);

    _chest_diffuse_texture_sampler =
        _core->device().createSamplerUnique(sampler_info);

    /* ****************************************
     * Setup Static Object
     */

    _transform_hierarchy = glm_transform_hierarchy(256);

    {
      static_object_t chest;
      glm::mat4 model_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.04f));
      chest.transform_id = _transform_hierarchy->add(model_matrix).value();

      chest.vertices =
          &(_chest.value().root().children.at(0).meshes.at(0).vertices.value());
      chest.vertices_length =
          _chest.value().root().children.at(0).meshes.at(0).vertices_length;

      chest.indices =
          &(_chest.value().root().children.at(0).meshes.at(0).indices.value());
      chest.indices_length =
          _chest.value().root().children.at(0).meshes.at(0).indices_length;

      _core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
        draw_info_t draw_info;

        alex::direct_memory_buffer_info_t direct_uniform_info;
        direct_uniform_info.physical_device = _core->physical_device();
        direct_uniform_info.device = _core->device();
        direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
        direct_uniform_info.memory_size = sizeof(draw_info);

        for (std::size_t i = 0; i < alex::frames_in_flight; i++) {
          chest.direct_uniforms.emplace_back(direct_uniform_info);
          std::memcpy(chest.direct_uniforms.back().memory_ptr(), &draw_info,
                      sizeof(draw_info));
        }

        for (std::size_t i = 0; i < alex::frames_in_flight; i++) {
          alex::memory_buffer_info_t uniform_info;
          uniform_info.physical_device = _core->physical_device();
          uniform_info.device = _core->device();
          uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
          uniform_info.memory_size = direct_uniform_info.memory_size;
          chest.uniforms.emplace_back(uniform_info);

          alex::memory_buffer_write_info_t write_info;
          write_info.physical_device = _core->physical_device();
          write_info.device = _core->device();
          write_info.direct = &chest.direct_uniforms[i];
          write_info.write_size = chest.uniforms.back().memory_size();
          write_info.commandbuffer = commandbuffer;
          chest.uniforms.back().record_write(write_info);
        }

        auto allocated_frame_uniform_descriptorsets =
            _core->allocate_repeated_descriptorsets(
                _rendering->_geometry.setlayout.frame_uniform.get(),
                vk::DescriptorType::eUniformBuffer, 2);

        chest.uniform_descriptor_pool =
            std::move(allocated_frame_uniform_descriptorsets.pool);
        chest.uniform_descriptorsets =
            std::move(allocated_frame_uniform_descriptorsets.sets);

        for (auto [i, uniform] : chest.uniforms | std::views::enumerate) {
          const auto buffer_info = vk::DescriptorBufferInfo{}
                                       .setBuffer(uniform.buffer())
                                       .setOffset(0)
                                       .setRange(uniform.memory_size());

          const std::array<vk::WriteDescriptorSet, 1> writes{
              vk::WriteDescriptorSet{}
                  .setDstBinding(0)
                  .setDstArrayElement(0)
                  .setDstSet(chest.uniform_descriptorsets[i].get())
                  .setDescriptorCount(1)
                  .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                  .setBufferInfo(buffer_info)};

          _core->device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                               nullptr);
        }

        auto allocated_diffuse_descriptorsets =
            _core->allocate_repeated_descriptorsets(
                _rendering->_geometry.setlayout.diffuse.get(),
                vk::DescriptorType::eCombinedImageSampler, 2);

        chest.diffuse_descriptor_pool =
            std::move(allocated_diffuse_descriptorsets.pool);
        chest.diffuse_descriptorsets =
            std::move(allocated_diffuse_descriptorsets.sets);

        for (vk::UniqueDescriptorSet &diffuse_set :
             chest.diffuse_descriptorsets) {
          const auto image_info =
              vk::DescriptorImageInfo{}
                  .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                  .setSampler(_chest_diffuse_texture_sampler.get())
                  .setImageView(_chest_diffuse_texture->view());

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

      _static_objects.push_back(std::move(chest));
    }

    float const camera_radius = 4.0f;
    glm::vec3 const target(0.0f, 0.5f, 0.0f);
    _player._camera = OrbitCamera(target, camera_radius);

    const float aspect = _window->window_extent().aspect();
    const float near_plane = 0.1f, far_plane = 200.0f;
    _player._projection = std::invoke([&]() {
      glm::mat4 p =
          glm::perspective(glm::radians(70.f), aspect, near_plane, far_plane);
      p[1][1] *= -1.0f;
      return p;
    });

    for (vk::UniqueSemaphore &semaphore : _rendergraph_semaphores) {
      semaphore = _core->create_semaphore();
    }
  }

  constexpr auto tick() -> scene::status_t override {
    _window->mark_next_frame();
    std::span<SDL_Event> events = _window->get_events();
    double const deltatime = _window->deltatime_seconds();

    player_update_info_t player_update_info;
    player_update_info.sdl_events = events;
    player_update_info.deltatime = deltatime;
    _player.update(player_update_info);

    for (SDL_Event event : events) {
      switch (event.type) {
      case SDL_QUIT:
        return scene::status_t::shutdown;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          return scene::status_t::shutdown;
        }
        break;
      }
    }

    alex::next_frame_info_t next_frame_info =
        _presenter->wait_for_next_frame(_core->device());

    auto upload_task_id = alex::task_id_t(0);
    auto geometry_task_id = alex::task_id_t(1);

    _rendergraphs[next_frame_info.flightframe] = alex::graph_t();
    alex::graph_t &graph = _rendergraphs[next_frame_info.flightframe];

    graph.add_task(
        upload_task_id,
        std::make_unique<alex::simple_task_t>(
            "upload", [&](vk::CommandBuffer commandbuffer) {
              for (static_object_t &model : _static_objects) {
                static_object_update_info_t update_info;
                update_info.physical_device = _core->physical_device();
                update_info.device = _core->device();
                update_info.commandbuffer = commandbuffer;
                update_info.flightframe = next_frame_info.flightframe;
                update_info.transform_hierarchy = &_transform_hierarchy.value();
                update_info.camera_view = _player._camera.view();
                update_info.camera_projection = _player._projection;
                model.update(update_info);
              }
            }));

    graph.add_task(
        geometry_task_id,
        std::make_unique<alex::simple_task_t>(
            "geometry", [&](vk::CommandBuffer commandbuffer) {
              const auto render_area =
                  vk::Rect2D{}
                      .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                      .setExtent(
                          vk::Extent2D(_rendering->_render_extent.width,
                                       _rendering->_render_extent.height));

              auto clearvalues =
                  _rendering->_geometry.renderpass->clearvalues();

              const auto renderpass_begin_info =
                  vk::RenderPassBeginInfo{}
                      .setRenderPass(
                          _rendering->_geometry.renderpass->renderpass())
                      .setFramebuffer(
                          _rendering->_geometry.renderpass->framebuffer(
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
                          static_cast<float>(_rendering->_render_extent.width))
                      .setHeight(
                          static_cast<float>(_rendering->_render_extent.height))
                      .setMinDepth(0.0f)
                      .setMaxDepth(1.0f);

              auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
                  {_rendering->_render_extent.width,
                   _rendering->_render_extent.height});

              commandbuffer.setViewport(0, viewport);
              commandbuffer.setScissor(0, scissor);
              commandbuffer.bindPipeline(
                  vk::PipelineBindPoint::eGraphics,
                  _rendering->_geometry.pipeline->pipeline());

              for (static_object_t &model : _static_objects) {
                static_object_draw_info_t info;
                info.commandbuffer = commandbuffer;
                info.flightframe = next_frame_info.flightframe;
                info.geometry_pipeline_layout =
                    _rendering->_geometry.pipeline->layout();
                model.draw(info);
              }
              commandbuffer.endRenderPass();
            }));

    graph.add_dependency(alex::dependency_info_t{.device = _core->device(),
                                                 .parent = upload_task_id,
                                                 .child = geometry_task_id});

    graph.set_end(geometry_task_id);

    vk::Semaphore graph_finished_semaphore =
        _rendergraph_semaphores[next_frame_info.flightframe].get();

    auto graph_evaluate_info = alex::graph_evaluate_info_t{};
    graph_evaluate_info.device = _core->device();
    graph_evaluate_info.commandpool = _core->commandpool();
    graph_evaluate_info.queue = _core->queue();
    graph_evaluate_info.sync_semaphore = graph_finished_semaphore;
    graph.evaluate(graph_evaluate_info);

    alex::presentation_info_t presentation_info;
    presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.source_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(_rendering->_render_extent.width),
        static_cast<std::int32_t>(_rendering->_render_extent.height), 1};

    presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.destination_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(_presenter->window_extent.width),
        static_cast<std::int32_t>(_presenter->window_extent.height), 1};

    presentation_info.blit_filter = vk::Filter::eNearest;
    presentation_info.image =
        _rendering->_geometry.attachments.color[next_frame_info.flightframe]
            .image();
    presentation_info.layout = vk::ImageLayout::eColorAttachmentOptimal;
    presentation_info.queue = _core->queue();
    presentation_info.wait_semaphore = graph_finished_semaphore;
    _presenter->present(presentation_info);

    return scene::status_t::ok;
  }

  constexpr auto unload() -> void override {
    std::println("{} Unloaded", _name);
  }

private:
  const std::string _name{"scene"};
  alex::core_t *_core{nullptr};
  rendering_t *_rendering{nullptr};
  alex::presenter_t *_presenter{nullptr};
  sdl::window_t *_window{nullptr};

  std::optional<game::model_source_t> _chest;
  std::optional<alex::texture_t> _chest_diffuse_texture;
  vk::UniqueSampler _chest_diffuse_texture_sampler;

  alex::flightframe_array_t<alex::graph_t> _rendergraphs;
  alex::flightframe_array_t<vk::UniqueSemaphore> _rendergraph_semaphores;

  std::vector<static_object_t> _static_objects;

  std::optional<glm_transform_hierarchy> _transform_hierarchy;
  player_t _player;
};

} // namespace game
