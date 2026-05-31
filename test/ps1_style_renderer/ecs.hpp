#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <alex/arena.hpp>
#include <alex/core.hpp>
#include <alex/ensure.hpp>
#include <alex/flightframe_array.hpp>
#include <alex/log.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/uniform_descriptorsets.hpp>

#define SIMPLE_GEOMETRY_IMPLEMENTATION
#include <simple_geometry.h>

#include "../sukoshi_ecs/sukoshi_ecs.hpp"

#include <ranges>

struct draw_info_t {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 model;
};

struct component_mesh_t {
  alex::memory_buffer_t* vertices{nullptr};
  std::uint32_t vertices_length{0};

  alex::memory_buffer_t* indices{nullptr};
  std::uint32_t indices_length{0};

  vk::UniqueDescriptorPool uniform_descriptor_pool;
  alex::flightframe_array_t<std::optional<alex::direct_memory_buffer_t>>
      direct_uniforms;

  alex::flightframe_array_t<std::optional<alex::memory_buffer_t>> uniforms;
  std::vector<vk::UniqueDescriptorSet> uniform_descriptorsets;

  vk::UniqueDescriptorPool diffuse_descriptor_pool;
  std::vector<vk::UniqueDescriptorSet> diffuse_descriptorsets;
};

struct component_transform_t {
  glm::mat4 mat;
};

static constexpr std::size_t max_entities = 100;

namespace ecs {
using manager_t = sukoshi::ecs::manager_t<
    sukoshi::ecs::component_policy_t<component_mesh_t, max_entities>,
    sukoshi::ecs::component_policy_t<component_transform_t, max_entities>>;
using entity_id_t = manager_t::entity_id_t;

using entity_pointer_t = manager_t::entity_pointer_t;

} // namespace ecs

#if 0
constexpr auto generate_cube(alex::memory::arena &arena, float r, float g,
                             float b) -> std::span<vertex_t> {
  sg_status status;
  sg_cube_info info;
  info.width = 1.0f;
  info.height = 1.0f;
  info.depth = 1.0f;

  size_t vertices_size{0};
  status = sg_cube_vertices(&info, &vertices_size, nullptr, nullptr, nullptr);
  ALEX_ERROR_IF(!sg_success(status), "Could not load vertices size")
  auto positions = arena.allocate<sg_position>(vertices_size);
  ALEX_ERROR_IF(positions.empty(), "Could not allocate positions buffer")

  status = sg_cube_vertices(&info, &vertices_size, positions.data(), nullptr,
                            nullptr);
  ALEX_ERROR_IF(!sg_success(status), "Could not load vertices")

  auto normals = arena.allocate<sg_normal>(vertices_size);
  ALEX_ERROR_IF(normals.empty(), "Could not allocate normals buffer")
  status =
      sg_cube_vertices(&info, &vertices_size, nullptr, normals.data(), nullptr);
  ALEX_ERROR_IF(!sg_success(status), "Could not load vertices")

  auto vertices = arena.allocate<vertex_t>(vertices_size);
  ALEX_ERROR_IF(vertices.empty(), "Could not allocate vertices buffer")

  for (auto [i, vertex] : vertices | std::views::enumerate) {
    vertex.position[0] = positions[i].x;
    vertex.position[1] = positions[i].y;
    vertex.position[2] = positions[i].z;
    vertex.color[0] = r;
    vertex.color[1] = g;
    vertex.color[2] = b;
  }

  return vertices;
}

struct cube_prefab_info_t {
  ecs::manager_t *manager;
  alex::core_t *core;
  vk::DescriptorSetLayout set_layout;
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
  std::span<vertex_t> cube_vertices =
      generate_cube(*info.arena, info.r, info.g, info.b);
  mesh->vertices_length = cube_vertices.size();
  alex::direct_memory_buffer_info_t direct_cube_buffer_info;
  direct_cube_buffer_info.physical_device = info.core->physical_device();
  direct_cube_buffer_info.device = info.core->device();
  direct_cube_buffer_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_cube_buffer_info.memory_size =
      sizeof(cube_vertices[0]) * cube_vertices.size();

  alex::direct_memory_buffer_t direct_cube_buffer(direct_cube_buffer_info);
  std::memcpy(direct_cube_buffer.memory_ptr(), cube_vertices.data(),
              direct_cube_buffer.memory_size());

  alex::memory_buffer_info_t cube_buffer_info;
  cube_buffer_info.physical_device = info.core->physical_device();
  cube_buffer_info.device = info.core->device();
  cube_buffer_info.buffer_type = alex::memory_buffer_type_t::vertices;
  cube_buffer_info.memory_size = direct_cube_buffer_info.memory_size;

  info.core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
    alex::memory_buffer_write_info_t cube_buffer_write_info;
    cube_buffer_write_info.physical_device = info.core->physical_device();
    cube_buffer_write_info.device = info.core->device();
    cube_buffer_write_info.direct = &direct_cube_buffer;
    cube_buffer_write_info.write_size = direct_cube_buffer.memory_size();
    cube_buffer_write_info.commandbuffer = commandbuffer;

    mesh->vertices.emplace(cube_buffer_info);
    mesh->vertices.value().record_write(cube_buffer_write_info);

    draw_info_t cube_draw_info;

    alex::direct_memory_buffer_info_t direct_uniform_info;
    direct_uniform_info.physical_device = info.core->physical_device();
    direct_uniform_info.device = info.core->device();
    direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
    direct_uniform_info.memory_size = sizeof(draw_info_t);

    for (auto &uniform : mesh->direct_uniforms) {
      uniform = alex::direct_memory_buffer_t(direct_uniform_info);
      std::memcpy(uniform->memory_ptr(), &cube_draw_info,
                  sizeof(cube_draw_info));
    };

    for (auto [i, uniform] : mesh->uniforms | std::views::enumerate) {
      alex::memory_buffer_info_t uniform_info;
      uniform_info.physical_device = info.core->physical_device();
      uniform_info.device = info.core->device();
      uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
      uniform_info.memory_size = direct_uniform_info.memory_size;
      uniform = alex::memory_buffer_t(uniform_info);

      alex::memory_buffer_write_info_t write_info;
      write_info.physical_device = info.core->physical_device();
      write_info.device = info.core->device();
      write_info.direct = &mesh->direct_uniforms[i].value();
      write_info.write_size = uniform->memory_size();
      write_info.commandbuffer = commandbuffer;
      uniform->record_write(write_info);
    }

    std::vector<vk::DescriptorPoolSize> const pool_sizes{
        vk::DescriptorPoolSize{}.setDescriptorCount(4).setType(
            vk::DescriptorType::eUniformBuffer)};

    auto pool_create_info =
        vk::DescriptorPoolCreateInfo{}
            .setPoolSizes(pool_sizes)
            .setMaxSets(4)
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

    mesh->uniform_descriptor_pool =
        info.core->device().createDescriptorPoolUnique(pool_create_info);

    alex::uniform_descriptorsets_info_t cube_descriptorsets_info;
    cube_descriptorsets_info.physical_device = info.core->physical_device();
    cube_descriptorsets_info.device = info.core->device();
    cube_descriptorsets_info.set_count = 2;
    cube_descriptorsets_info.layout = info.set_layout;
    cube_descriptorsets_info.pool = mesh->uniform_descriptor_pool.get();

    mesh->descriptorsets.emplace(cube_descriptorsets_info);
    for (auto [i, uniform] : mesh->uniforms | std::views::enumerate) {
      alex::uniform_descriptorsets_update_info_t update_info;
      update_info.device = info.core->device();
      update_info.set_index = i;
      update_info.buffer = &uniform.value();
      update_info.buffer_offset = 0;
      update_info.buffer_size = uniform->memory_size();
      mesh->descriptorsets->update(update_info);
    };
  });

  return cube;
}

#endif
