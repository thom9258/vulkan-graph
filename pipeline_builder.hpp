#pragma once

#include "core.hpp"
#include "arena.hpp"

#include <filesystem>

namespace alex::graph {

struct pipeline_info_t {
  pipeline_info_t(vk::Device device);
  pipeline_info_t& set_extent(vk::Extent3D extent);
  pipeline_info_t& set_renderpass(vk::RenderPass renderpass);
  pipeline_info_t& set_vertex_program_path(std::filesystem::path path);
  pipeline_info_t& set_fragment_program_path(std::filesystem::path path);
  pipeline_info_t& add_setlayout(vk::DescriptorSetLayout setlayout);

  vk::Device device;
  vk::Extent3D extent;
  vk::RenderPass renderpass;
  std::filesystem::path vertex_program_path;
  std::filesystem::path fragment_program_path;
  std::vector<vk::DescriptorSetLayout> setlayouts;
};

struct pipeline_t {
  pipeline_t(pipeline_info_t& info, memory::arena &arena);

  vk::PipelineLayout layout;
  vk::Pipeline pipeline;
};

}
