#pragma once

#include "arena.hpp"
#include "core.hpp"

#include <filesystem>

namespace alex {

struct pipeline_info_t {
  pipeline_info_t(vk::Device device);
  pipeline_info_t &set_extent(vk::Extent3D extent);
  pipeline_info_t &set_renderpass(vk::RenderPass renderpass);
  pipeline_info_t &set_vertex_program_path(std::filesystem::path path);
  pipeline_info_t &set_fragment_program_path(std::filesystem::path path);
  pipeline_info_t &add_setlayout(vk::DescriptorSetLayout setlayout);

  vk::Device device;
  vk::Extent3D extent;
  vk::RenderPass renderpass;
  vk::PolygonMode polygon_mode{vk::PolygonMode::eFill};
  vk::CullModeFlags cull_mode{vk::CullModeFlagBits::eBack};
  vk::FrontFace front_face{vk::FrontFace::eClockwise};
  std::filesystem::path vertex_program_path;
  std::filesystem::path fragment_program_path;
  std::vector<vk::DescriptorSetLayout> setlayouts;
};

class pipeline_t {
public:
  pipeline_t(pipeline_info_t &info, memory::arena &arena);

  auto layout() -> vk::PipelineLayout;
  auto pipeline() -> vk::Pipeline;

private:
  vk::UniquePipelineLayout _layout;
  vk::UniquePipeline _pipeline;
};

} // namespace alex
