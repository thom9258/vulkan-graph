#pragma once

#include "graph_builder.hpp"
#include "renderpass_builder.hpp"
#include "texture.hpp"

#include <vulkan/vulkan_enums.hpp>

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
  resource_t *depth_attachment{nullptr};
  resource_t *color_attachment{nullptr};
  std::vector<resource_t *> inputs;
  std::vector<resource_t *> outputs;
  std::vector<renderpass_node_t *> dependencies;
  std::vector<renderpass_node_t *> parents;

  vk::Extent3D extent;
  geometrypass_t geometry_pass;
};

namespace command {

struct draw_t {
  std::uint32_t instance_count{1};
  std::uint32_t first_instance{0};
  std::uint32_t vertex_count{0};
  std::uint32_t first_vertex{0};
};

struct bind_pipeline_t {
  vk::Pipeline pipeline;
};

struct bind_vertexbuffer_t {
  std::uint32_t first_binding{0};
  std::vector<vk::DeviceSize> binding_offsets;
  std::uint32_t first_buffer{0};
  std::vector<vk::Buffer> buffers;
};

struct bind_indexbuffer_t {
  vk::Buffer buffer;
  vk::DeviceSize offset;
  vk::IndexType type;
};

struct bind_descriptorsets_t {
  vk::PipelineLayout layout;
  std::vector<vk::DescriptorSet> sets;
  std::uint32_t first_set{0};
};
} // namespace command

using renderpass_command_t =
    std::variant<command::draw_t, command::bind_pipeline_t,
                 command::bind_vertexbuffer_t, command::bind_indexbuffer_t,
                 command::bind_descriptorsets_t>;

struct renderpass_commands_t {
  std::string renderpass_name;
  std::vector<renderpass_command_t> commands;
};

struct record_info_t {
  vk::CommandBuffer commandbuffer;
  std::uint32_t flightframe;
  std::vector<renderpass_commands_t> renderpass_commands;
};

struct graph_t {
  graph_t(graph_info_t &info);

  texture_storage_t m_texture_storage;
  std::vector<std::unique_ptr<renderpass_node_t>> m_nodes;
  std::vector<std::unique_ptr<resource_t>> m_resources;

  void print_execution_order(std::ostream &os);
  void print_graphviz(std::ostream &os);
  void record(record_info_t &info);

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
};

} // namespace alex::graph
