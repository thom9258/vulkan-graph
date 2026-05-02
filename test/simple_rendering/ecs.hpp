#pragma once

#include <alex/memory_buffer.hpp>
#include <alex/uniform_descriptorsets.hpp>

#include "glm.hpp"

#include "../sukoshi_ecs/sukoshi_ecs.hpp"

struct component_mesh_t {
  alex::memory_buffer_t vertices;
  std::uint32_t vertices_length;
  alex::flightframe_array_t<alex::direct_memory_buffer_t> direct_uniforms;
  alex::flightframe_array_t<alex::memory_buffer_t> uniforms;
  alex::uniform_descriptorsets_t descriptorsets;
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
