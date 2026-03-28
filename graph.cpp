#include "graph.hpp"
#include "ensure.hpp"
#include "vector.hpp"
#include <iostream>

namespace alex::graph {

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

void graph_t::connect_node_dependencies() {
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    m_nodes[i]->dependencies.init(m_arena, 3);
    for (framegraph_resource_t *input : m_nodes[i]->inputs.span()) {
      m_nodes[i]->dependencies.put(input->producer);
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
  connect_node_dependencies();
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
    for (alex::graph::framegraph_node_t *edge : node->dependencies.span()) {
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

void graph_t::debug_graphviz() {
  std::cout << "digraph \"renderpass_dependencies\" {" << std::endl;
  std::cout << "\tnode [shape=box, style=outline, color=black];" << std::endl;

  for (alex::graph::framegraph_node_t *node : m_nodes) {
    for (alex::graph::framegraph_node_t *edge : node->dependencies.span()) {
      std::cout << "\t\"" << node->name << "\" -> \""<< edge->name << "\";" << std::endl;
    }
  }

  std::cout << "}" << std::endl;
}

} // namespace alex::graph
