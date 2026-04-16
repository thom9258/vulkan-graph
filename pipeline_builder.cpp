#include "pipeline_builder.hpp"
#include "drawing.hpp"
#include "read_spirv_source.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex::graph {

pipeline_info_t::pipeline_info_t(vk::Device device) : device{device} {}

pipeline_info_t &pipeline_info_t::set_extent(vk::Extent3D extent) {
  this->extent = extent;
  return *this;
}

pipeline_info_t &pipeline_info_t::set_renderpass(vk::RenderPass renderpass) {
  this->renderpass = renderpass;
  return *this;
}

pipeline_info_t &
pipeline_info_t::set_vertex_program_path(std::filesystem::path path) {
  this->vertex_program_path = path;
  return *this;
}

pipeline_info_t &
pipeline_info_t::set_fragment_program_path(std::filesystem::path path) {
  this->fragment_program_path = path;
  return *this;
}

pipeline_info_t &
pipeline_info_t::add_setlayout(vk::DescriptorSetLayout setlayout) {
  setlayouts.push_back(setlayout);
  return *this;
}

pipeline_t::pipeline_t(pipeline_info_t &info, memory::arena &arena) {
  auto vertex_source = read_spirv_source(info.vertex_program_path, arena);

  ENSURE_NOT(vertex_source.empty(), "Could not load vertex source: [{}]",
             info.vertex_program_path.string())

  auto fragment_source = read_spirv_source(info.fragment_program_path, arena);
  ENSURE_NOT(fragment_source.empty(), "Could not load fragment source: [{}]",
             info.fragment_program_path.string())

  LOG_INFO("Compiled shader source for geometry pipeline: {} + {}",
           info.vertex_program_path.string(),
           info.fragment_program_path.string());

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

  std::array<vk::DynamicState, 2> const dynamic_states{
      vk::DynamicState::eViewport, vk::DynamicState::eScissor};

  auto pipelineDynamicStateCreateInfo =
      vk::PipelineDynamicStateCreateInfo{}.setDynamicStates(dynamic_states);

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
          .setWidth(static_cast<float>(info.extent.width))
          .setHeight(static_cast<float>(info.extent.height))
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
          // TODO: next 3 should be exposed
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

  auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo{}
                                      .setFlags(vk::PipelineLayoutCreateFlags())
                                      .setSetLayouts(info.setlayouts);

  layout = info.device.createPipelineLayout(pipelineLayoutCreateInfo);

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
          .setLayout(layout)
          .setRenderPass(info.renderpass);

  vk::ResultValue<vk::Pipeline> result =
      info.device.createGraphicsPipeline(nullptr, graphicsPipelineCreateInfo);

  ENSURE(result.result == vk::Result::eSuccess,
         "Could not create graphics pipeline")
  pipeline = result.value;
}

} // namespace alex::graph
