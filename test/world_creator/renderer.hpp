#pragma once

#include "alex/geometrypass_builder.hpp"
#include "alex/overlaypass_builder.hpp"
#include "ui/entity_hierarchy.hpp"
#include "ui/game_manager.hpp"
#include "ui/imgui_context.hpp"
#include "ui/level_settings.hpp"
#include "world.hpp"
#include "world_creator/ui/imgui_context.hpp"

#include <alex/task_graph.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace game {

struct renderer_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::Extent2D render_extent;
  vk::Extent2D ui_extent;
  vk::Extent2D display_extent;
};

struct renderer_draw_frame_info_t {
  vk::Device device;
  vk::CommandPool commandpool;
  vk::Queue queue;

  uint32_t flightframe;
  world_t* world;
  imgui_context_t* imgui_context;
  ui::game_manager_t* game_manager;
  ui::entity_hierarchy_t* entity_hierarchy;
  ui::level_settings_t* level_settings;
};

struct draw_frame_result_t
{
    vk::Semaphore semaphore;
    vk::Image image;
    vk::Extent2D extent;
};

class renderer_t {
public:
  struct uniform_mvp_t {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 model;
  };

  struct uniform_lights_t {
    alignas(sizeof(glm::vec4)) glm::vec3 direction;
    alignas(sizeof(glm::vec4)) glm::vec3 color;
    alignas(sizeof(glm::vec4)) float intensity;
  };

  renderer_t(renderer_info_t &info);

  auto draw_frame(renderer_draw_frame_info_t& info) -> draw_frame_result_t;

  auto debugui_renderpass() -> vk::RenderPass;

  auto display_extent() -> vk::Extent2D;

  auto final_image() -> vk::Image;

private:
  auto construct_geometry_renderer() -> void;
  auto construct_debugui_renderer() -> void;
  auto construct_frame() -> void;

  vk::PhysicalDevice _physical_device;
  vk::Device _device;
  vk::Extent2D _render_extent;
  vk::Extent2D _display_extent;

  struct {
    std::vector<alex::texture_t> color_attachments;
    std::vector<alex::texture_t> depth_attachments;
    vk::UniqueDescriptorSetLayout diffuse_setlayout;
    vk::UniqueDescriptorSetLayout frame_uniform_setlayout;
    vk::UniqueDescriptorSetLayout light_uniform_setlayout;
    vk::UniqueDescriptorSet frame_uniform_set;
    vk::UniqueDescriptorSet light_uniform_set;
    std::optional<alex::geometrypass_t> renderpass;
    std::optional<alex::pipeline_t> pipeline;
  } _geometry;

  struct {
    std::vector<alex::texture_t> color_attachments;
    std::optional<alex::overlaypass_t> renderpass;
  } _debugui;

  struct {
    alex::flightframe_array_t<alex::graph_t> rendergraphs;
    alex::flightframe_array_t<vk::UniqueSemaphore> rendergraph_semaphores;
  } _frame;
};

} // namespace game
