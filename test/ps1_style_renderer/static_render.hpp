#pragma once

#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/overlaypass_builder.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture.hpp>

#include "include_glm.hpp"
#include "mesh.hpp"

#include <ranges>

namespace game {

//TODO: make this resizeable so it re-allocated textures

class static_render_t {
public:
  struct frame_uniform_t {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 model;
  };

  static_render_t(alex::core_t &core, vk::Extent3D extent);
  auto extent() const -> vk::Extent3D;
  auto renderpass() -> alex::geometrypass_t&;
  auto pipeline() -> alex::pipeline_t&;
  auto color_attachments() -> std::span<alex::texture_t>;
  auto depth_attachments() -> std::span<alex::texture_t>;
  auto diffuse_setlayout() -> vk::DescriptorSetLayout;
  auto frame_uniform_setlayout() -> vk::DescriptorSetLayout;

private:
  vk::Extent3D _extent;

  std::vector<alex::texture_t> _color_attachments;
  std::vector<alex::texture_t> _depth_attachments;

  vk::UniqueDescriptorSetLayout _diffuse_setlayout;
  vk::UniqueDescriptorSetLayout _frame_uniform_setlayout;

  std::optional<alex::geometrypass_t> _renderpass;
  std::optional<alex::pipeline_t> _pipeline;
};

} // namespace game
