#pragma once

#include "core.hpp"

#include <filesystem>
#include <vulkan/vulkan_enums.hpp>

namespace alex {

struct pipeline_info_t {
  pipeline_info_t(vk::Device device);
  pipeline_info_t &set_extent(vk::Extent3D extent);
  pipeline_info_t &set_polygon_mode(vk::PolygonMode mode);
  pipeline_info_t &set_cull_mode(vk::CullModeFlags cull_mode);
  pipeline_info_t &set_front_face(vk::FrontFace front_face);
  pipeline_info_t &set_renderpass(vk::RenderPass renderpass);
  pipeline_info_t &set_vertex_program_path(std::filesystem::path path);
  pipeline_info_t &set_fragment_program_path(std::filesystem::path path);
  pipeline_info_t &add_setlayout(vk::DescriptorSetLayout setlayout);
  pipeline_info_t &
  add_vertex_input_binding(vk::VertexInputBindingDescription binding);
  pipeline_info_t &
  add_vertex_input_attribute(vk::VertexInputAttributeDescription attribute);

  vk::Device device;
  vk::Extent3D extent;
  vk::RenderPass renderpass;
  vk::PolygonMode polygon_mode{vk::PolygonMode::eFill};
  vk::CullModeFlags cull_mode{vk::CullModeFlagBits::eBack};
  vk::FrontFace front_face{vk::FrontFace::eClockwise};
  std::filesystem::path vertex_program_path;
  std::filesystem::path fragment_program_path;
  std::vector<vk::DescriptorSetLayout> setlayouts;
  std::vector<vk::VertexInputBindingDescription> vertex_bindings;
  std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
};

class pipeline_t {
public:
  pipeline_t(pipeline_info_t &info);

  auto layout() -> vk::PipelineLayout;
  auto pipeline() -> vk::Pipeline;

private:
  vk::UniquePipelineLayout _layout;
  vk::UniquePipeline _pipeline;
};

} // namespace alex
