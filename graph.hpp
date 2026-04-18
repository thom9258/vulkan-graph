#pragma once

#include "geometrypass_builder.hpp"
#include "graph_builder.hpp"
#include "memory_buffer.hpp"
#include "texture.hpp"

#include <vulkan/vulkan_enums.hpp>

#include <vector>

namespace alex::graph {

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

struct set_viewport_t {
  float x;
  float y;
  float w;
  float h;
  struct {
    float min{0.0f};
    float max{1.0f};
  } depth;
};

struct set_scissor_t {
  vk::Offset2D offset;
  vk::Extent2D extent;
};

struct buffer_upload_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  direct_memory_buffer_t *direct_buffer;
  memory_buffer_t *buffer;
};

} // namespace command

using renderpass_command_t =
    std::variant<command::draw_t, command::bind_pipeline_t,
                 command::bind_vertexbuffer_t, command::bind_indexbuffer_t,
                 command::bind_descriptorsets_t, command::set_viewport_t,
                 command::set_scissor_t>;

using uploadpass_command_t = std::variant<command::buffer_upload_t>;

struct renderpass_commands_t {
  std::string renderpass_name;
  std::vector<renderpass_command_t> commands;
};

struct uploadpass_commands_t {
  std::string renderpass_name;
  std::vector<uploadpass_command_t> commands;
};

struct record_info_t {
  vk::CommandBuffer commandbuffer;
  std::uint32_t flightframe;
  std::vector<renderpass_commands_t> renderpass_commands;
  std::vector<uploadpass_commands_t> uploadpass_commands;
};

struct resource_t {
  resource_t(std::string_view name, texture_info_t texture);
  resource_t(std::string_view name, attachment_info_t attachment);

  std::string name{""};
  std::variant<texture_info_t, attachment_info_t> resource;
  std::uint32_t reference_count{0};
  vk::ImageLayout layout{vk::ImageLayout::eUndefined};
};

struct renderpass_node_t;
struct uploadpass_node_t;
using node_t = std::variant<renderpass_node_t, uploadpass_node_t>;

std::string_view get_name(node_t &node);
std::span<node_t *> get_dependencies(node_t &node);

void add_dependency(node_t &node, node_t *dependency);
void add_parent(node_t &node, node_t *parent);

struct renderpass_node_t {
  std::string name{""};
  resource_t *depth_attachment{nullptr};
  resource_t *color_attachment{nullptr};
  std::vector<resource_t *> inputs;
  std::vector<resource_t *> outputs;
  std::vector<node_t *> dependencies;
  std::vector<node_t *> parents;

  vk::Extent3D extent;
  geometrypass_t geometry_pass;

  void record(std::span<renderpass_command_t> commands,
              vk::CommandBuffer commandbuffer, std::uint32_t flightframe);
};

struct uploadpass_node_t {
  std::string name{""};
  std::vector<node_t *> dependencies;
  std::vector<node_t *> parents;

  void record(std::span<uploadpass_command_t> commands,
              vk::CommandBuffer commandbuffer);
};

struct graph_t {
  graph_t(graph_info_t &info);

  texture_storage_t m_texture_storage;
  std::vector<std::unique_ptr<node_t>> m_nodes;
  std::vector<std::unique_ptr<resource_t>> m_resources;

  void print_execution_order(std::ostream &os);
  void print_graphviz(std::ostream &os);
  void record(record_info_t &info);

  node_t *find_node(std::string_view name);
  resource_t *find_resource(std::string_view name);
  flightframe_array_t<vk::ImageView>
  get_attachment_views(std::string_view name);

private:
  void init_resources(graph_info_t &info);
  void init_nodes(graph_info_t &info);
  void prune_unused_resources();
  void prune_unused_nodes();
  void connect_node_dependencies(graph_info_t &info);
  void connect_node_parents();
  void sort_nodes();
  void create_framepass_resources(graph_info_t &info);
  void create_framepass_renderpasses(graph_info_t &info);
};

} // namespace alex::graph
