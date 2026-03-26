#pragma once

#include "geometry_pipeline.hpp"
#include "presentation_context.hpp"

#include <string_view>
#include <variant>
#include <print>

namespace alex {

struct TextureDependency {
  std::string_view name;
  vk::Format format;
};

struct clear_texture_job_t {
  std::string_view texture;
};

struct presentation_job_t {
  presentation_context_t *presentation_context;
  std::string_view texture_to_present;

  void record(vk::CommandBuffer commandbuffer);
};

struct enable_renderpass_job_t {
  renderpass_t *renderpass;

  void record(vk::CommandBuffer commandbuffer);
};

struct record_geometry_pipeline_job_t {
  geometry_pipeline_t *pipeline;

  void record(vk::CommandBuffer commandbuffer);
};

using job_t =
    std::variant<clear_texture_job_t, presentation_job_t,
                 enable_renderpass_job_t, record_geometry_pipeline_job_t>;

struct graph_t {
  job_t job;
  std::string_view name{""};
  std::span<graph_t*> children;
};


void _print_graph(graph_t *graph, std::size_t depth) {
  if (graph == nullptr) {
	  return;
  }

  for (std::size_t i = 0; i < depth; i++) {
	std::print(" ");
    
  }

  std::println("{}:", graph->name);
  for (graph_t *child : graph->children) {
	  _print_graph(child, depth+1);
  }
}

void print_graph(graph_t *graph) {
	_print_graph(graph, 0);
}

} // namespace alex
