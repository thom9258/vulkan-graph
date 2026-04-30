#pragma once

#include "geometrypass_builder.hpp"
#include "graph_builder.hpp"
#include "memory_buffer.hpp"
#include "texture.hpp"

#include <vulkan/vulkan_enums.hpp>

#include <vector>
#include <vulkan/vulkan_handles.hpp>

namespace alex::graph {

struct node_sync_info_t {
  vk::Device device;
  vk::CommandPool commandpool;
  std::size_t children_count{0};
};

struct node_sync_t {
  node_sync_t() = default;
  node_sync_t(node_sync_info_t info);

  vk::CommandBuffer commandbuffer(std::uint32_t flightframe);
  std::span<vk::Semaphore> wait_group(std::uint32_t flightframe);

private:
  flightframe_array_t<vk::CommandBuffer> commandbuffers;
  using wait_group_t = std::vector<vk::Semaphore>;
  flightframe_array_t<wait_group_t> wait_groups;
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

struct evaluate_info_t {
  vk::Queue queue;
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

bool is_renderpass_node(node_t &node);
bool is_uploadpass_node(node_t &node);

std::string_view get_name(node_t &node);
std::span<node_t *> get_dependencies(node_t &node);
std::span<node_t *> get_children(node_t &node);
node_sync_t& get_sync(node_t &node);

std::optional<vk::Semaphore> get_wait_semaphore(node_t &dependency,
                                                std::string_view name,
                                                std::uint32_t flightframe);

void add_dependency(node_t &node, node_t *dependency);
void add_child(node_t &node, node_t *child);

struct node_recording_t {
  vk::CommandBuffer commandbuffer;
  vk::Semaphore semaphore;
};

struct renderpass_node_t {
  std::string name{""};
  resource_t *depth_attachment{nullptr};
  resource_t *color_attachment{nullptr};
  std::vector<resource_t *> inputs;
  std::vector<resource_t *> outputs;
  std::vector<node_t *> dependencies;
  std::vector<node_t *> children;
  vk::Extent3D extent;
  geometrypass_t geometry_pass;
  node_sync_t sync;

  [[nodiscard]]

  vk::CommandBuffer record(std::span<renderpass_command_t> commands,
                           std::uint32_t flightframe);
};

// TODO: AI NOTE STUFF
// The Best Practice: To be 100% spec-compliant, your Upload Command Buffer
// should still end with a vkCmdPipelineBarrier. This barrier doesn't need to
// wait for anything, but it should "flush" the writes (e.g., srcAccess =
// TRANSFER_WRITE_BIT, dstAccess = HOST_WRITE_BIT or similar) so that when the
// semaphore signals, the data is actually visible to the next pass.

/*

Broadly speaking, yes, but the "correct" stage depends entirely on where the
data is first consumed in the pipeline. The pWaitDstStageMask tells the GPU:
"You can start executing this command buffer, but you must stall the following
stage (and everything after it) until the semaphore is signaled." Here is how
you pick the stage based on your archetype dependencies:
1. Upload Pass → Render Pass (Vertex Data)
If your upload pass is filling a Vertex Buffer or Index Buffer:

  Wait Stage: VK_PIPELINE_STAGE_VERTEX_INPUT_BIT.
  Why: This is the fixed-function stage that reads your vertex/index buffers.
There is no point in waiting for the "Top of Pipe," but you must wait before the
GPU tries to pull those vertices.

2. Upload Pass → Render Pass (Uniforms)
If your upload pass is updating Uniform Buffers (UBOs) used in a Vertex Shader:

  Wait Stage: VK_PIPELINE_STAGE_VERTEX_SHADER_BIT.
  Why: The GPU can do Vertex Input (fetching positions), but it must pause
before the Vertex Shader starts executing and tries to read those constants.

3. Shadow Pass (Render) → Main Pass (Render)
If your Main Pass is reading a Shadow Map texture in the Fragment Shader:

  Wait Stage: VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT.
  Why: This is the most efficient choice. The GPU can perform Vertex Shading
and Rasterization for your main scene while the Shadow Pass is still finishing
up on the GPU. It only stalls when it reaches the pixels that need to sample the
shadow texture.

Pro-Tip: The "Catch-All" Stage
If you aren't sure where the data is used, or if it’s used in many places (like
a Compute shader that reads some buffers and writes others), many developers
use:

  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT: Specifically for when the
previous pass was a RenderPass (it waits for the previous colors to finish
writing). VK_PIPELINE_STAGE_ALL_COMMANDS_BIT: The "safe but slow" option. It
stops the entire GPU pipeline for that submission until the semaphore is
signaled.


      Summary Table for your Archetypes:
Dependency Origin	Data Type	Recommended waitDstStage
Upload Pass	Vertex/Index	VERTEX_INPUT_BIT
Upload Pass	Uniforms (VS)	VERTEX_SHADER_BIT
Render Pass	Shadow Map	FRAGMENT_SHADER_BIT
Compute Pass	Storage Buffer	COMPUTE_SHADER_BIT
Important Note on Layouts: Remember that while the Semaphore handles the timing
(waiting), it does not handle the image layout transition. If your Shadow Pass
output needs to be a SHADER_READ_ONLY_OPTIMAL for the Main Pass, ensure your
RenderPass finalLayout or a Pipeline Barrier handles that transition. Does your
graph system allow you to tag which pipeline stage a dependency belongs to?

It shouldn't be complicated at all. In fact, most robust Frame Graphs or Render
Graphs do exactly this because it is the only way to achieve maximum GPU
efficiency. If you already have a "Dependency" object or a link between your
Archetypes, you just need to add a small piece of metadata to that link.
1. The Data Structure
In your graph, a dependency usually looks like:
MainRenderPass -> depends on -> ShadowPass.
You simply expand that dependency link to include the Vulkan Stage:
cpp

struct Dependency {
  Archetype* producer;
  Archetype* consumer;
  VkPipelineStageFlags waitStage; // <--- The "Tag"
  // Optional: VkAccessFlags accessType; (e.g., Shader Read, Uniform Read)
};

Vær forsigtig med at bruge denne kode.
2. How it fits into your logic
When your graph records the submissions, it looks at the waitStage tags for all
incoming dependencies for that Archetype:

  Gather: Look at all Archetypes that must finish before the current one.
  Collect Semaphores: Get the "Finished" semaphores from those producers.
  Collect Stages: Get the waitStage tags you defined.
  Submit: Pass those arrays directly into VkSubmitInfo.

3. Why this is actually simpler in the long run
Adding these tags saves you from "guessing" or using ALL_COMMANDS_BIT (which
kills performance). It also makes your system more self-documenting:

  Shadow Dependency: Tag it FRAGMENT_SHADER_BIT.
  Upload Dependency: Tag it VERTEX_INPUT_BIT.
  Compute-to-Compute: Tag it COMPUTE_SHADER_BIT.

The "Auto-Tag" approach (Even easier)
If you don't want to manually tag every single link, you can assign a Default
Wait Stage to each Archetype Type:

  If the Consumer is a Render Archetype, the default wait is
COLOR_ATTACHMENT_OUTPUT_BIT. If the Consumer is a Compute Archetype, the default
wait is COMPUTE_SHADER_BIT.

This covers 90% of cases, and you only manually "tag" the specific ones (like
Shadow Maps) where you want to be more granular for better performance. Does
your current graph logic automatically generate the submission order, or do you
manually define the sequence?

*/

struct uploadpass_node_t {
  std::string name{""};
  std::vector<node_t *> dependencies;
  std::vector<node_t *> children;
  node_sync_t sync;

  [[nodiscard]]

  vk::CommandBuffer record(std::span<uploadpass_command_t> commands,
                           std::uint32_t flightframe);
};

struct graph_t {
  graph_t(graph_info_t &info);

  texture_storage_t m_texture_storage;
  std::vector<std::unique_ptr<node_t>> m_nodes;
  std::vector<std::unique_ptr<resource_t>> m_resources;

  void print_execution_order(std::ostream &os);
  void print_graphviz(std::ostream &os);
  void evaluate(evaluate_info_t &info);

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
  void connect_node_children();
  void sort_nodes();
  void create_framepass_resources(graph_info_t &info);
  void create_framepass_nodes(graph_info_t &info);
};

} // namespace alex::graph
