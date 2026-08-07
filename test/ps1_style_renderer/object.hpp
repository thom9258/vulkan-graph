#pragma once

#include "include_glm.hpp"
#include "glm_transform_hierarchy.hpp"
#include "rendering.hpp"

namespace game {

struct static_object_update_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::CommandBuffer commandbuffer;
  std::uint32_t flightframe;

  glm::mat4 camera_view;
  glm::mat4 camera_projection;

  glm_transform_hierarchy *transform_hierarchy{nullptr};
};

struct object_draw_info_t {
  vk::CommandBuffer commandbuffer;
  vk::PipelineLayout geometry_pipeline_layout;
  std::uint32_t flightframe;
};

struct object_t {
  std::string name{"<unnamed-object>"};

  transform_hierarchy::transform_id_t transform_id{
      transform_hierarchy::invalid_transform_id};

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

  constexpr auto update(static_object_update_info_t &info) -> void;
  constexpr auto draw(object_draw_info_t &info) -> void;
};

constexpr auto object_t::update(static_object_update_info_t &info) -> void {
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

constexpr auto object_t::draw(object_draw_info_t &info) -> void {

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
