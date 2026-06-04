#pragma once

#include "manager.hpp"

#include <ranges>

namespace ecs::system {

struct models_upload_info_t {
  manager_t *manager;
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::CommandBuffer commandbuffer;
  std::uint32_t flightframe;
};

constexpr auto models_upload(models_upload_info_t &info) -> void {

  auto *orbit_camera = std::invoke([&]() {
    std::array<entity_id_t, orbit_camera_components_max> entities;
    std::size_t entity_count = info.manager->get_entities(entities);
    entity_count = info.manager->filter_inplace<component_orbit_camera_t>(
        std::span(entities).subspan(0, entity_count));

    auto is_active = [&](ecs::entity_id_t id) {
      auto *orbit_camera =
          info.manager->get_component<component_orbit_camera_t>(id);
      return orbit_camera->active;
    };

    auto active_cameras = entities | std::views::take(entity_count) |
                          std::views::filter(is_active) |
                          std::ranges::to<std::vector>();

    return info.manager->get_component<component_orbit_camera_t>(
        active_cameras[0]);
  });

  std::array<entity_id_t, max_entities> entities;
  std::size_t entity_count = info.manager->get_entities(entities);
  entity_count = info.manager->filter_inplace<component_drawable_t>(
      std::span(entities).subspan(0, entity_count));
  entity_count = info.manager->filter_inplace<component_transform_t>(
      std::span(entities).subspan(0, entity_count));

  for (ecs::entity_id_t entity : entities | std::views::take(entity_count)) {
    auto *mesh = info.manager->get_component<component_drawable_t>(entity);
    auto *transform =
        info.manager->get_component<component_transform_t>(entity);

    draw_info_t draw_info;
    draw_info.view = orbit_camera->camera.view();
    draw_info.projection = orbit_camera->projection;
    draw_info.model = transform->mat;
    std::memcpy(mesh->direct_uniforms[info.flightframe]->memory_ptr(),
                &draw_info, sizeof(draw_info));

    alex::memory_buffer_write_info_t write_info;
    write_info.physical_device = info.physical_device;
    write_info.device = info.device;
    write_info.direct = &mesh->direct_uniforms[info.flightframe].value();
    write_info.write_size = mesh->uniforms[info.flightframe]->memory_size();
    write_info.commandbuffer = info.commandbuffer;
    mesh->uniforms[info.flightframe]->record_write(write_info);
  }
}

struct models_draw_info_t {
  manager_t *manager;
  vk::CommandBuffer commandbuffer;
  vk::PipelineLayout geometry_pipeline_layout;
  std::uint32_t flightframe;
};

constexpr auto models_draw(models_draw_info_t &info) -> void {
  std::array<entity_id_t, max_entities> entities;
  std::size_t entity_count = info.manager->get_entities(entities);
  entity_count = info.manager->filter_inplace<component_drawable_t>(
      std::span(entities).subspan(0, entity_count));
  entity_count = info.manager->filter_inplace<component_transform_t>(
      std::span(entities).subspan(0, entity_count));

  for (ecs::entity_id_t entity : entities | std::views::take(entity_count)) {
    auto *drawable = info.manager->get_component<component_drawable_t>(entity);

    {
      vk::DescriptorSet uniform =
          drawable->uniform_descriptorsets[info.flightframe].get();

      info.commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            info.geometry_pipeline_layout, 0, 1,
                                            &uniform, 0, nullptr);
    }
    {
      vk::DescriptorSet diffuse =
          drawable->diffuse_descriptorsets[info.flightframe].get();

      info.commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            info.geometry_pipeline_layout, 1, 1,
                                            &diffuse, 0, nullptr);
    }

    {
      std::vector<vk::DeviceSize> offsets = {0};
      std::vector<vk::Buffer> buffers = {drawable->vertices->buffer()};
      info.commandbuffer.bindVertexBuffers(0, 1, buffers.data(),
                                           offsets.data());
    }

    {
      vk::DeviceSize offset = 0;
      vk::Buffer buffer = drawable->indices->buffer();
      info.commandbuffer.bindIndexBuffer(buffer, offset,
                                         vk::IndexType::eUint32);
    }

    info.commandbuffer.drawIndexed(drawable->indices_length, 1, 0, 0, 0);
  }
};

} // namespace ecs::system
