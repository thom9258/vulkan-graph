#include "graph.hpp"
#include "ensure.hpp"
#include "vector.hpp"

namespace alex::graph {

bool contains_resource(vector_t<framegraph_resource_t *> resources,
                       std::string_view name) {

  for (framegraph_resource_t *resource : resources.span()) {
    if (resource->name == name)
      return true;
  }

  return false;
}
#if 0
auto create_framegraph_nodes(std::span<renderpass_t *> renderpasses,
                             std::span<framegraph_resource_t> resources,
                             memory::arena &arena)
    -> std::span<framegraph_node_t> {
  ENSURE_NOT(renderpasses.empty(), "must have renderpasses")
  ENSURE_NOT(resources.empty(), "must have resources")

  auto nodes = arena.allocate<framegraph_node_t>(renderpasses.size());

  // TODO: simplify this shit
  for (std::size_t nodei = 0; nodei < renderpasses.size(); nodei++) {
    nodes[nodei].name = renderpasses[nodei]->name;
    nodes[nodei].inputs.init(&arena, 3);
    nodes[nodei].outputs.init(&arena, 3);

    for (resource_t *input : renderpasses[nodei]->inputs) {
      std::string_view input_name = input->name;

      for (std::size_t resourcei = 0; resourcei < resources.size();
           resourcei++) {
        if (resources[resourcei].name == input_name) {
          if (!contains_resource(nodes[nodei].inputs,
                                 resources[resourcei].name)) {
            nodes[nodei].inputs.put(&resources[resourcei]);
          }
        }
      }
    }

    for (resource_t *output : renderpasses[nodei]->outputs) {
      std::string_view output_name = output->name;
      for (std::size_t resourcei = 0; resourcei < resources.size();
           resourcei++) {
        if (resources[resourcei].name == output_name) {
          if (!contains_resource(nodes[nodei].outputs,
                                 resources[resourcei].name)) {
            nodes[nodei].outputs.put(&resources[resourcei]);
          }
        }
      }
    }
  }

  return nodes;
}

auto create_framegraph_resources(std::span<renderpass_t *> renderpasses,
                                 memory::arena &arena)
    -> std::span<framegraph_resource_t> {
  ENSURE_NOT(renderpasses.empty(), "must have renderpasses")

  vector_t<framegraph_resource_t> buffer;
  std::size_t const estimated_buffer_size = renderpasses.size() * 2;
  buffer.init(&arena, estimated_buffer_size);

  for (std::size_t i = 0; i < renderpasses.size(); i++) {
    ENSURE(renderpasses[i] != nullptr, "renderpass must not be nullptr");
    for (resource_t *input : renderpasses[i]->inputs) {
      ENSURE(input != nullptr, "renderpass input must not be nullptr");
      framegraph_resource_t *curr = buffer.put_empty();
      curr->name = input->name;
      curr->type = input->type;
    }

    for (resource_t *output : renderpasses[i]->outputs) {
      ENSURE(output != nullptr, "renderpass output must not be nullptr");
      framegraph_resource_t *curr = buffer.put_empty();
      curr->name = output->name;
      curr->type = output->type;
    }
  }

  return buffer.span();
}

auto insert_resource_producers(std::span<renderpass_t *> renderpasses,
                               memory::arena &arena)
    -> std::span<framegraph_node_t> {
  auto nodes = arena.allocate<framegraph_node_t>(renderpasses.size());
  for (std::size_t i = 0; i < renderpasses.size(); i++) {
    nodes[i].name = renderpasses[i]->name;
  }

  return nodes;
}

bool nodes_should_be_swapped(framegraph_node_t &left,
                             framegraph_node_t &right) {
  for (framegraph_resource_t *left_input : left.inputs.span()) {
    for (framegraph_resource_t *right_output : right.outputs.span()) {
      ENSURE(left_input != nullptr, "invalid ptr")
      ENSURE(right_output != nullptr, "invalid ptr")
      if (left_input->name == right_output->name) {
        return true;
      }
    }
  }

  return false;
}

void topological_sort_nodes(std::span<framegraph_node_t> nodes) {
  ENSURE_NOT(nodes.empty(), "must have atleast 1 node")

  if (nodes.size() == 1) {
    return;
  }

  std::size_t l = 0;
  std::size_t r = nodes.size() - 1;
  while (l < r) {
    std::println("l/r {}/{}", l, r);
    if (nodes_should_be_swapped(nodes[l], nodes[r])) {
      std::println("swapping {} and {}", nodes[l].name, nodes[r].name);
      std::swap(nodes[l], nodes[r]);
      r--;
    } else {
      std::println("didnt swap {} and {}", nodes[l].name, nodes[r].name);
      l++;
    }
  }
}

void insert_resource_producers(std::span<framegraph_node_t> nodes,
                               std::span<framegraph_resource_t> resources) {
  ENSURE_NOT(nodes.empty(), "must have atleast 1 node")
  ENSURE_NOT(resources.empty(), "must have atleast 1 resource")

  for (framegraph_node_t &node : nodes) {
    for (framegraph_resource_t *output : outputs) {
      
    }
  }
}
#endif

void graph_t::init_framegraph_resources() {
  m_resources =
      m_arena->allocate<framegraph_resource_t *>(m_resource_infos.size());

  for (std::size_t i = 0; i < m_resources.size(); i++) {
    m_resources[i] = m_arena->allocate<framegraph_resource_t>(1).data();

    m_resources[i]->name = m_resource_infos[i]->name;
    m_resources[i]->type = m_resource_infos[i]->type;
  }
}

framegraph_resource_t *graph_t::find_resource(std::string_view name) {
  for (framegraph_resource_t *resource : m_resources) {
    if (resource->name == name) {
      return resource;
    }
  }

  return nullptr;
}

void graph_t::init_framegraph_nodes() {
  m_nodes = m_arena->allocate<framegraph_node_t *>(m_renderpass_infos.size());
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    m_nodes[i] = m_arena->allocate<framegraph_node_t>(1).data();
    m_nodes[i]->name = m_renderpass_infos[i]->name;
    m_nodes[i]->inputs.init(m_arena, 10);

    for (std::string_view input : m_renderpass_infos[i]->inputs) {
      framegraph_resource_t *resource = find_resource(input);
      ENSURE(resource != nullptr, "could not find resource")
      m_nodes[i]->inputs.put(resource);
      resource->reference_count++;
    }

    m_nodes[i]->outputs.init(m_arena, 10);
    for (std::string_view output : m_renderpass_infos[i]->outputs) {
      framegraph_resource_t *resource = find_resource(output);
      ENSURE(resource != nullptr, "could not find resource")
      m_nodes[i]->outputs.put(resource);
      ENSURE(resource->producer == nullptr,
             "resource producer was already assigned")
      resource->producer = m_nodes[i];
    }
  }
}

void graph_t::prune_unused_resources() {
  vector_t<framegraph_resource_t *> used;
  used.init(m_arena, m_resources.size());

  for (framegraph_resource_t *resource : m_resources) {
    if (resource->reference_count > 0) {
      used.put(resource);
    }
  }

  m_resources = used.span();
}

void graph_t::prune_unused_nodes() {
  // TODO: not implemented
}

void graph_t::connect_node_edges() {
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
     m_nodes[i]->edges.init(m_arena, 3);
     for (framegraph_resource_t* input : m_nodes[i]->inputs.span()) {
		 m_nodes[i]->edges.put(input->producer);
     }
  }
}

void graph_t::init(graph_info_t &info) {
  ENSURE(info.arena != nullptr, "allocator must not be nullptr");
  ENSURE_NOT(info.renderpass_infos.empty(), "must have renderpass infos");
  ENSURE_NOT(info.resource_infos.empty(), "must have resource infos");
  m_arena = info.arena;
  m_renderpass_infos = info.renderpass_infos;
  m_resource_infos = info.resource_infos;

  init_framegraph_resources();
  init_framegraph_nodes();
  prune_unused_resources();
  prune_unused_nodes();
  connect_node_edges();
  debug_print();
}

void graph_t::debug_print() {
  std::println("====================");
  std::println("nodes:");
  for (alex::graph::framegraph_node_t *node : m_nodes) {
    std::println("  {}", node->name);
    std::print("    [in: ");
    for (alex::graph::framegraph_resource_t *input : node->inputs.span()) {
      std::print("{} ", input->name);
    }
    std::println("]");

    std::print("    [out: ");
    for (alex::graph::framegraph_resource_t *output : node->outputs.span()) {
      std::print("{} ", output->name);
    }
    std::println("]");

    std::print("    [depends on: ");
    for (alex::graph::framegraph_node_t *edge : node->edges.span()) {
      std::print("{} ", edge->name);
    }
    std::println("]");
  }
  std::println("");
  std::println("resources:");
  for (alex::graph::framegraph_resource_t *resource : m_resources) {
    ENSURE(resource->producer != nullptr, "found resource with no producer")
    std::println("  ({}) producer: {}, refs: {}", resource->name,
                 resource->producer->name, resource->reference_count);
  }

  std::println("====================");
}

} // namespace alex::graph
