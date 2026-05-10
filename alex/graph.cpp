#include "graph.hpp"
#include "arena.hpp"
#include "core.hpp"
#include "drawing.hpp"
#include "ensure.hpp"
#include "graph_builder.hpp"
#include "log.hpp"
#include "read_spirv_source.hpp"
#include "texture_storage.hpp"

#include <iostream>
#include <ranges>
#include <variant>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace alex::graph {

node_synchronization_t::lock_t::lock_t(vk::Semaphore semaphore)
    : _semaphore{semaphore} {}

auto node_synchronization_t::lock_t::take() -> vk::Semaphore {
  _taken = true;
  return semaphore();
}

auto node_synchronization_t::lock_t::semaphore() -> vk::Semaphore {
  return _semaphore;
}

auto node_synchronization_t::lock_t::reset() -> void { _taken = false; }

auto node_synchronization_t::lock_t::is_taken() -> bool { return _taken; }

node_synchronization_t::node_synchronization_t(
    node_synchronization_info_t info) {
  auto semaphore_create_info = vk::SemaphoreCreateInfo{};

  for (std::vector<lock_t> &lock_group : lock_groups) {
    for (std::size_t i = 0; i < info.children_count; i++) {
      vk::Semaphore semaphore;
      vk::Result result = info.device.createSemaphore(&semaphore_create_info,
                                                      nullptr, &semaphore);
      ENSURE(result == vk::Result::eSuccess, "could not allocate semaphore");
      lock_group.emplace_back(std::move(semaphore));
    }
  }

  auto commandbuffer_alloc_info =
      vk::CommandBufferAllocateInfo{}
          .setLevel(vk::CommandBufferLevel::ePrimary)
          .setCommandPool(info.commandpool)
          .setCommandBufferCount(frames_in_flight);

  vk::Result result = info.device.allocateCommandBuffers(
      &commandbuffer_alloc_info, commandbuffers.data());
  ENSURE(result == vk::Result::eSuccess, "could not allocate commandbuffers");
}

auto node_synchronization_t::commandbuffer(std::uint32_t flightframe)
    -> vk::CommandBuffer {
  return commandbuffers[flightframe];
}

auto node_synchronization_t::locks(std::uint32_t flightframe)
    -> std::span<lock_t> {
  return lock_groups[flightframe];
}

auto node_synchronization_t::reset_locks(std::uint32_t flightframe) -> void {
  for (lock_t &lock : locks(flightframe)) {
    lock.reset();
  }
}

auto node_synchronization_t::take_lock(std::uint32_t flightframe,
                                       std::size_t index) -> vk::Semaphore {

  lock_t &lock = locks(flightframe)[index];
  ENSURE(lock.is_taken() == false,
         "lock was already taken for flightframe {} index {}", flightframe,
         index)
  return lock.take();
}

auto node_edges_t::add_dependency(node_t *dependency) -> node_t * {
  _dependencies.push_back(dependency);
  return _children.back();
}

auto node_edges_t::add_child(node_t *child) -> node_t * {
  _children.push_back(child);
  return _children.back();
}

auto node_edges_t::dependencies() -> std::span<node_t *> {
  return _dependencies;
}

auto node_edges_t::children() -> std::span<node_t *> { return _children; }

resource_t::resource_t(std::string_view name, texture_info_t texture)
    : name{name}, resource{texture} {}

resource_t::resource_t(std::string_view name, attachment_info_t attachment)
    : name{name}, resource{attachment} {}

bool is_renderpass_node(node_t &node) {
  return std::holds_alternative<renderpass_node_t>(node);
}
bool is_uploadpass_node(node_t &node) {
  return std::holds_alternative<renderpass_node_t>(node);
}

std::string_view get_name(node_t &node) {
  auto const _get_name = [](auto &n) -> std::string_view { return n.name; };
  return std::visit(_get_name, node);
}

void add_dependency(node_t &node, node_t *dependency) {
  ENSURE(dependency != nullptr, "got nullptr dependency")
  auto const _add_dependency = [&](auto &n) {
    n.node_edges.add_dependency(dependency);
  };
  std::visit(_add_dependency, node);
}

void add_child(node_t &node, node_t *child) {
  ENSURE(child != nullptr, "got nullptr dependency")
  auto const _add_child = [&](auto &n) { n.node_edges.add_child(child); };
  std::visit(_add_child, node);
}

std::span<node_t *> get_dependencies(node_t &node) {
  auto const _get_dependencies = [&](auto &n) {
    return n.node_edges.dependencies();
  };
  return std::visit(_get_dependencies, node);
}

std::span<node_t *> get_children(node_t &node) {
  auto const _get_children = [&](auto &n) { return n.node_edges.children(); };
  return std::visit(_get_children, node);
}

node_synchronization_t &get_sync(node_t &node) {
  if (auto *p = std::get_if<renderpass_node_t>(&node)) {
    return p->sync;
  } else if (auto *p = std::get_if<uploadpass_node_t>(&node)) {
    return p->sync;
  }

  UNREACHABLE("invalid node type")
  std::unreachable();
}

std::optional<vk::Semaphore> get_wait_semaphore(node_t &dependency,
                                                std::string_view name,
                                                std::uint32_t flightframe) {
  std::size_t node_index_for_child{0};
  for (node_t *child : get_children(dependency)) {
    ENSURE(child != nullptr, "found nullptr child for node {}", name)
    if (get_name(*child) == name) {
      return get_sync(dependency).take_lock(flightframe, node_index_for_child);
    }

    node_index_for_child++;
  }

  return std::nullopt;
}

graph_t::graph_t(graph_info_t &info) {
  ENSURE_NOT(info.framepass_infos.empty(), "must have renderpass infos");
  init_resources(info);
  init_nodes(info);
  connect_node_dependencies(info);
  connect_node_children();

  // prune_unused_resources();
  // prune_unused_nodes();

  sort_nodes();
  create_framepass_resources(info);
  create_framepass_nodes(info);
}

void graph_t::init_resources(graph_info_t &info) {
  for (texture_info_t &info : info.texture_infos) {
    m_resources.push_back(std::make_unique<resource_t>(info.name, info));
  }

  for (attachment_info_t &info : info.attachment_infos) {
    m_resources.push_back(std::make_unique<resource_t>(info.name, info));
  }
}

node_t *graph_t::find_node(std::string_view name) {
  for (std::unique_ptr<node_t> &node : m_nodes) {
    if (node != nullptr && get_name(*node) == name) {
      return node.get();
    }
  }

  return nullptr;
}

resource_t *graph_t::find_resource(std::string_view name) {
  for (std::unique_ptr<resource_t> &resource : m_resources) {
    if (resource != nullptr && resource->name == name) {
      return resource.get();
    }
  }

  return nullptr;
}

flightframe_array_t<vk::ImageView>
graph_t::get_attachment_views(std::string_view name) {
  std::span<texture_t> textures = m_texture_storage.find(name);
  if (textures.empty()) {
    return {};
  }

  flightframe_array_t<vk::ImageView> views;
  for (const auto &[i, texture] : textures | std::views::enumerate) {
    views[i] = texture.view;
  }

  return views;
}

void graph_t::init_nodes(graph_info_t &info) {

  for (renderpass_info_t &framepass_info : info.framepass_infos) {
    m_nodes.push_back(std::make_unique<node_t>(renderpass_node_t{}));
    auto *renderpass = std::get_if<renderpass_node_t>(m_nodes.back().get());
    ENSURE(renderpass != nullptr, "")
    renderpass->name = framepass_info.name;
    renderpass->extent = framepass_info.extent;

    if (framepass_info.color_attachment.has_value()) {
      renderpass->color_attachment =
          find_resource(framepass_info.color_attachment.value());
      ENSURE(renderpass->color_attachment != nullptr,
             "could not find specified color attachment {} for node {}",
             framepass_info.color_attachment.value(), renderpass->name)
    }

    if (framepass_info.depth_attachment.has_value()) {
      renderpass->depth_attachment =
          find_resource(framepass_info.depth_attachment.value());
      ENSURE(renderpass->depth_attachment != nullptr,
             "could not find specified depth attachment {} for node {}",
             framepass_info.depth_attachment.value(), renderpass->name)
    }

    for (renderpass_info_t::name_and_usage_t &input : framepass_info.inputs) {
      resource_t *resource = find_resource(input.name);
      ENSURE(resource != nullptr, "could not find resource")
      renderpass->inputs.push_back(resource);
      resource->reference_count++;
    }

    for (std::string_view output : framepass_info.outputs) {
      resource_t *resource = find_resource(output);
      renderpass->outputs.push_back(resource);
    }
  }

  for (uploadpass_info_t &uploadpass_info : info.uploadpass_infos) {
    m_nodes.push_back(std::make_unique<node_t>(uploadpass_node_t{}));
    auto *uploadpass = std::get_if<uploadpass_node_t>(m_nodes.back().get());
    ENSURE(uploadpass != nullptr, "")
    uploadpass->name = uploadpass_info.name;
  }
}

void graph_t::prune_unused_resources() {
  decltype(m_resources) used;
  used.reserve((m_resources.size()));

  for (std::unique_ptr<resource_t> &resource : m_resources) {
    if (resource != nullptr && resource->reference_count > 0) {
      used.push_back(std::move(resource));
    }
  }

  m_resources = std::move(used);
}

void graph_t::prune_unused_nodes() {
  // decltype(m_nodes) used;
  // used.reserve((m_nodes.size()));
  //
  // for (std::unique_ptr<renderpass_node_t> &node : m_nodes) {
  //   if (node != nullptr && node->inputs.size() > 1 &&
  //       node->outputs.size() < 1) {
  //     used.push_back(std::move(node));
  //   }
  // }
  //
  // m_nodes = std::move(used);
}

void graph_t::connect_node_dependencies(graph_info_t &info) {
  for (renderpass_info_t &framepass_info : info.framepass_infos) {
    node_t *node = find_node(framepass_info.name);
    ENSURE(node != nullptr, "nullptr node")
    for (std::string &dependency_name : framepass_info.dependencies) {
      node_t *dependency = find_node(dependency_name);
      ENSURE(dependency != nullptr, "nullptr dependency for node {}",
             get_name(*node))
      add_dependency(*node, dependency);
    }
  }

  for (uploadpass_info_t &uploadpass_info : info.uploadpass_infos) {
    node_t *node = find_node(uploadpass_info.name);
    ENSURE(node != nullptr, "nullptr node")
    for (std::string &dependency_name : uploadpass_info.dependencies) {
      node_t *dependency = find_node(dependency_name);
      ENSURE(dependency != nullptr, "nullptr dependency for node {}",
             get_name(*node))
      add_dependency(*node, dependency);
    }
  }
}

void graph_t::connect_node_children() {
  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node");
    for (node_t *dependency : get_dependencies(*node)) {
      ENSURE(dependency != nullptr,
             "found nullptr dependency specified by node {}", get_name(*node));

      add_child(*dependency, node.get());
    }
  }
}

void graph_t::print_execution_order(std::ostream &os) {
  std::println(os, "nodes:");
  for (std::unique_ptr<node_t> &current : m_nodes) {
    ENSURE(current != nullptr, "")

    if (auto *p = std::get_if<renderpass_node_t>(current.get())) {
      std::println(os, "\t[renderpass] \"{}\"", p->name);
      if (!p->inputs.empty()) {
        std::print(os, "\t\tin: [ ");
        for (resource_t *input : p->inputs) {
          std::print(os, "{} ", input->name);
        }
        std::println(os, "]");
      }

      if (!p->outputs.empty()) {
        std::print(os, "\t\tout: [ ");
        for (resource_t *output : p->outputs) {
          std::print(os, "{} ", output->name);
        }
        std::println(os, "]");
      }
      if (p->color_attachment != nullptr) {
        std::println(os, "\t\tcolor attachment: [ {} ]",
                     p->color_attachment->name);
      }

      if (p->depth_attachment != nullptr) {
        std::println(os, "\t\tdepth attachment: [ {} ]",
                     p->depth_attachment->name);
      }
    } else if (auto *p = std::get_if<uploadpass_node_t>(current.get())) {
      std::println(os, "\t[uploadpass] \"{}\"", p->name);
    } else {
      UNREACHABLE("invalid node type")
      std::unreachable();
    }

    if (!get_dependencies(*current).empty()) {
      std::print(os, "\t\tdepends on: [ ");
      for (node_t *edge : get_dependencies(*current)) {
        std::print(os, "{} ", get_name(*edge));
      }
      std::println(os, "]");
    }
  }
}

namespace traits {
static constexpr std::string_view const presentation_node =
    "[shape=box, style=outline, color=green]";
static constexpr std::string_view const framepass_node =
    "[shape=box, style=outline, color=black]";
static constexpr std::string_view const uploadpass_node =
    "[shape=box, style=outline, color=blue]";
static constexpr std::string_view const attachment_node =
    "[shape=oval, style=outline, color=red]";
static constexpr std::string_view const resource_node =
    "[shape=oval, style=outline]";
static constexpr std::string_view const input_resource_arrow = "";
static constexpr std::string_view const output_resource_arrow = "";
static constexpr std::string_view const attachment_arrow =
    "[arrowhead=none, style=dashed, color=red]";
static constexpr std::string_view const dependency_arrow = "";
}; // namespace traits

void graph_t::print_graphviz(std::ostream &os) {
  std::println(os, "digraph R {{");

  for (std::unique_ptr<resource_t> &resource : m_resources) {
    ENSURE(resource != nullptr, "found nullptr resource")
    std::string_view node_style = traits::resource_node;
    if (std::holds_alternative<attachment_info_t>(resource->resource)) {
      node_style = traits::attachment_node;
    }

    std::println(os, "\t\"{}\" {}", resource->name, node_style);
  }

  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")
    if (auto *p = std::get_if<renderpass_node_t>(node.get())) {
      std::println(os, "\t\"{}\" {}", p->name, traits::framepass_node);
    } else if (auto *p = std::get_if<uploadpass_node_t>(node.get())) {
      std::println(os, "\t\"{}\" {}", p->name, traits::uploadpass_node);
    } else {
      UNREACHABLE("invalid node type")
      std::unreachable();
    }
  }

  for (std::unique_ptr<node_t> &current : m_nodes) {

    if (auto *p = std::get_if<renderpass_node_t>(current.get())) {
      if (p->color_attachment != nullptr) {
        std::println(os, "\t\"{}\" -> \"{}\" {}", p->name,
                     p->color_attachment->name, traits::attachment_arrow);
      }

      if (p->depth_attachment != nullptr) {
        std::println(os, "\t\"{}\" -> \"{}\" {}", p->name,
                     p->depth_attachment->name, traits::attachment_arrow);
      }

      for (resource_t *input : p->inputs) {
        ENSURE(input != nullptr, "found nullptr input")
        std::println(os, "\t\"{}\" -> \"{}\" {}", input->name, p->name,
                     traits::input_resource_arrow);
      }

      for (resource_t *output : p->outputs) {
        ENSURE(output != nullptr, "found nullptr output")
        std::println(os, "\t\"{}\" -> \"{}\" {}", p->name, output->name,
                     traits::output_resource_arrow);
      }

    } else if (/*auto *p = */ std::get_if<uploadpass_node_t>(current.get())) {
    } else {
      UNREACHABLE("invalid node type")
      std::unreachable();
    }

    for (node_t *dependency : get_dependencies(*current)) {
      ENSURE(dependency != nullptr, "found nullptr dependency for {}",
             get_name(*current))
      std::println(os, "\t\"{}\" -> \"{}\" {}", get_name(*dependency),
                   get_name(*current), traits::dependency_arrow);
    }
  }

  std::println(os, "}}");
}

void graph_t::sort_nodes() {
  // TODO: i do not think sorting like this (only checking a single dependency
  // layer) is good enough..

  auto const is_a_dependency = [](std::unique_ptr<node_t> &a,
                                  std::unique_ptr<node_t> &b) -> bool {
    ENSURE(a != nullptr, "found nullptr node")
    ENSURE(b != nullptr, "found nullptr node")

    for (node_t *dependency : get_dependencies(*a)) {
      ENSURE(dependency != nullptr, "found nullptr dependency")
      if (get_name(*dependency) == get_name(*b))
        return false;
    }

    return true;
  };

  std::ranges::sort(m_nodes, is_a_dependency);
}

void graph_t::create_framepass_resources(graph_info_t &info) {
  for (std::unique_ptr<resource_t> &resource : m_resources) {
    if (auto *texture = std::get_if<texture_info_t>(&resource->resource)) {
      alex::texture_info_t texture_info;
      texture_info.physical_device = info.physical_device;
      texture_info.device = info.device;
      texture_info.extent.setWidth(texture->extent.width)
          .setHeight(texture->extent.height);

      texture_info.format = texture->format;
      texture_info.tiling = vk::ImageTiling::eOptimal;
      texture_info.aspect_flags = texture->aspect_flags;
      texture_info.property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
      texture_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                           vk::ImageUsageFlagBits::eTransferSrc |
                           vk::ImageUsageFlagBits::eSampled;

      std::vector<texture_t> textures;
      textures.resize(frames_in_flight);
      for (texture_t &texture : textures) {
        texture.init(texture_info);
      }

      m_texture_storage.add(resource->name, textures);
    } else if (auto *attachment =
                   std::get_if<attachment_info_t>(&resource->resource)) {

      alex::texture_info_t attachment_info;
      attachment_info.physical_device = info.physical_device;
      attachment_info.device = info.device;
      attachment_info.extent.setWidth(attachment->extent.width)
          .setHeight(attachment->extent.height);

      attachment_info.format = attachment->format;
      attachment_info.tiling = vk::ImageTiling::eOptimal;
      attachment_info.aspect_flags = attachment->aspect_flags;
      attachment_info.property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
      attachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                              vk::ImageUsageFlagBits::eTransferSrc |
                              vk::ImageUsageFlagBits::eSampled;

      if (attachment->type == attachment_type_t::color) {
        attachment_info.usage |= vk::ImageUsageFlagBits::eColorAttachment;
      } else if (attachment->type == attachment_type_t::depth) {
        attachment_info.usage |=
            vk::ImageUsageFlagBits::eDepthStencilAttachment;
      }

      std::vector<texture_t> attachments;
      attachments.resize(frames_in_flight);
      for (texture_t &attachment : attachments) {
        attachment.init(attachment_info);
      }

      m_texture_storage.add(resource->name, attachments);
    } else {
      UNREACHABLE("invalid resource type")
      std::unreachable();
    }
  }
}

void graph_t::create_framepass_nodes(graph_info_t &info) {
  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")
    if (auto *p = std::get_if<renderpass_node_t>(node.get())) {
      ENSURE(p->color_attachment != nullptr,
             "found nullptr color attachment for node {}", p->name)
      ENSURE(p->depth_attachment != nullptr,
             "found nullptr depth attachment for node {}", p->name)

      auto *color_attachment =
          std::get_if<attachment_info_t>(&p->color_attachment->resource);
      auto *depth_attachment =
          std::get_if<attachment_info_t>(&p->depth_attachment->resource);

      ENSURE(color_attachment != nullptr, "no color attachment for node {}",
             p->name);
      ENSURE(depth_attachment != nullptr, "no depth attachment for node {}",
             p->name);

      auto geometrypass_info =
          geometrypass_info_t(info.device)
              .set_extent(p->extent)
              .set_loadop(vk::AttachmentLoadOp::eClear)
              .set_color_attachments(
                  get_attachment_views(p->color_attachment->name))
              // TODO: propagate clearcolor to renderpass_info_t
              .set_color_clearvalue(0.0f, 0.0f, 0.0f, 1.0f)
              .set_color_format(color_attachment->format)
              .set_depth_attachments(
                  get_attachment_views(p->depth_attachment->name))
              .set_depth_clearvalue(1.0f)
              .set_depth_format(depth_attachment->format);

      p->geometry_pass = geometrypass_t(geometrypass_info);

      node_synchronization_info_t sync_info;
      sync_info.device = info.device;
      sync_info.commandpool = info.commandpool;
      sync_info.children_count = get_children(*node).size();
      p->sync = node_synchronization_t(sync_info);

    } else if (auto *p = std::get_if<uploadpass_node_t>(node.get())) {
      node_synchronization_info_t sync_info;
      sync_info.device = info.device;
      sync_info.commandpool = info.commandpool;
      sync_info.children_count = get_children(*node).size();
      p->sync = node_synchronization_t(sync_info);
    } else {
      ENSURE(false, "unreachable")
      std::unreachable();
    }
  }
}

vk::CommandBuffer
uploadpass_node_t::record(std::span<uploadpass_command_t> commands,
                          std::uint32_t flightframe) {

  vk::CommandBuffer commandbuffer = sync.commandbuffer(flightframe);
  commandbuffer.begin(vk::CommandBufferBeginInfo{});
  for (uploadpass_command_t &command : commands) {
    if (auto *p = std::get_if<command::buffer_upload_t>(&command)) {
      alex::memory_buffer_write_info_t write_info;
      write_info.physical_device = p->physical_device;
      write_info.device = p->device;
      write_info.direct = p->direct_buffer;
      write_info.write_size = p->buffer->memory_size();
      write_info.commandbuffer = commandbuffer;
      p->buffer->record_write(write_info);
    } else {
      UNREACHABLE("invalid uploadpass command")
      std::unreachable();
    }
  }

  commandbuffer.end();
  return commandbuffer;
}

vk::CommandBuffer
renderpass_node_t::record(std::span<renderpass_command_t> commands,
                          std::uint32_t flightframe) {

  vk::CommandBuffer commandbuffer = sync.commandbuffer(flightframe);
  commandbuffer.begin(vk::CommandBufferBeginInfo{});
  const auto render_area =
      vk::Rect2D{}
          .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
          .setExtent(vk::Extent2D(extent.width, extent.height));

  ENSURE(geometry_pass.renderpass != VK_NULL_HANDLE,
         "renderpass is nullhandle for node {}", name)
  ENSURE(geometry_pass.framebuffers[flightframe] != VK_NULL_HANDLE,
         "framebuffer is nullhandle for node {}", name)

  const auto renderpass_begin_info =
      vk::RenderPassBeginInfo{}
          .setRenderPass(geometry_pass.renderpass)
          .setFramebuffer(geometry_pass.framebuffers[flightframe])
          .setRenderArea(render_area)
          .setClearValues(geometry_pass.clearvalues);

  commandbuffer.beginRenderPass(renderpass_begin_info,
                                vk::SubpassContents::eInline);

  for (renderpass_command_t &command : commands) {
    if (auto *p = std::get_if<command::draw_t>(&command)) {
      commandbuffer.draw(p->vertex_count, p->instance_count, p->first_vertex,
                         p->first_instance);
    } else if (auto *p = std::get_if<command::bind_pipeline_t>(&command)) {
      commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, p->pipeline);
    } else if (auto *p = std::get_if<command::set_viewport_t>(&command)) {
      auto viewport = vk::Viewport{}
                          .setX(p->x)
                          .setY(p->y)
                          .setWidth(p->w)
                          .setHeight(p->h)
                          .setMinDepth(p->depth.min)
                          .setMaxDepth(p->depth.max);

      commandbuffer.setViewport(0, viewport);
    } else if (auto *p = std::get_if<command::set_scissor_t>(&command)) {
      auto scissor = vk::Rect2D{}.setOffset(p->offset).setExtent(p->extent);
      commandbuffer.setScissor(0, scissor);
    } else if (auto *p = std::get_if<command::bind_vertexbuffer_t>(&command)) {

      commandbuffer.bindVertexBuffers(
          p->first_binding, p->binding_offsets.size(), p->buffers.data(),
          p->binding_offsets.data());

    } else if (auto *p = std::get_if<command::bind_indexbuffer_t>(&command)) {
      commandbuffer.bindIndexBuffer(p->buffer, p->offset, p->type);
    } else if (auto *p =
                   std::get_if<command::bind_descriptorsets_t>(&command)) {
      commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       p->layout, p->first_set, p->sets.size(),
                                       p->sets.data(), 0, nullptr);
    } else if (auto *p = std::get_if<command::custom_command_t>(&command)) {
		p->fn(commandbuffer);
    } else {
      UNREACHABLE("invalid draw command type")
      std::unreachable();
    }
  }

  commandbuffer.endRenderPass();
  commandbuffer.end();
  return commandbuffer;
}

void graph_t::evaluate(evaluate_info_t &info) {
  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")

    vk::CommandBuffer recorded;
    // vk::Semaphore sync_semaphore;

    if (auto *renderpass = std::get_if<renderpass_node_t>(node.get())) {
      auto found_commands = std::ranges::find_if(
          info.renderpass_commands, [&](renderpass_commands_t &commands) {
            return commands.renderpass_name == get_name(*node);
          });

      if (found_commands == info.renderpass_commands.end()) {
        continue;
      }

      recorded = renderpass->record(found_commands->commands, info.flightframe);

    } else if (auto *uploadpass = std::get_if<uploadpass_node_t>(node.get())) {
      auto found_commands = std::ranges::find_if(
          info.uploadpass_commands, [&](uploadpass_commands_t &commands) {
            return commands.renderpass_name == get_name(*node);
          });

      if (found_commands == info.uploadpass_commands.end()) {
        continue;
      }

      recorded = uploadpass->record(found_commands->commands, info.flightframe);
    } else {
      UNREACHABLE("invalid node")
      std::unreachable();
    }

    get_sync(*node).reset_locks(info.flightframe);

    // TODO: This must be deduced by dependencies, they have to specify what
    // type of dependency they are and they correspond to wait dst stage
    // masks TopOfPipe is not a good solution!
    // So basically each dependency has a mask for itself, and it will then wait
    // for all masks before computing
    std::array<vk::PipelineStageFlags, 1> const wait_dst_stage_masks{
        vk::PipelineStageFlagBits::eTopOfPipe};

    std::vector<vk::Semaphore> wait_semaphores;
    for (node_t *dependency : get_dependencies(*node)) {
      ENSURE(dependency != nullptr, "found nullptr dependency")
      std::optional<vk::Semaphore> wait_semaphore =
          get_wait_semaphore(*dependency, get_name(*node), info.flightframe);

      ENSURE(wait_semaphore.has_value(),
             "could not find wait semaphore for dependency for node {}",
             get_name(*node))

      wait_semaphores.push_back(wait_semaphore.value());
    }

    std::vector<vk::Semaphore> signal_semaphores;
    for (node_synchronization_t::lock_t &lock :
         get_sync(*node).locks(info.flightframe)) {
      signal_semaphores.push_back(lock.semaphore());
    }

    auto submit_info = vk::SubmitInfo{}
                           .setWaitDstStageMask(wait_dst_stage_masks)
                           .setCommandBuffers(recorded)
                           .setSignalSemaphores(signal_semaphores)
                           .setWaitSemaphores(wait_semaphores);

    info.queue.submit(submit_info);
  }
}

} // namespace alex::graph
