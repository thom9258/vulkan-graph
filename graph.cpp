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
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace alex::graph {

resource_t::resource_t(std::string_view name, texture_info_t texture)
    : name{name}, resource{texture} {}

resource_t::resource_t(std::string_view name, attachment_info_t attachment)
    : name{name}, resource{attachment} {}

std::string_view get_name(node_t &node) {
  if (auto *p = std::get_if<renderpass_node_t>(&node)) {
    return p->name;
  } else if (auto *p = std::get_if<uploadpass_node_t>(&node)) {
    return p->name;
  }

  ENSURE(false, "invalid node type")
  return "";
}

void add_dependency(node_t &node, node_t *dependency) {
  ENSURE(dependency != nullptr, "got nullptr dependency")
  if (auto *p = std::get_if<renderpass_node_t>(&node)) {
    p->dependencies.push_back(dependency);
  } else if (auto *p = std::get_if<uploadpass_node_t>(&node)) {
    p->dependencies.push_back(dependency);
  }

  ENSURE(false, "invalid node type")
}

void add_parent(node_t &node, node_t *parent) {
  if (auto *p = std::get_if<renderpass_node_t>(&node)) {
    p->parents.push_back(parent);
  } else if (auto *p = std::get_if<uploadpass_node_t>(&node)) {
    p->parents.push_back(parent);
  }

  ENSURE(false, "invalid node type")
}

std::span<node_t *> get_dependencies(node_t &node) {
  if (auto *p = std::get_if<renderpass_node_t>(&node)) {
    return p->dependencies;
  } else if (auto *p = std::get_if<uploadpass_node_t>(&node)) {
    return p->dependencies;
  }

  ENSURE(false, "invalid node type")
}

graph_t::graph_t(graph_info_t &info) {
  ENSURE_NOT(info.framepass_infos.empty(), "must have renderpass infos");
  init_resources(info);
  init_nodes(info);
  connect_node_dependencies(info);
  connect_node_parents();

  // prune_unused_resources();
  // prune_unused_nodes();

  create_framepass_resources(info);
  create_framepass_renderpasses(info);
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
    std::println("inited renderpass {}", renderpass->name);
  }

  for (uploadpass_info_t &uploadpass_info : info.uploadpass_infos) {
    m_nodes.push_back(std::make_unique<node_t>(uploadpass_node_t{}));
    auto *uploadpass = std::get_if<uploadpass_node_t>(m_nodes.back().get());
	ENSURE(uploadpass != nullptr, "")
	uploadpass->name = uploadpass_info.name;

    std::println("inited uploadpass {}", uploadpass->name);
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
	  std::println("adding dependency {} to node {}", get_name(*dependency), get_name(*node));
      add_dependency(*node, dependency);
    }
  }
}

void graph_t::connect_node_parents() {
  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node");
    for (node_t *dependency : get_dependencies(*node)) {
      ENSURE(dependency != nullptr,
             "found nullptr dependency specified by node {}", get_name(*node));
      add_parent(*dependency, node.get());
    }
  }
}

void graph_t::print_execution_order(std::ostream &os) {
  std::println(os, "nodes:");
  for (std::unique_ptr<node_t> &current : m_nodes) {
    ENSURE(current != nullptr, "")
    std::println(os, "\t{}:", get_name(*current));

    if (auto *p = std::get_if<renderpass_node_t>(current.get())) {
      std::println(os, "\t\t[renderpass]");
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
    } else if (/*auto *p = */ std::get_if<uploadpass_node_t>(current.get())) {
      std::println(os, "\t\t[uploadpass]");
    } else {
      ENSURE(false, "")
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
static constexpr std::string_view const framepass_node =
    "[shape=box, style=outline, color=black]";
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
    std::println(os, "\t\"{}\" {}", get_name(*node), traits::framepass_node);
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
      ENSURE(false, "")
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
      ENSURE(false, "resource type not supported yet")
    }
  }
}

void graph_t::create_framepass_renderpasses(graph_info_t &info) {
  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")
    auto *p = std::get_if<renderpass_node_t>(node.get());
    if (p == nullptr) {
      continue;
    }

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
  }
}

void graph_t::record(record_info_t &info) {

  for (std::unique_ptr<node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")
    auto *renderpass = std::get_if<renderpass_node_t>(node.get());
    if (renderpass == nullptr) {
      continue;
    }

#if 0    
    for (resource_t *input : node->inputs) {
      std::span<texture_t> textures = m_texture_storage.find(input->name);
      ENSURE_NOT(textures.empty(),
                 "could not find output textures for framepass output {}",
                 input->name)

      vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlags();
      vk::ImageLayout new_layout = vk::ImageLayout::eReadOnlyOptimal;
      if (auto *resource = std::get_if<attachment_info_t>(&input->resource)) {
        if (resource->type == attachment_type_t::color) {
          new_layout = vk::ImageLayout::eColorAttachmentOptimal;
          aspect_mask |= vk::ImageAspectFlagBits::eColor;
        } else if (resource->type == attachment_type_t::depth) {
          new_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
          aspect_mask |= vk::ImageAspectFlagBits::eDepth;
        }
      }

      // TODO: entirety of range and also layouts in barrier can just be stored
      // in the resosurce itself
      auto range = vk::ImageSubresourceRange{}
                       .setAspectMask(aspect_mask)
                       .setBaseMipLevel(0)
                       .setLevelCount(1)
                       .setBaseArrayLayer(0)
                       .setLayerCount(1);

      auto barrier = vk::ImageMemoryBarrier{}
                         .setImage(textures[next_frame.flightframe].image)
                         .setSubresourceRange(range)
                         .setOldLayout(input->layout)
                         .setNewLayout(new_layout)
                         .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                         .setDstAccessMask(vk::AccessFlags())
                         .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                         .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }
#endif

    auto found = std::ranges::find_if(
        info.renderpass_commands, [&](renderpass_commands_t &commands) {
          return commands.renderpass_name == get_name(*node);
        });

    if (found == info.renderpass_commands.end()) {
      LOG_WARN("renderpass node {} had no renderpass commands!", get_name(*node))
      continue;
    }

    const auto render_area =
        vk::Rect2D{}
            .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
            .setExtent(vk::Extent2D(renderpass->extent.width, renderpass->extent.height));

    ENSURE(renderpass->geometry_pass.renderpass != VK_NULL_HANDLE,
           "renderpass is nullhandle for node {}", renderpass->name)
    ENSURE(renderpass->geometry_pass.framebuffers[info.flightframe] != VK_NULL_HANDLE,
           "framebuffer is nullhandle for node {}", renderpass->name)

    const auto renderpass_begin_info =
        vk::RenderPassBeginInfo{}
            .setRenderPass(renderpass->geometry_pass.renderpass)
            .setFramebuffer(renderpass->geometry_pass.framebuffers[info.flightframe])
            .setRenderArea(render_area)
            .setClearValues(renderpass->geometry_pass.clearvalues);

    info.commandbuffer.beginRenderPass(renderpass_begin_info,
                                       vk::SubpassContents::eInline);

    for (renderpass_command_t &command : found->commands) {
      if (auto *p = std::get_if<command::draw_t>(&command)) {
        info.commandbuffer.draw(p->vertex_count, p->instance_count,
                                p->first_vertex, p->first_instance);
      } else if (auto *p = std::get_if<command::bind_pipeline_t>(&command)) {
        info.commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                        p->pipeline);
      } else if (auto *p = std::get_if<command::set_viewport_t>(&command)) {

        auto viewport = vk::Viewport{}
                            .setX(p->x)
                            .setY(p->y)
                            .setWidth(p->w)
                            .setHeight(p->h)
                            .setMinDepth(p->depth.min)
                            .setMaxDepth(p->depth.max);

        info.commandbuffer.setViewport(0, viewport);
      } else if (auto *p = std::get_if<command::set_scissor_t>(&command)) {
        auto scissor = vk::Rect2D{}.setOffset(p->offset).setExtent(p->extent);
        info.commandbuffer.setScissor(0, scissor);
      } else if (auto *p =
                     std::get_if<command::bind_vertexbuffer_t>(&command)) {

        info.commandbuffer.bindVertexBuffers(
            p->first_binding, p->binding_offsets.size(), p->buffers.data(),
            p->binding_offsets.data());

      } else if (auto *p = std::get_if<command::bind_indexbuffer_t>(&command)) {
        info.commandbuffer.bindIndexBuffer(p->buffer, p->offset, p->type);
      } else if (auto *p =
                     std::get_if<command::bind_descriptorsets_t>(&command)) {
        info.commandbuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, p->layout, p->first_set,
            p->sets.size(), p->sets.data(), 0, nullptr);
      } else {
        ENSURE(false, "Invalid unknown draw command")
      }
    }

    info.commandbuffer.endRenderPass();
  }
}

} // namespace alex::graph
