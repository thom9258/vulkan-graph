#include "graph.hpp"
#include "core.hpp"
#include "drawing.hpp"
#include "ensure.hpp"
#include "log.hpp"
#include "read_spirv_source.hpp"
#include "vector.hpp"
#include <iostream>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace alex::graph {

void graph_t::init_framepass_resources() {
  m_resources =
      m_arena->allocate<framepass_resource_t *>(m_resource_infos.size());

  for (std::size_t i = 0; i < m_resources.size(); i++) {
    m_resources[i] = m_arena->allocate<framepass_resource_t>(1).data();
    m_resources[i]->name = m_resource_infos[i]->name;
    m_resources[i]->type = m_resource_infos[i]->type;
    switch (m_resources[i]->type) {
    case resource_type_t::texture:
      m_resources[i]->texture = m_resource_infos[i]->texture;
      break;
    case resource_type_t::attachment:
      m_resources[i]->attachment = m_resource_infos[i]->attachment;
      break;
    case resource_type_t::reference:
      ENSURE(false, "reference not supported")
      break;
    case resource_type_t::memory_buffer:
      ENSURE(false, "memory buffer not supported")
      break;
    };
  }
}

framepass_node_t *graph_t::find_node(std::string_view name) {
  for (framepass_node_t *node : m_nodes) {
    if (node->name == name) {
      return node;
    }
  }

  return nullptr;
}

framepass_resource_t *graph_t::find_resource(std::string_view name) {
  for (framepass_resource_t *resource : m_resources) {
    if (resource->name == name) {
      return resource;
    }
  }

  return nullptr;
}

void graph_t::init_framepass_nodes() {
  m_nodes = m_arena->allocate<framepass_node_t *>(m_framepass_infos.size());
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    m_nodes[i] = m_arena->allocate<framepass_node_t>(1).data();
    m_nodes[i]->name = m_framepass_infos[i]->name;
    m_nodes[i]->extent = m_framepass_infos[i]->extent;
    m_nodes[i]->vertex_program_path = m_framepass_infos[i]->vertex_program_path;
    m_nodes[i]->fragment_program_path =
        m_framepass_infos[i]->fragment_program_path;
    m_nodes[i]->set_layouts = m_framepass_infos[i]->set_layouts;
    m_nodes[i]->inputs.init(m_arena, 10);

    for (std::string_view input : m_framepass_infos[i]->inputs) {
      framepass_resource_t *resource = find_resource(input);
      ENSURE(resource != nullptr, "could not find resource")
      m_nodes[i]->inputs.put(resource);
      resource->reference_count++;
    }

    m_nodes[i]->outputs.init(m_arena, 10);
    for (std::string_view output : m_framepass_infos[i]->outputs) {
      framepass_resource_t *resource = find_resource(output);
      ENSURE(resource != nullptr, "could not find resource")
      m_nodes[i]->outputs.put(resource);
      ENSURE(resource->producer == nullptr,
             "resource producer was already assigned")
      resource->producer = m_nodes[i];
    }
  }
}

void graph_t::prune_unused_resources() {
  vector_t<framepass_resource_t *> used;
  used.init(m_arena, m_resources.size());

  for (framepass_resource_t *resource : m_resources) {
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
    for (framepass_resource_t *output : m_nodes[i]->outputs.span()) {
      if (output->producer != nullptr) {
        LOG_INFO("output {} already has a producer {}", output->name,
                 output->producer->name)
        continue;
      }
      //     ENSURE(output->producer == nullptr, "output {} already has producer
      //     {}",
      //            output->name, output->producer->name);

      output->producer = m_nodes[i];

      m_nodes[i]->dependencies.put(output->producer);
      LOG_INFO("added dependency {} to framepass {}", output->name,
               m_nodes[i]->name)
    }
  }
}

void graph_t::connect_node_parents() {
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    ENSURE(m_nodes[i] != nullptr, "found nullptr node");
    m_nodes[i]->parents.init(m_arena, 3);
  }

  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    LOG_INFO("adding dependencies to {}", m_nodes[i]->name)
    LOG_INFO("specified dependencies count {}",
             m_nodes[i]->dependencies.length())

    for (framepass_node_t *dependency : m_nodes[i]->dependencies.span()) {
      ENSURE(dependency != nullptr,
             "found nullptr dependency specified by node {}", m_nodes[i]->name);
      ENSURE(dependency->parents.is_initialized(),
             "tried to access parents but dependency is not initialized");

      LOG_INFO("added parent {} to dependency {}", m_nodes[i]->name,
               dependency->name)

      dependency->parents.put(m_nodes[i]);
    }
  }
}

void graph_t::debug_print() {
  std::println("====================");
  std::println("nodes:");
  for (framepass_node_t *node : m_nodes) {
    std::println("  {}", node->name);
    std::print("    [in: ");
    for (framepass_resource_t *input : node->inputs.span()) {
      std::print("{} ", input->name);
    }
    std::println("]");

    std::print("    [out: ");
    for (framepass_resource_t *output : node->outputs.span()) {
      std::print("{} ", output->name);
    }
    std::println("]");

    std::print("    [depends on: ");
    for (framepass_node_t *edge : node->dependencies.span()) {
      std::print("{} ", edge->name);
    }
    std::println("]");
  }
  std::println("");
  std::println("resources:");
  for (framepass_resource_t *resource : m_resources) {
    // ENSURE(resource->producer != nullptr, "found resource with no producer")
    if (resource->producer == nullptr) {
      std::println("  ({}) producer: 'none', refs: {}", resource->name,
                   resource->reference_count);
    } else {
      std::println("  ({}) producer: {}, refs: {}", resource->name,
                   resource->producer->name, resource->reference_count);
    }
  }

  std::println("====================");
}

void graph_t::debug_graphviz() {
  std::cout << "digraph \"renderpass_dependencies\" {" << std::endl;
  std::cout << "\tnode [shape=box, style=outline, color=black];" << std::endl;
  for (framepass_node_t *node : m_nodes) {
    for (framepass_node_t *parent : node->parents.span()) {
      std::cout << "\t\"" << node->name << "\" -> \"" << parent->name << "\";"
                << std::endl;
    }
  }

  std::cout << "}" << std::endl;
}

void graph_t::init(graph_info_t &info) {
  ENSURE(info.arena != nullptr, "allocator must not be nullptr");
  ENSURE(info.texture_storage != nullptr,
         "texture storage must not be nullptr");
  ENSURE_NOT(info.framepass_infos.empty(), "must have renderpass infos");
  ENSURE_NOT(info.resource_infos.empty(), "must have resource infos");
  m_arena = info.arena;
  m_texture_storage = info.texture_storage;
  m_framepass_infos = info.framepass_infos;
  m_resource_infos = info.resource_infos;
  init_framepass_resources();
  init_framepass_nodes();
  // prune_unused_resources();
  connect_node_dependencies();
  connect_node_parents();
  prune_unused_nodes();
  create_framepass_resources(info);
  create_framepass_renderpasses(info);
  create_framepass_pipelines(info);
}

void graph_t::create_framepass_resources(graph_info_t &info) {
  for (framepass_resource_t *resource : m_resources) {
    if (resource->type == resource_type_t::texture) {
      texture_info_t texture_info;
      texture_info.physical_device = info.physical_device;
      texture_info.device = info.device;
      texture_info.extent.setWidth(resource->texture.extent.width)
          .setHeight(resource->texture.extent.height);
      texture_info.format = resource->texture.format;
      texture_info.tiling = vk::ImageTiling::eOptimal;
      texture_info.aspect_flags = resource->texture.aspect_flags;
      texture_info.property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
      texture_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                           vk::ImageUsageFlagBits::eTransferSrc |
                           vk::ImageUsageFlagBits::eSampled;

      std::span<texture_t> textures =
          m_arena->allocate<texture_t>(frames_in_flight);
      ENSURE_NOT(textures.empty(), "arena full")
      for (texture_t &texture : textures) {
        texture.init(texture_info);
      }

      m_texture_storage->add(resource->name, textures);
      LOG_INFO("created texture resource {} texture size {}/{}", resource->name,
               resource->texture.extent.width, resource->texture.extent.height)
    } else if (resource->type == resource_type_t::attachment) {
      texture_info_t texture_info;
      texture_info.physical_device = info.physical_device;
      texture_info.device = info.device;
      texture_info.extent.setWidth(resource->attachment.extent.width)
          .setHeight(resource->attachment.extent.height);

      texture_info.format = resource->attachment.format;
      texture_info.tiling = vk::ImageTiling::eOptimal;
      texture_info.aspect_flags = resource->attachment.aspect_flags;
      texture_info.property_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
      texture_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                           vk::ImageUsageFlagBits::eTransferSrc |
                           vk::ImageUsageFlagBits::eSampled;

      if (texture_info.aspect_flags & vk::ImageAspectFlagBits::eColor) {
        texture_info.usage |= vk::ImageUsageFlagBits::eColorAttachment;
      } else if (texture_info.aspect_flags & vk::ImageAspectFlagBits::eDepth) {
        texture_info.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
      }

      std::span<texture_t> textures =
          m_arena->allocate<texture_t>(frames_in_flight);
      ENSURE_NOT(textures.empty(), "arena full")
      for (texture_t &texture : textures) {
        texture.init(texture_info);
      }

      m_texture_storage->add(resource->name, textures);

      LOG_INFO("created attachment resource {} texture size {}/{}",
               resource->name, resource->attachment.extent.width,
               resource->attachment.extent.height)

    } else if (resource->type == resource_type_t::memory_buffer) {
      ENSURE(false, "memory_buffer not supported yet")
    }
  }
}

void graph_t::create_framepass_renderpasses(graph_info_t &info) {
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    ENSURE(m_nodes[i] != nullptr, "found nullptr node")

    framepass_resource_t *color_attachment_resource{nullptr};
    framepass_resource_t *depth_attachment_resource{nullptr};

    for (framepass_resource_t *input : m_nodes[i]->inputs.span()) {
      ENSURE(input != nullptr, "found nullptr input for node {}",
             m_nodes[i]->name)

      if (input->type == resource_type_t::attachment) {
        if (input->attachment.type == attachment_type_t::color) {
          ENSURE(color_attachment_resource == nullptr,
                 "only one color attachment is supported for framepass {}",
                 m_nodes[i]->name)
          color_attachment_resource = input;
        } else if (input->attachment.type == attachment_type_t::depth) {
          ENSURE(depth_attachment_resource == nullptr,
                 "only one depth attachment is supported for framepass {}",
                 m_nodes[i]->name)
          depth_attachment_resource = input;
        }
      }
    }

    // TODO: we need to be able to only set depth for shadowpasses etc. in the
    // future
    ENSURE(color_attachment_resource != nullptr &&
               depth_attachment_resource != nullptr,
           "currently both color and depth attachments must be set for "
           "framepass {}",
           m_nodes[i]->name)

    const auto color_attachment =
        vk::AttachmentDescription{}
            .setFlags(vk::AttachmentDescriptionFlags())
            .setFormat(color_attachment_resource->attachment.format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

    const auto depth_attachment =
        vk::AttachmentDescription{}
            .setFlags(vk::AttachmentDescriptionFlags())
            .setFormat(depth_attachment_resource->attachment.format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    const auto color_reference =
        vk::AttachmentReference{}.setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

    const auto depth_reference =
        vk::AttachmentReference{}.setAttachment(1).setLayout(
            vk::ImageLayout::eDepthStencilAttachmentOptimal);

    auto subpass = vk::SubpassDescription{}
                       .setFlags(vk::SubpassDescriptionFlags())
                       .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                       .setInputAttachments({})
                       .setResolveAttachments({})
                       .setColorAttachments(color_reference)
                       .setPDepthStencilAttachment(&depth_reference);

    auto color_depth_dependency =
        vk::SubpassDependency{}
            .setSrcSubpass(vk::SubpassExternal)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                             vk::PipelineStageFlagBits::eEarlyFragmentTests)
            .setSrcAccessMask(vk::AccessFlags())
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                             vk::PipelineStageFlagBits::eEarlyFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                              vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    std::array<vk::AttachmentDescription, 2> attachments{color_attachment,
                                                         depth_attachment};

    std::array<vk::SubpassDependency, 1> dependencies{color_depth_dependency};
    auto renderPassCreateInfo = vk::RenderPassCreateInfo{}
                                    .setFlags(vk::RenderPassCreateFlags())
                                    .setAttachments(attachments)
                                    .setDependencies(dependencies)
                                    .setSubpasses(subpass);

    vk::Result result = info.device.createRenderPass(&renderPassCreateInfo, nullptr, &m_nodes[i]->renderpass);

	ENSURE(result == vk::Result::eSuccess, "could not create renderpass")
	LOG_INFO("Created renderpass for node {}", m_nodes[i]->name)

    std::span<texture_t> color_attachments =
        m_texture_storage->find(color_attachment_resource->name);
    ENSURE_NOT(
        color_attachments.empty(),
        "could not find created color attachments for framebuffers for node {}",
        m_nodes[i]->name)

    std::span<texture_t> depth_attachments =
        m_texture_storage->find(depth_attachment_resource->name);
    ENSURE_NOT(
        depth_attachments.empty(),
        "could not find created depth attachments for framebuffers for node {}",
        m_nodes[i]->name)

    LOG_INFO("node {} color/depth framebuffer {}/{}", m_nodes[i]->name,
             color_attachment_resource->name, depth_attachment_resource->name)

    for (vk::Framebuffer& framebuffer : m_nodes[i]->framebuffers) {
      std::array<vk::ImageView, 2> attachments = {color_attachments[i].view,
                                                  depth_attachments[i].view};
      auto framebuffer_info =
          vk::FramebufferCreateInfo{}
              .setAttachments(attachments)
              .setWidth(color_attachment_resource->attachment.extent.width)
              .setHeight(color_attachment_resource->attachment.extent.height)
              .setLayers(1)
              .setRenderPass(m_nodes[i]->renderpass);
	  
       vk::Result result = info.device.createFramebuffer(&framebuffer_info, nullptr, &framebuffer);
	   ENSURE(result == vk::Result::eSuccess, "could not allocate framebuffers")
	  LOG_INFO("created framebuffer for node {}", m_nodes[i]->name)
    }
  }
}

void graph_t::create_framepass_pipelines(graph_info_t &info) {
  for (std::size_t i = 0; i < m_nodes.size(); i++) {
    ENSURE(m_nodes[i] != nullptr, "found nullptr node")

    auto vertex_source =
        read_spirv_source(m_nodes[i]->vertex_program_path, *m_arena);

    ENSURE_NOT(vertex_source.empty(), "Could not load vertex source: {}",
               m_nodes[i]->vertex_program_path)

    auto fragment_source =
        read_spirv_source(m_nodes[i]->fragment_program_path, *m_arena);
    ENSURE_NOT(fragment_source.empty(), "Could not load fragment source: {}",
               m_nodes[i]->fragment_program_path)

    LOG_INFO("Compiled shader source for geometry pipeline: {} + {}",
             m_nodes[i]->vertex_program_path,
             m_nodes[i]->fragment_program_path);

    auto vertexShaderModuleCreateInfo =
        vk::ShaderModuleCreateInfo{}
            .setFlags(vk::ShaderModuleCreateFlags())
            .setCode(vertex_source);

    auto fragmentShaderModuleCreateInfo =
        vk::ShaderModuleCreateInfo{}
            .setFlags(vk::ShaderModuleCreateFlags())
            .setCode(fragment_source);

    vk::ShaderModule vertex_module =
        info.device.createShaderModule(vertexShaderModuleCreateInfo);
    vk::ShaderModule fragment_module =
        info.device.createShaderModule(fragmentShaderModuleCreateInfo);

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderstage_infos{
        vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setFlags(vk::PipelineShaderStageCreateFlags())
            .setModule(vertex_module)
            .setPName("main"),
        vk::PipelineShaderStageCreateInfo{}
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setFlags(vk::PipelineShaderStageCreateFlags())
            .setModule(fragment_module)
            .setPName("main")};

    auto pipelineDynamicStateCreateInfo = vk::PipelineDynamicStateCreateInfo{};

    std::array<vk::VertexInputBindingDescription,
               1> constexpr vertex_binding_descriptions{
        vk::VertexInputBindingDescription{}
            .setBinding(0)
            .setStride(sizeof(vertex_t))
            .setInputRate(vk::VertexInputRate::eVertex),
    };

    std::array<vk::VertexInputAttributeDescription,
               4> constexpr vertex_attribute_descriptions{
        vk::VertexInputAttributeDescription{}
            .setBinding(0)
            .setLocation(0)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(vertex_t, position)),

        vk::VertexInputAttributeDescription{}
            .setBinding(0)
            .setLocation(1)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(vertex_t, normal)),

        vk::VertexInputAttributeDescription{}
            .setBinding(0)
            .setLocation(2)
            .setFormat(vk::Format::eR32G32B32Sfloat)
            .setOffset(offsetof(vertex_t, color)),

        vk::VertexInputAttributeDescription{}
            .setBinding(0)
            .setLocation(3)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(vertex_t, texcoord)),
    };

    auto pipelineVertexInputStateCreateInfo =
        vk::PipelineVertexInputStateCreateInfo{}
            .setFlags(vk::PipelineVertexInputStateCreateFlags())
            .setVertexBindingDescriptions(vertex_binding_descriptions)
            .setVertexAttributeDescriptions(vertex_attribute_descriptions);

    auto pipelineInputAssemblyStateCreateInfo =
        vk::PipelineInputAssemblyStateCreateInfo{}
            .setFlags(vk::PipelineInputAssemblyStateCreateFlags())
            .setPrimitiveRestartEnable(vk::False)
            .setTopology(vk::PrimitiveTopology::eTriangleList);

    const auto initial_viewport =
        vk::Viewport{}
            .setX(0.0f)
            .setY(0.0f)
            .setWidth(static_cast<float>(m_nodes[i]->extent.width))
            .setHeight(static_cast<float>(m_nodes[i]->extent.height))
            .setMinDepth(0.0f)
            .setMaxDepth(1.0f);

    auto initial_scissor =
        vk::Rect2D{}.setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f));

    auto pipelineViewportStateCreateInfo =
        vk::PipelineViewportStateCreateInfo{}
            .setFlags(vk::PipelineViewportStateCreateFlags())
            .setViewports(initial_viewport)
            .setScissors(initial_scissor);

    auto pipelineRasterizationStateCreateInfo =
        vk::PipelineRasterizationStateCreateInfo{}
            .setFlags(vk::PipelineRasterizationStateCreateFlags())
            .setDepthClampEnable(false)
            .setRasterizerDiscardEnable(false)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setDepthBiasEnable(false)
            .setDepthBiasConstantFactor(0.0f)
            .setDepthBiasClamp(0.0f)
            .setDepthBiasSlopeFactor(0.0f)
            .setLineWidth(1.0f);

    auto pipelineMultisampleStateCreateInfo =
        vk::PipelineMultisampleStateCreateInfo{}
            .setFlags(vk::PipelineMultisampleStateCreateFlags())
            .setSampleShadingEnable(false)
            .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::ColorComponentFlags constexpr colorComponentFlags(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    auto pipelineColorBlendAttachmentState =
        vk::PipelineColorBlendAttachmentState{}
            .setBlendEnable(false)
            .setSrcColorBlendFactor(vk::BlendFactor::eOne)
            .setDstColorBlendFactor(vk::BlendFactor::eZero)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd)
            .setColorWriteMask(colorComponentFlags);

    auto pipelineColorBlendStateCreateInfo =
        vk::PipelineColorBlendStateCreateInfo{}
            .setFlags(vk::PipelineColorBlendStateCreateFlags())
            .setLogicOpEnable(false)
            .setLogicOp(vk::LogicOp::eNoOp)
            .setAttachments(pipelineColorBlendAttachmentState)
            .setBlendConstants({1.0f, 1.0f, 1.0f, 1.0f});

    auto pipelineLayoutCreateInfo =
        vk::PipelineLayoutCreateInfo{}
            .setFlags(vk::PipelineLayoutCreateFlags())
            .setSetLayouts(m_nodes[i]->set_layouts);

    m_nodes[i]->layout =
        info.device.createPipelineLayout(pipelineLayoutCreateInfo);

    auto depth_stencil_state_info = vk::PipelineDepthStencilStateCreateInfo{}
                                        .setDepthTestEnable(true)
                                        .setDepthWriteEnable(true)
                                        .setDepthCompareOp(vk::CompareOp::eLess)
                                        .setDepthBoundsTestEnable(false)
                                        .setMinDepthBounds(0.0f)
                                        .setMaxDepthBounds(1.0f)
                                        .setStencilTestEnable(false);

    auto graphicsPipelineCreateInfo =
        vk::GraphicsPipelineCreateInfo{}
            .setFlags(vk::PipelineCreateFlags())
            .setStages(shaderstage_infos)
            .setPVertexInputState(&pipelineVertexInputStateCreateInfo)
            .setPInputAssemblyState(&pipelineInputAssemblyStateCreateInfo)
            .setPTessellationState(nullptr)
            .setPViewportState(&pipelineViewportStateCreateInfo)
            .setPRasterizationState(&pipelineRasterizationStateCreateInfo)
            .setPMultisampleState(&pipelineMultisampleStateCreateInfo)
            .setPDepthStencilState(&depth_stencil_state_info)
            .setPColorBlendState(&pipelineColorBlendStateCreateInfo)
            .setPDynamicState(&pipelineDynamicStateCreateInfo)
            .setLayout(m_nodes[i]->layout)
            .setRenderPass(m_nodes[i]->renderpass);

    vk::ResultValue<vk::Pipeline> result =
        info.device.createGraphicsPipeline(nullptr, graphicsPipelineCreateInfo);

    ENSURE(result.result == vk::Result::eSuccess, "Could not create graphics pipeline")
    m_nodes[i]->pipeline = result.value;
	LOG_INFO("Created graphics pipeline for node {}", m_nodes[i]->name)
  }
}

void graph_t::record(alex::next_frame_info_t &next_frame) {

  vk::CommandBuffer &commandbuffer = next_frame.presentation_commandbuffer;
  commandbuffer.reset();
  commandbuffer.begin(vk::CommandBufferBeginInfo{});

  vector_t<framepass_node_t *> starters;
  starters.init(m_arena, 3);

  for (framepass_node_t *node : m_nodes) {
    if (node->parents.is_initialized() || node->parents.length() == 0) {
      starters.put(node);
    }
  }

  for (framepass_node_t *node : starters.span()) {
    ENSURE(node != nullptr, "found nullptr node")
    LOG_INFO("Recording node {}", node->name)
    LOG_INFO("    node extent {}/{}", node->extent.width, node->extent.height)
    LOG_INFO("    flightframe {}", next_frame.flightframe)

    const auto render_area =
        vk::Rect2D{}
            .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
            .setExtent(vk::Extent2D(node->extent.width, node->extent.height));

    float constexpr clearcolor = static_cast<float>(0x20) / 255;
    std::array<vk::ClearValue, 2> clearvalues{
        vk::ClearValue{}.setColor({clearcolor, clearcolor, clearcolor, 1.0f}),
        vk::ClearValue{}.setDepthStencil({1.0f, 0}),
    };

	ENSURE(node->renderpass != VK_NULL_HANDLE, "renderpass is nullhandle for node {}", node->name)
	ENSURE(node->framebuffers[next_frame.flightframe] != VK_NULL_HANDLE, "framebuffer is nullhandle for node {}", node->name)


    const auto renderpass_begin_info =
        vk::RenderPassBeginInfo{}
            .setRenderPass(node->renderpass)
            .setFramebuffer(node->framebuffers[next_frame.flightframe])
            .setRenderArea(render_area)
            .setClearValues(clearvalues);

    commandbuffer.beginRenderPass(renderpass_begin_info,
                                  vk::SubpassContents::eInline);

    commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               node->pipeline);

    commandbuffer.endRenderPass();

    auto range = vk::ImageSubresourceRange{}
                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                     .setBaseMipLevel(0)
                     .setLevelCount(1)
                     .setBaseArrayLayer(0)
                     .setLayerCount(1);

    for (framepass_resource_t *output : node->outputs.span()) {
      LOG_INFO("Finding textures for framepass {} output {}", node->name,
               output->name)

      std::span<texture_t> textures = m_texture_storage->find(output->name);
      ENSURE_NOT(textures.empty(),
                 "could not find output textures for framepass output {}",
                 output->name)

      auto barrier = vk::ImageMemoryBarrier{}
                         .setImage(textures[next_frame.flightframe].image)
                         .setSubresourceRange(range)
                         .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                         .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                         .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                         .setDstAccessMask(vk::AccessFlags())
                         .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                         .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }
  }
}

} // namespace alex::graph
