#pragma once

#include "texture_storage.hpp"
#include "texture.hpp"
#include "vector.hpp"
#include "presentation_context.hpp"

#include <vulkan/vulkan_enums.hpp>

namespace alex::graph {

enum class resource_type_t { attachment, texture, memory_buffer, reference };

struct texture_resource_t {
  vk::Format format;
  vk::Extent3D extent;
  vk::ImageAspectFlags aspect_flags;
};

enum class attachment_type_t { color, depth};

struct attachment_resource_t {
  attachment_type_t type;
  std::uint32_t index;
  vk::Format format;
  vk::Extent3D extent;
  vk::ImageAspectFlags aspect_flags;
};

struct resource_info_t {
  std::string_view name{""};
  resource_type_t type;
  union {
    texture_resource_t texture;
    attachment_resource_t attachment;
  };
};

struct framepass_info_t {
  std::string_view name{""};
  std::span<std::string_view> inputs;
  std::span<std::string_view> outputs;

  std::array<vk::ClearValue, 2> clearvalues;
  vk::AttachmentLoadOp load_op;
  vk::Extent3D extent;

  std::string_view vertex_program_path;
  std::string_view fragment_program_path;
  std::span<vk::DescriptorSetLayout> set_layouts;
};

struct framepass_node_t;

struct framepass_resource_t {
  std::string_view name{""};
  std::uint32_t reference_count{0};
  framepass_node_t *producer{nullptr};

  resource_type_t type;
  union {
    texture_resource_t texture;
    attachment_resource_t attachment;
  };
};

struct framepass_node_t {
  std::string_view name{""};
  vector_t<framepass_resource_t *> inputs;
  vector_t<framepass_resource_t *> outputs;
  vector_t<framepass_node_t *> dependencies;
  vector_t<framepass_node_t *> parents;

  vk::Extent3D extent;
  std::string_view vertex_program_path;
  std::string_view fragment_program_path;
  std::span<vk::DescriptorSetLayout> set_layouts;

  vk::RenderPass renderpass;
  flightframe_array_t<vk::Framebuffer> framebuffers;
  vk::PipelineLayout layout;
  vk::Pipeline pipeline;
  //TODO: maybe we have an span of these?
  vk::DescriptorSetLayout setlayout;
};

struct graph_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;

  memory::arena* arena{nullptr};
  texture_storage_t* texture_storage;
  std::span<framepass_info_t *> framepass_infos;
  std::span<resource_info_t *> resource_infos;
};

struct graph_t {
  memory::arena* m_arena{nullptr};
  texture_storage_t* m_texture_storage;

  std::span<framepass_info_t *> m_framepass_infos;
  std::span<resource_info_t *> m_resource_infos;

  std::span<framepass_node_t*> m_nodes;
  std::span<framepass_resource_t*> m_resources;

  void init(graph_info_t &info);
  void debug_print();
  void debug_graphviz();
  void record(alex::next_frame_info_t& next_frame);

	framepass_node_t *find_node(std::string_view name);

private:
  framepass_resource_t *find_resource(std::string_view name);
  void init_framepass_resources();
  void init_framepass_nodes();
  void prune_unused_resources();
  void prune_unused_nodes();
  void connect_node_dependencies();
  void connect_node_parents();
  void create_framepass_resources(graph_info_t& info);
  void create_framepass_renderpasses(graph_info_t& info);
  void create_framepass_pipelines(graph_info_t& info);
};

} // namespace alex::graph
