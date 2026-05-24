#include "pipeline_builder.hpp"
#include "log.hpp"
#include "read_spirv_source.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

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

pipeline_info_t &pipeline_info_t::add_vertex_input_binding(
    vk::VertexInputBindingDescription binding) {
  vertex_bindings.push_back(binding);
  return *this;
}

pipeline_info_t &pipeline_info_t::add_vertex_input_attribute(
    vk::VertexInputAttributeDescription attribute) {
  vertex_attributes.push_back(attribute);
  return *this;
}

pipeline_t::pipeline_t(pipeline_info_t &info, memory::arena &arena) {
  auto vertex_source = read_spirv_source(info.vertex_program_path, arena);

  ALEX_ERROR_IF(vertex_source.empty(), "Could not load vertex source: [{}]",
                info.vertex_program_path.string())

  auto fragment_source = read_spirv_source(info.fragment_program_path, arena);
  ALEX_ERROR_IF(fragment_source.empty(), "Could not load fragment source: [{}]",
                info.fragment_program_path.string())

  auto vertexShaderModuleCreateInfo =
      vk::ShaderModuleCreateInfo{}
          .setFlags(vk::ShaderModuleCreateFlags())
          .setCode(vertex_source);

  auto fragmentShaderModuleCreateInfo =
      vk::ShaderModuleCreateInfo{}
          .setFlags(vk::ShaderModuleCreateFlags())
          .setCode(fragment_source);

  vk::UniqueShaderModule vertex_module =
      info.device.createShaderModuleUnique(vertexShaderModuleCreateInfo);
  vk::UniqueShaderModule fragment_module =
      info.device.createShaderModuleUnique(fragmentShaderModuleCreateInfo);

  std::array<vk::PipelineShaderStageCreateInfo, 2> shaderstage_infos{
      vk::PipelineShaderStageCreateInfo{}
          .setStage(vk::ShaderStageFlagBits::eVertex)
          .setFlags(vk::PipelineShaderStageCreateFlags())
          .setModule(vertex_module.get())
          .setPName("main"),
      vk::PipelineShaderStageCreateInfo{}
          .setStage(vk::ShaderStageFlagBits::eFragment)
          .setFlags(vk::PipelineShaderStageCreateFlags())
          .setModule(fragment_module.get())
          .setPName("main")};

  std::array<vk::DynamicState, 2> const dynamic_states{
      vk::DynamicState::eViewport, vk::DynamicState::eScissor};

  auto pipelineDynamicStateCreateInfo =
      vk::PipelineDynamicStateCreateInfo{}.setDynamicStates(dynamic_states);

  auto pipelineVertexInputStateCreateInfo =
      vk::PipelineVertexInputStateCreateInfo{}
          .setFlags(vk::PipelineVertexInputStateCreateFlags())
          .setVertexBindingDescriptions(info.vertex_bindings)
          .setVertexAttributeDescriptions(info.vertex_attributes);

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
          .setPolygonMode(info.polygon_mode)
          .setCullMode(info.cull_mode)
          .setFrontFace(info.front_face)
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

  _layout = info.device.createPipelineLayoutUnique(pipelineLayoutCreateInfo);

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
          .setLayout(_layout.get())
          .setRenderPass(info.renderpass);

  vk::ResultValue<vk::UniquePipeline> result =
      info.device.createGraphicsPipelineUnique(nullptr,
                                               graphicsPipelineCreateInfo);

  ALEX_ERROR_IF(result.result != vk::Result::eSuccess,
                "Could not create graphics pipeline")
  _pipeline = std::move(result.value);
}

auto pipeline_t::layout() -> vk::PipelineLayout { return _layout.get(); }
auto pipeline_t::pipeline() -> vk::Pipeline { return _pipeline.get(); }

} // namespace alex
