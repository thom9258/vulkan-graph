#pragma once

#include "../utility/button.hpp"
#include "../utility/camera.hpp"
#include "../utility/sdl.hpp"

#include "glm_transform_hierarchy.hpp"
#include "include_glm.hpp"
#include "rendering.hpp"
#include "utility/transform_hierarchy.hpp"

namespace game {

struct player_draw_resource_update_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::CommandBuffer commandbuffer;
  std::uint32_t flightframe;

  glm::mat4 camera_view;
  glm::mat4 camera_projection;

  glm_transform_hierarchy *transform_hierarchy{nullptr};
};

struct player_draw_info_t {
  vk::CommandBuffer commandbuffer;
  vk::PipelineLayout geometry_pipeline_layout;
  std::uint32_t flightframe;
};

struct player_t {

  constexpr player_t(camera_t *camera);
  constexpr auto update_input(std::span<SDL_Event> events) -> void;
  constexpr auto update_logic(double deltatime) -> void;

  constexpr auto draw_resource_update(player_draw_resource_update_info_t &info)
      -> void;

  constexpr auto draw(player_draw_info_t &info) -> void;

private:
  struct {
    button_t w;
    button_t a;
    button_t s;
    button_t d;
  } _button;

  camera_t *_camera{nullptr};
  float move_speed{3.0f};

  alex::memory_buffer_t *vertices{nullptr};
  std::uint32_t vertices_length{0};
  alex::memory_buffer_t *indices{nullptr};
  std::uint32_t indices_length{0};

  vk::UniqueDescriptorPool uniform_descriptor_pool;
  std::vector<alex::direct_memory_buffer_t> direct_uniforms;

  std::vector<alex::memory_buffer_t> uniforms;
  std::vector<vk::UniqueDescriptorSet> uniform_descriptorsets;

  vk::UniqueDescriptorPool diffuse_descriptor_pool;
  std::vector<vk::UniqueDescriptorSet> diffuse_descriptorsets;

  transform_hierarchy::transform_id_t transform_id{
      transform_hierarchy::invalid_transform_id};
};

constexpr player_t::player_t(camera_t *camera) : _camera{camera} {}

constexpr auto player_t::update_logic(double deltatime) -> void {
  auto move_direction = std::optional<glm::vec3>{};
  if (_button.w.is_pressed()) {
    move_direction =
        move_direction.value_or(glm::vec3(0.0f)) + glm::vec3(-1.0f, 0.0f, 0.0f);
  }
  if (_button.s.is_pressed()) {
    move_direction =
        move_direction.value_or(glm::vec3(0.0f)) + glm::vec3(1.0f, 0.0f, 0.0f);
  }
  if (_button.a.is_pressed()) {
    move_direction =
        move_direction.value_or(glm::vec3(0.0f)) + glm::vec3(0.0f, 0.0f, 1.0f);
  }
  if (_button.d.is_pressed()) {
    move_direction =
        move_direction.value_or(glm::vec3(0.0f)) + glm::vec3(0.0f, 0.0f, -1.0f);
  }

  if (move_direction.has_value()) {
    const auto translation =
        glm::normalize(*move_direction) * move_speed * static_cast<float>(deltatime);
    _camera->translate_global(translation);
  }
}

constexpr auto player_t::update_input(std::span<SDL_Event> events) -> void {
  for (SDL_Event event : events) {
    switch (event.type) {
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
      case SDLK_w:
        _button.w.release();
        break;
      case SDLK_s:
        _button.s.release();
        break;
      case SDLK_a:
        _button.a.release();
        break;
      case SDLK_d:
        _button.d.release();
        break;
      }
      break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_w:
        _button.w.press();
        break;
      case SDLK_s:
        _button.s.press();
        break;
      case SDLK_a:
        _button.a.press();
        break;
      case SDLK_d:
        _button.d.press();
        break;
      }
    }
  }
}

constexpr auto
player_t::draw_resource_update(player_draw_resource_update_info_t &info)
    -> void {
  auto model_matrix = info.transform_hierarchy->global_location(transform_id);

  draw_info_t draw_info;
  draw_info.view = info.camera_view;
  draw_info.projection = info.camera_projection;
  draw_info.model = model_matrix.value();
  std::memcpy(direct_uniforms[info.flightframe].memory_ptr(), &draw_info,
              sizeof(draw_info));

  alex::memory_buffer_write_info_t write_info;
  write_info.physical_device = info.physical_device;
  write_info.device = info.device;
  write_info.direct = &direct_uniforms[info.flightframe];
  write_info.write_size = uniforms[info.flightframe].memory_size();
  write_info.commandbuffer = info.commandbuffer;
  uniforms[info.flightframe].record_write(write_info);
}

constexpr auto player_t::draw(player_draw_info_t &info) -> void {

  vk::DescriptorSet uniform = uniform_descriptorsets[info.flightframe].get();
  info.commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                        info.geometry_pipeline_layout, 0, 1,
                                        &uniform, 0, nullptr);

  vk::DescriptorSet diffuse = diffuse_descriptorsets[info.flightframe].get();
  info.commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                        info.geometry_pipeline_layout, 1, 1,
                                        &diffuse, 0, nullptr);

  std::vector<vk::DeviceSize> offsets = {0};
  std::vector<vk::Buffer> buffers = {vertices->buffer()};
  info.commandbuffer.bindVertexBuffers(0, 1, buffers.data(), offsets.data());

  vk::DeviceSize offset = 0;
  vk::Buffer buffer = indices->buffer();
  info.commandbuffer.bindIndexBuffer(buffer, offset, vk::IndexType::eUint32);

  info.commandbuffer.drawIndexed(indices_length, 1, 0, 0, 0);
}

} // namespace game
