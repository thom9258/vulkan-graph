#pragma once

#include "geometry_pipeline.hpp"
#include "memory_buffer.hpp"
#include "presentation_context.hpp"
#include "vector.hpp"

#include <print>
#include <string_view>
#include <variant>
#include <vulkan/vulkan_handles.hpp>

namespace alex::graph {

struct renderpass_job_t;
struct presentation_job_t;

struct node_t {
  using job_pointer_t = std::variant<renderpass_job_t *, presentation_job_t *>;
  std::string_view name{""};
  job_pointer_t job;
  vector_t<node_t *> dependencies;
};

enum class renderjob_e {
  bind_geometry_pipeline,
  bind_vertexbuffer,
  bind_descriptorset,
  draw,
};

struct renderjob_t {
  std::string_view name{""};
  renderjob_e type;
  union {
    struct {
      geometry_pipeline_t *pipeline;
    } bind_geometry_pipeline;
    struct {
      vk::DescriptorSet *descriptorset;
    } bind_descriptorset;
    struct {
      memory_buffer_t *buffer;
    } bind_vertexbuffer;
  };
};

enum class job_e {
  presentation,
  renderpass,
};

struct job_t {
  std::string_view name;
  job_e type;
  vector_t<job_t*> dependencies;

  union {
    struct {
		presentation_context_t *presenter;
    } presentation;
    struct {
		renderpass_t *renderpass;
		vector_t<renderjob_t>* renderjobs;
    } renderpass;
  };
};





#if 0

using id_t = std::uint32_t;

struct builder_t {
  void init(memory::arena *arena);
  id_t make_renderpass_job();
  id_t make_presentation_job();
  bool add_dependency(id_t base, id_t depends_on);
  job_t* find_job(id_t id);

  memory::arena *arena{nullptr};
  id_t next_id{0};
  vector_t<job_t> jobs;
};
void print_graph(node_t *root);
void record_graph(vk::CommandBuffer commandbuffer, node_t *root);
#endif

} // namespace alex::graph
