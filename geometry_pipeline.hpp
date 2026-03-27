#pragma once

#include "core.hpp"
#include "renderpass.hpp"

#include <filesystem>

namespace alex {

// struct mat4 {
//	float data[16];
// };
//
// struct render_info_t {
//	mat4 view;
//	mat4 proj;
//	mat4 model;
// };

struct geometry_pipeline_info_t {
  core_t *core{nullptr};
  renderpass_t* renderpass{nullptr};
  vk::Extent2D extent;
  std::filesystem::path vertex_program_path;
  std::filesystem::path fragment_program_path;
};

struct geometry_pipeline_t {
  vk::Extent2D extent;
  vk::PipelineLayout layout;
  vk::Pipeline pipeline;
  vk::DescriptorSetLayout setlayout;

  void init(geometry_pipeline_info_t &info, memory::arena &allocator);
};

} // namespace alex
