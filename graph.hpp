#pragma once

#include "memory_buffer.hpp"
#include "texture.hpp"
#include "vector.hpp"

namespace alex::graph {

enum class resource_type_t { attachment, texture, memory_buffer, reference };

struct texture_resource_t {
  vk::Format format;
  vk::Extent3D extent;
};

struct attachment_resource_t {
  vk::Format format;
  vk::Extent3D extent;
};

struct resource_info_t {
  std::string_view name{""};
  resource_type_t type;
  union {
    texture_resource_t texture;
    attachment_resource_t attachment;
  };
};

struct renderpass_info_t {
  std::string_view name{""};
  std::span<std::string_view> inputs;
  std::span<std::string_view> outputs;
};

struct framegraph_node_t;

struct framegraph_resource_t {
  std::string_view name{""};
  resource_type_t type;
  std::uint32_t reference_count{0};
  framegraph_node_t *producer{nullptr};
};

struct framegraph_node_t {
  std::string_view name{""};
  vector_t<framegraph_resource_t *> inputs;
  vector_t<framegraph_resource_t *> outputs;
  vector_t<framegraph_node_t *> edges;
};

struct graph_info_t {
  memory::arena* arena{nullptr};
  std::span<renderpass_info_t *> renderpass_infos;
  std::span<resource_info_t *> resource_infos;
};

struct graph_t {
  memory::arena* m_arena{nullptr};
  std::span<renderpass_info_t *> m_renderpass_infos;
  std::span<resource_info_t *> m_resource_infos;

  std::span<framegraph_node_t*> m_nodes;
  std::span<framegraph_resource_t*> m_resources;

  void init(graph_info_t &info);
  void debug_print();

private:
  framegraph_resource_t *find_resource(std::string_view name);
  void init_framegraph_resources();
  void init_framegraph_nodes();
  void prune_unused_resources();
  void prune_unused_nodes();
  void connect_node_edges();
};

} // namespace alex::graph
