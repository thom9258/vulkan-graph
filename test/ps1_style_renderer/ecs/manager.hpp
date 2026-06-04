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

#include "../../utility/button.hpp"
#include "../../utility/sdl.hpp"
#include "../../utility/orbit_camera.hpp"

#include "../../sukoshi_ecs/sukoshi_ecs.hpp"

struct draw_info_t {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 model;
};

struct component_drawable_t {
  struct mesh_t {
    alex::memory_buffer_t *vertices{nullptr};
    std::uint32_t vertices_length{0};

    alex::memory_buffer_t *indices{nullptr};
    std::uint32_t indices_length{0};

    vk::UniqueDescriptorPool uniform_descriptor_pool;
    alex::flightframe_array_t<std::optional<alex::direct_memory_buffer_t>>
        direct_uniforms;

    alex::flightframe_array_t<std::optional<alex::memory_buffer_t>> uniforms;
    std::vector<vk::UniqueDescriptorSet> uniform_descriptorsets;

    vk::UniqueDescriptorPool diffuse_descriptor_pool;
    std::vector<vk::UniqueDescriptorSet> diffuse_descriptorsets;

    glm::mat4 transform;
    std::vector<mesh_t> children;
  };

  //mesh_t root;

    alex::memory_buffer_t *vertices{nullptr};
    std::uint32_t vertices_length{0};

    alex::memory_buffer_t *indices{nullptr};
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

struct component_orbit_camera_t {
    button_t w;
    button_t a;
    button_t s;
    button_t d;
    button_t e;
    button_t q;

	OrbitCamera camera;
	glm::mat4 projection;
	bool active = true;
    double rotatespeed = 5.0f;
    double zoomspeed = 8.0f;
};

static constexpr std::size_t max_entities = 100;
static constexpr std::size_t drawable_components_max = 100;
static constexpr std::size_t transform_components_max = 10;
static constexpr std::size_t orbit_camera_components_max = 10;

namespace ecs {
using manager_t = sukoshi::ecs::manager_t<
    sukoshi::ecs::component_policy_t<component_drawable_t, max_entities>,
    sukoshi::ecs::component_policy_t<component_transform_t, max_entities>,
    sukoshi::ecs::component_policy_t<component_orbit_camera_t, orbit_camera_components_max>>;
using entity_id_t = manager_t::entity_id_t;

using entity_pointer_t = manager_t::entity_pointer_t;

} // namespace ecs
