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

graph_t::graph_t(graph_info_t &info, memory::arena &arena) {
  ENSURE_NOT(info.framepass_infos.empty(), "must have renderpass infos");
  ENSURE_NOT(info.resource_infos.empty(), "must have resource infos");

  init_framepass_resources(info);
  init_framepass_nodes(info);
  connect_node_dependencies(info);
  connect_node_parents();

  // prune_unused_resources();
  // prune_unused_nodes();

  create_framepass_resources(info);
  create_framepass_renderpasses(info);
  create_framepass_pipelines(info, arena);
}

void graph_t::init_framepass_resources(graph_info_t &info) {
  for (resource_info_t &resource_info : info.resource_infos) {
    m_resources.push_back(std::make_unique<framepass_resource_t>());
    m_resources.back()->name = resource_info.name;
    m_resources.back()->resource = resource_info.resource;
  }
}

framepass_node_t *graph_t::find_node(std::string_view name) {
  for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
    if (node != nullptr && node->name == name) {
      return node.get();
    }
  }

  return nullptr;
}

framepass_resource_t *graph_t::find_resource(std::string_view name) {
  for (std::unique_ptr<framepass_resource_t> &resource : m_resources) {
    if (resource != nullptr && resource->name == name) {
      return resource.get();
    }
  }

  return nullptr;
}

void graph_t::init_framepass_nodes(graph_info_t &info) {
  for (framepass_info_t &framepass_info : info.framepass_infos) {
    m_nodes.push_back(std::make_unique<framepass_node_t>());
    m_nodes.back()->name = framepass_info.name;
    m_nodes.back()->record_callback = framepass_info.record_callback;
    m_nodes.back()->extent = framepass_info.extent;

    if (framepass_info.color_attachment.has_value()) {
      m_nodes.back()->color_attachment =
          find_resource(framepass_info.color_attachment.value());
      ENSURE(m_nodes.back()->color_attachment != nullptr,
             "could not find specified color attachment for node {}",
             m_nodes.back()->name)
    }

    if (framepass_info.depth_attachment.has_value()) {
      m_nodes.back()->depth_attachment =
          find_resource(framepass_info.depth_attachment.value());
      ENSURE(m_nodes.back()->depth_attachment != nullptr,
             "could not find specified depth attachment for node {}",
             m_nodes.back()->name)
    }

    m_nodes.back()->vertex_program_path = framepass_info.vertex_program_path;
    m_nodes.back()->fragment_program_path =
        framepass_info.fragment_program_path;
    m_nodes.back()->set_layouts = framepass_info.set_layouts;

    for (framepass_info_t::name_and_usage_t &input : framepass_info.inputs) {
      framepass_resource_t *resource = find_resource(input.name);
      ENSURE(resource != nullptr, "could not find resource")
      m_nodes.back()->inputs.push_back(resource);
      resource->reference_count++;
    }

    for (std::string_view output : framepass_info.outputs) {
      framepass_resource_t *resource = find_resource(output);
      m_nodes.back()->outputs.push_back(resource);
    }
  }
}

void graph_t::prune_unused_resources() {
  decltype(m_resources) used;
  used.reserve((m_resources.size()));

  for (std::unique_ptr<framepass_resource_t> &resource : m_resources) {
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
  // for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
  //   if (node != nullptr && node->inputs.size() > 1 &&
  //       node->outputs.size() < 1) {
  //     used.push_back(std::move(node));
  //   }
  // }
  //
  // m_nodes = std::move(used);
}

void graph_t::connect_node_dependencies(graph_info_t &info) {
  for (framepass_info_t &framepass_info : info.framepass_infos) {
    framepass_node_t *framepass = find_node(framepass_info.name);
    ENSURE(framepass != nullptr, "nullptr node")
    for (std::string &dependency_name : framepass_info.dependencies) {
      framepass_node_t *dependency = find_node(dependency_name);
      ENSURE(dependency != nullptr, "nullptr dependency for node {}",
             framepass->name)
      framepass->dependencies.push_back(dependency);
    }
  }
}

void graph_t::connect_node_parents() {
  for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node");
    for (framepass_node_t *dependency : node->dependencies) {
      ENSURE(dependency != nullptr,
             "found nullptr dependency specified by node {}", node->name);
      dependency->parents.push_back(node.get());
    }
  }
}

void graph_t::print_execution_order(std::ostream &os) {
  std::println("nodes:");
  for (std::unique_ptr<framepass_node_t> &current : m_nodes) {
    std::println("\t{}:", current->name);

    if (!current->inputs.empty()) {
      std::print("\t\tin: [ ");
      for (framepass_resource_t *input : current->inputs) {
        std::print("{} ", input->name);
      }
      std::println("]");
    }

    if (!current->outputs.empty()) {
      std::print("\t\tout: [ ");
      for (framepass_resource_t *output : current->outputs) {
        std::print("{} ", output->name);
      }
      std::println("]");
    }

    if (!current->dependencies.empty()) {
      std::print("\t\tdepends on: [ ");
      for (framepass_node_t *edge : current->dependencies) {
        std::print("{} ", edge->name);
      }
      std::println("]");
    }

    if (current->color_attachment != nullptr) {
      std::println("\t\tcolor attachment: [ {} ]",
                   current->color_attachment->name);
    }

    if (current->depth_attachment != nullptr) {
      std::println("\t\tdepth attachment: [ {} ]",
                   current->depth_attachment->name);
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

  for (std::unique_ptr<framepass_resource_t> &resource : m_resources) {
    ENSURE(resource != nullptr, "found nullptr resource")
    std::string_view node_style = traits::resource_node;
    if (std::holds_alternative<attachment_info_t>(resource->resource)) {
      node_style = traits::attachment_node;
    }

    std::println(os, "\t\"{}\" {}", resource->name, node_style);
  }

  for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")
    std::println(os, "\t\"{}\" {}", node->name, traits::framepass_node);
  }

  for (std::unique_ptr<framepass_node_t> &current : m_nodes) {
    if (current->color_attachment != nullptr) {
      std::println(os, "\t\"{}\" -> \"{}\" {}", current->name,
                   current->color_attachment->name, traits::attachment_arrow);
    }

    if (current->depth_attachment != nullptr) {
      std::println(os, "\t\"{}\" -> \"{}\" {}", current->name,
                   current->depth_attachment->name, traits::attachment_arrow);
    }

    for (framepass_resource_t *input : current->inputs) {
      ENSURE(input != nullptr, "found nullptr input")
      std::println(os, "\t\"{}\" -> \"{}\" {}", input->name, current->name,
                   traits::input_resource_arrow);
    }

    for (framepass_resource_t *output : current->outputs) {
      ENSURE(output != nullptr, "found nullptr output")
      std::println(os, "\t\"{}\" -> \"{}\" {}", current->name, output->name,
                   traits::output_resource_arrow);
    }

    for (framepass_node_t *dependency : current->dependencies) {
      ENSURE(dependency != nullptr, "found nullptr dependency for {}",
             current->name)
      std::println(os, "\t\"{}\" -> \"{}\" {}", dependency->name, current->name,
                   traits::dependency_arrow);
    }
  }

  std::println(os, "}}");
}

void graph_t::create_framepass_resources(graph_info_t &info) {
  for (std::unique_ptr<framepass_resource_t> &resource : m_resources) {
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
  for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")

    struct attachment_t {
      vk::AttachmentDescription description;
      vk::AttachmentReference reference;
    };

    std::uint32_t color_attachment_index = 0;
    std::uint32_t depth_attachment_index = 1;

    std::optional<attachment_t> color_attachment =
        std::invoke([&]() -> std::optional<attachment_t> {
          if (node->color_attachment == nullptr) {
            return std::nullopt;
          }

          auto *attachment =
              std::get_if<attachment_info_t>(&node->color_attachment->resource);
          if (attachment == nullptr) {
            return std::nullopt;
          }

          attachment_t color_attachment;
          color_attachment.description =
              vk::AttachmentDescription{}
                  .setFlags(vk::AttachmentDescriptionFlags())
                  .setFormat(attachment->format)
                  .setSamples(vk::SampleCountFlagBits::e1)
                  .setLoadOp(vk::AttachmentLoadOp::eClear)
                  .setStoreOp(vk::AttachmentStoreOp::eStore)
                  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                  .setInitialLayout(vk::ImageLayout::eUndefined)
                  .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

          color_attachment.reference =
              vk::AttachmentReference{}
                  .setAttachment(color_attachment_index)
                  .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

          return color_attachment;
        });

    std::optional<attachment_t> depth_attachment =
        std::invoke([&]() -> std::optional<attachment_t> {
          if (node->depth_attachment == nullptr) {
            return std::nullopt;
          }

          auto *attachment =
              std::get_if<attachment_info_t>(&node->depth_attachment->resource);
          if (attachment == nullptr) {
            return std::nullopt;
          }

          attachment_t depth_attachment;
          depth_attachment.description =
              vk::AttachmentDescription{}
                  .setFlags(vk::AttachmentDescriptionFlags())
                  .setFormat(attachment->format)
                  .setSamples(vk::SampleCountFlagBits::e1)
                  .setLoadOp(vk::AttachmentLoadOp::eClear)
                  .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                  .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                  .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                  .setInitialLayout(vk::ImageLayout::eUndefined)
                  .setFinalLayout(
                      vk::ImageLayout::eDepthStencilAttachmentOptimal);

          depth_attachment.reference =
              vk::AttachmentReference{}
                  .setAttachment(depth_attachment_index)
                  .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

          return depth_attachment;
        });

    auto subpass = vk::SubpassDescription{}
                       .setFlags(vk::SubpassDescriptionFlags())
                       .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                       .setInputAttachments({})
                       .setResolveAttachments({});

    if (color_attachment.has_value()) {
      subpass.setColorAttachments(color_attachment->reference);
    }
    if (depth_attachment.has_value()) {
      subpass.setPDepthStencilAttachment(&depth_attachment->reference);
    }

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

    std::array<vk::SubpassDependency, 1> dependencies{color_depth_dependency};

    std::vector<vk::AttachmentDescription> attachments;
    if (color_attachment.has_value()) {
      attachments.push_back(color_attachment->description);
    }
    if (depth_attachment.has_value()) {
      attachments.push_back(depth_attachment->description);
    }

    auto renderPassCreateInfo = vk::RenderPassCreateInfo{}
                                    .setFlags(vk::RenderPassCreateFlags())
                                    .setAttachments(attachments)
                                    .setDependencies(dependencies)
                                    .setSubpasses(subpass);

    vk::Result result = info.device.createRenderPass(
        &renderPassCreateInfo, nullptr, &node->renderpass);

    ENSURE(result == vk::Result::eSuccess, "could not create renderpass")
    LOG_INFO("Created renderpass for node {}", node->name)

    std::span<texture_t> color_textures =
        std::invoke([&]() -> std::span<texture_t> {
          if (node->color_attachment == nullptr) {
            return {};
          }

          return m_texture_storage.find(node->color_attachment->name);
        });

    std::span<texture_t> depth_textures =
        std::invoke([&]() -> std::span<texture_t> {
          if (node->depth_attachment == nullptr) {
            return {};
          }

          return m_texture_storage.find(node->depth_attachment->name);
        });

    for (auto [i, framebuffer] : node->framebuffers | std::views::enumerate) {
      std::vector<vk::ImageView> views;
      if (!color_textures.empty()) {
        views.push_back(color_textures[i].view);
      }

      if (!depth_textures.empty()) {
        views.push_back(depth_textures[i].view);
      }

      auto framebuffer_info = vk::FramebufferCreateInfo{}
                                  .setAttachments(views)
                                  .setWidth(node->extent.width)
                                  .setHeight(node->extent.height)
                                  .setLayers(1)
                                  .setRenderPass(node->renderpass);

      vk::Result result = info.device.createFramebuffer(&framebuffer_info,
                                                        nullptr, &framebuffer);
      ENSURE(result == vk::Result::eSuccess, "could not allocate framebuffers")
      LOG_INFO("created framebuffer for node {}", node->name)
    }
  }
}

void graph_t::create_framepass_pipelines(graph_info_t &info,
                                         memory::arena &arena) {
  for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")

    auto vertex_source = read_spirv_source(node->vertex_program_path, arena);

    ENSURE_NOT(vertex_source.empty(), "Could not load vertex source: {}",
               node->vertex_program_path.string())

    auto fragment_source =
        read_spirv_source(node->fragment_program_path, arena);
    ENSURE_NOT(fragment_source.empty(), "Could not load fragment source: {}",
               node->fragment_program_path.string())

    LOG_INFO("Compiled shader source for geometry pipeline: {} + {}",
             node->vertex_program_path.string(),
             node->fragment_program_path.string());

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
            .setWidth(static_cast<float>(node->extent.width))
            .setHeight(static_cast<float>(node->extent.height))
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
            .setSetLayouts(node->set_layouts);

    node->layout = info.device.createPipelineLayout(pipelineLayoutCreateInfo);

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
            .setLayout(node->layout)
            .setRenderPass(node->renderpass);

    vk::ResultValue<vk::Pipeline> result =
        info.device.createGraphicsPipeline(nullptr, graphicsPipelineCreateInfo);

    ENSURE(result.result == vk::Result::eSuccess,
           "Could not create graphics pipeline")
    node->pipeline = result.value;
    LOG_INFO("Created graphics pipeline for node {}", node->name)
  }
}

void graph_t::record(alex::next_frame_info_t &next_frame) {

  vk::CommandBuffer &commandbuffer = next_frame.presentation_commandbuffer;
  commandbuffer.reset();
  commandbuffer.begin(vk::CommandBufferBeginInfo{});

  for (std::unique_ptr<framepass_node_t> &node : m_nodes) {
    ENSURE(node != nullptr, "found nullptr node")

#if 0    
    for (framepass_resource_t *input : node->inputs) {
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

    const auto render_area =
        vk::Rect2D{}
            .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
            .setExtent(vk::Extent2D(node->extent.width, node->extent.height));

    float constexpr clearcolor = static_cast<float>(0x20) / 255;
    std::array<vk::ClearValue, 2> clearvalues{
        vk::ClearValue{}.setColor({clearcolor, clearcolor, clearcolor, 1.0f}),
        vk::ClearValue{}.setDepthStencil({1.0f, 0}),
    };

    ENSURE(node->renderpass != VK_NULL_HANDLE,
           "renderpass is nullhandle for node {}", node->name)
    ENSURE(node->framebuffers[next_frame.flightframe] != VK_NULL_HANDLE,
           "framebuffer is nullhandle for node {}", node->name)

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

	renderpass_record_info_t record_info;
	record_info.commandbuffer = commandbuffer;
	record_info.flightframe = next_frame.flightframe;

	node->record_callback(record_info);

    commandbuffer.endRenderPass();
  }
}

} // namespace alex::graph
