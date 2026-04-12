#pragma once

#include "graph_builder.hpp"
#include "texture.hpp"

#include <vulkan/vulkan_enums.hpp>

#include <filesystem>
#include <vector>

namespace alex::graph {

struct resource_t {
  resource_t(std::string_view name, texture_info_t texture);
  resource_t(std::string_view name, attachment_info_t attachment);

  std::string name{""};
  std::variant<texture_info_t, attachment_info_t> resource;
  std::uint32_t reference_count{0};
  vk::ImageLayout layout{vk::ImageLayout::eUndefined};
};

struct renderpass_node_t {
  std::string name{""};
  renderpass_record_callback_t record_callback;
  resource_t *depth_attachment{nullptr};
  resource_t *color_attachment{nullptr};
  std::vector<resource_t *> inputs;
  std::vector<resource_t *> outputs;
  std::vector<renderpass_node_t *> dependencies;
  std::vector<renderpass_node_t *> parents;

  vk::Extent3D extent;
#if 0
  std::filesystem::path vertex_program_path;
  std::filesystem::path fragment_program_path;
  std::vector<vk::DescriptorSetLayout> set_layouts;
  vk::RenderPass renderpass;
  flightframe_array_t<vk::Framebuffer> framebuffers;
  vk::PipelineLayout layout;
  vk::Pipeline pipeline;
  // TODO: maybe we have an span of these?
  vk::DescriptorSetLayout setlayout;
#endif
};

struct graph_t {
  graph_t(graph_info_t &info, memory::arena &arena);

  texture_storage_t m_texture_storage;
  std::vector<std::unique_ptr<renderpass_node_t>> m_nodes;
  std::vector<std::unique_ptr<resource_t>> m_resources;

  void print_execution_order(std::ostream &os);
  void print_graphviz(std::ostream &os);
  void record(alex::next_frame_info_t &next_frame);

  renderpass_node_t *find_node(std::string_view name);
  resource_t *find_resource(std::string_view name);
  flightframe_array_t<vk::ImageView>
  get_attachment_views(std::string_view name);

private:
  void init_resources(graph_info_t &info);
  void init_framepass_nodes(graph_info_t &info);
  void prune_unused_resources();
  void prune_unused_nodes();
  void connect_node_dependencies(graph_info_t &info);
  void connect_node_parents();
  void create_framepass_resources(graph_info_t &info);
  void create_framepass_renderpasses(graph_info_t &info);
  // void create_framepass_pipelines(graph_info_t &info, memory::arena &arena);
};

} // namespace alex::graph
