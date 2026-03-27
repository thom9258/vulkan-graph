#include "graph.hpp"

namespace alex::graph {

#if 0
void builder_t::init(memory::arena *arena) {
  this->arena = arena;
  jobs.init(this->arena, 10);
}

id_t builder_t::make_renderpass_job() {
  job_t *job = jobs.put_empty();
  job->id = next_id++;
  job->type = job_e::renderpass;
  return job->id;
}

id_t builder_t::make_presentation_job() {
  job_t *job = jobs.put_empty(*arena);
  job->id = next_id++;
  job->type = job_e::presentation;
  return job->id;
}

job_t *builder_t::find_job(id_t id) {
  for (std::size_t i = 0; i < jobs.length(); i++) {
    if (jobs[i].id == id) {
      return &jobs[i];
    }
  }

  return nullptr;
}

bool builder_t::add_dependency(id_t base, id_t depends_on) {
  job_t *found = find_job(base);
  if (found) {
    found->dependencies.put(*arena, depends_on);
    return true;
  }

  return false;
}

void print_indentation(std::size_t depth) {
  for (std::size_t i = 0; i < depth; i++) {
    std::print("    ");
  }
}

void print_renderjob(renderjob_t &job) {
  switch (job.type) {
  case renderjob_type_e::draw:
	  std::println("Draw: {}", job.name);
  case renderjob_type_e::bind_geometry_pipeline:
	  std::println("BindGeometryPipeline: {}", job.name);
  case renderjob_type_e::bind_descriptorset:
	  std::println("BindDescriptorSet: {}", job.name);
  case renderjob_type_e::bind_vertexbuffer:
	  std::println("BindVertexBuffer: {}", job.name);
  };

  std::unreachable();
}

void _print_graph(node_t *node, std::size_t depth) {
  if (node == nullptr) {
    return;
  }

  if (auto **p = std::get_if<renderpass_job_t *>(&node->job)) {
    print_indentation(depth);
    std::println("RenderPass: {}", node->name);
    print_indentation(depth+1);
    std::println("Jobs: {}", (*p)->jobs.length());
    for (std::size_t i = 0; i < (*p)->jobs.length(); i++) {
      print_indentation(depth + 1);
      print_renderjob((*p)->jobs[i]);
    }
  } else if (auto **p = std::get_if<presentation_job_t *>(&node->job)) {
    print_indentation(depth);
    std::println("Present: {} ", node->name);
    print_indentation(depth+1);
    std::println("Presentation index: {}",
                 (*p)->presentation_context->sync.image_index);
    print_indentation(depth+1);
    std::println("Flightframe: {}",
                 (*p)->presentation_context->sync.flightframe);
  }

  print_indentation(depth+1);
  std::println("Dependencies:");
  for (std::size_t i = 0; i < node->dependencies.length(); i++) {
    _print_graph(node->dependencies[i], depth + 2);
  }
}

void print_graph(node_t *root) { _print_graph(root, 0); }
#endif

} // namespace alex::graph
