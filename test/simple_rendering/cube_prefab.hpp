#pragma once

#include <alex/arena.hpp>
#include <alex/core.hpp>
#include <alex/drawing.hpp>
#include <alex/ensure.hpp>

#include "draw_info_uniform.hpp"
#include "ecs.hpp"
#include "geometry_primitives.hpp"

struct cube_prefab_info_t {
  ecs::manager_t *manager;
  alex::core_t *core;
  vk::DescriptorSetLayout set_layout;
  vk::CommandBuffer commandbuffer;
  alex::memory::arena *arena;
  float r;
  float g;
  float b;
  glm::mat4 transform;
};

ecs::entity_id_t add_cube_prefab(cube_prefab_info_t &info) {
  ecs::entity_id_t cube = info.manager->new_entity();
  auto *transform = info.manager->add_component<component_transform_t>(cube);
  transform->mat = info.transform;

  auto *mesh = info.manager->add_component<component_mesh_t>(cube);
  /* ****************************************
   * Vertex Buffer Setup
   */
  std::span<alex::vertex_t> cube_vertices =
      load_cube(*info.arena, info.r, info.g, info.b);
  mesh->vertices_length = cube_vertices.size();

  alex::direct_memory_buffer_info_t direct_cube_buffer_info;
  direct_cube_buffer_info.physical_device = info.core->physical_device;
  direct_cube_buffer_info.device = info.core->device;
  direct_cube_buffer_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_cube_buffer_info.memory_size =
      sizeof(cube_vertices[0]) * cube_vertices.size();

  alex::direct_memory_buffer_t direct_cube_buffer;
  direct_cube_buffer.init(direct_cube_buffer_info);
  std::memcpy(direct_cube_buffer.memory_ptr, cube_vertices.data(),
              direct_cube_buffer.memory_size);
  LOG_INFO("Created direct vertex buffer");

  alex::memory_buffer_info_t cube_buffer_info;
  cube_buffer_info.physical_device = info.core->physical_device;
  cube_buffer_info.device = info.core->device;
  cube_buffer_info.buffer_type = alex::memory_buffer_type_t::vertices;
  cube_buffer_info.memory_size = direct_cube_buffer_info.memory_size;

  alex::memory_buffer_write_info_t cube_buffer_write_info;
  cube_buffer_write_info.physical_device = info.core->physical_device;
  cube_buffer_write_info.device = info.core->device;
  cube_buffer_write_info.memory = &direct_cube_buffer;
  cube_buffer_write_info.write_size = direct_cube_buffer.memory_size;
  cube_buffer_write_info.commandbuffer = info.commandbuffer;

  mesh->vertices.init(cube_buffer_info);
  mesh->vertices.record_write(cube_buffer_write_info);
  LOG_INFO("Created cube vertex buffer");

  draw_info_t cube_draw_info;

  alex::direct_memory_buffer_info_t direct_uniform_info;
  direct_uniform_info.physical_device = info.core->physical_device;
  direct_uniform_info.device = info.core->device;
  direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_uniform_info.memory_size = sizeof(draw_info_t);

  for (alex::direct_memory_buffer_t &uniform : mesh->direct_uniforms) {
    uniform.init(direct_uniform_info);
    std::memcpy(uniform.memory_ptr, &cube_draw_info, sizeof(cube_draw_info));
  };

  for (auto [i, uniform] : mesh->uniforms | std::views::enumerate) {
    alex::memory_buffer_info_t uniform_info;
    uniform_info.physical_device = info.core->physical_device;
    uniform_info.device = info.core->device;
    uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
    uniform_info.memory_size = direct_uniform_info.memory_size;
    uniform.init(uniform_info);

    alex::memory_buffer_write_info_t write_info;
    write_info.physical_device = info.core->physical_device;
    write_info.device = info.core->device;
    write_info.memory = &mesh->direct_uniforms[i];
    write_info.write_size = uniform.memory_size;
    write_info.commandbuffer = info.commandbuffer;
    uniform.record_write(write_info);
  }

  std::vector<vk::DescriptorPoolSize> const pool_sizes{
      vk::DescriptorPoolSize{}.setDescriptorCount(4).setType(
          vk::DescriptorType::eUniformBuffer)};

  auto pool_create_info =
      vk::DescriptorPoolCreateInfo{}.setPoolSizes(pool_sizes).setMaxSets(4);

  vk::DescriptorPool descriptor_pool =
      info.core->device.createDescriptorPool(pool_create_info);

  alex::uniform_descriptorsets_info_t cube_descriptorsets_info;
  cube_descriptorsets_info.physical_device = info.core->physical_device;
  cube_descriptorsets_info.device = info.core->device;
  cube_descriptorsets_info.set_count = 2;
  cube_descriptorsets_info.layout = info.set_layout;
  cube_descriptorsets_info.pool = descriptor_pool;

  mesh->descriptorsets.init(cube_descriptorsets_info);
  for (auto [i, uniform] : mesh->uniforms | std::views::enumerate) {
    alex::uniform_descriptorsets_update_info_t update_info;
    update_info.device = info.core->device;
    update_info.set_index = i;
    update_info.buffer = &uniform;
    update_info.buffer_offset = 0;
    update_info.buffer_size = uniform.memory_size;
    mesh->descriptorsets.update(update_info);
  };

  return cube;
}
