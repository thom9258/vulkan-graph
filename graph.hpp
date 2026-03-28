#pragma once

#include "memory_buffer.hpp"
#include "texture_storage.hpp"
#include "texture.hpp"
#include "vector.hpp"

#include <functional>

namespace alex::graph {

enum class resource_type_t { attachment, texture, memory_buffer, reference };

struct texture_resource_t {
  vk::Format format;
  vk::Extent3D extent;
  vk::ImageAspectFlags aspect_flags;
};

struct attachment_resource_t {
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

struct renderpass_record_info_t {
  vk::CommandBuffer commandbuffer;
  std::span<std::string_view> inputs;
  std::span<std::string_view> outputs;
};

using renderpass_recorder_t = std::function<void(renderpass_record_info_t&)>;

struct renderpass_info_t {
  std::string_view name{""};
  std::span<std::string_view> inputs;
  std::span<std::string_view> outputs;
};

struct framegraph_node_t;

struct framegraph_resource_t {
  std::string_view name{""};
  std::uint32_t reference_count{0};
  framegraph_node_t *producer{nullptr};

  resource_type_t type;
  union {
    texture_resource_t texture;
    attachment_resource_t attachment;
  };
};

struct framegraph_node_t {
  std::string_view name{""};
  vector_t<framegraph_resource_t *> inputs;
  vector_t<framegraph_resource_t *> outputs;
  vector_t<framegraph_node_t *> dependencies;
  vector_t<framegraph_node_t *> parents;
};

struct graph_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;

  memory::arena* arena{nullptr};
  texture_storage_t* texture_storage;
  std::span<renderpass_info_t *> renderpass_infos;
  std::span<resource_info_t *> resource_infos;
};

struct graph_t {
  memory::arena* m_arena{nullptr};
  texture_storage_t* m_texture_storage;

  std::span<renderpass_info_t *> m_renderpass_infos;
  std::span<resource_info_t *> m_resource_infos;

  std::span<framegraph_node_t*> m_nodes;
  std::span<framegraph_resource_t*> m_resources;

  void init(graph_info_t &info);
  void debug_print();
  void debug_graphviz();

private:
  framegraph_resource_t *find_resource(std::string_view name);
  void init_framegraph_resources();
  void init_framegraph_nodes();
  void prune_unused_resources();
  void prune_unused_nodes();
  void connect_node_dependencies();
  void connect_node_parents();
  void create_graph_textures(graph_info_t& info);
};

} // namespace alex::graph
