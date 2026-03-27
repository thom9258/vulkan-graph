#include "geometry_pipeline.hpp"
#include "arena.hpp"
#include "ensure.hpp"
#include "drawing.hpp"

#include <filesystem>
#include <fstream>

namespace alex {

[[nodiscard]]
std::span<uint32_t> read_spirv_source(std::filesystem::path path,
									  memory::arena &allocator) {

  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return {};
  }

  size_t const bytecount = static_cast<size_t>(file.tellg());
  constexpr size_t scaling_factor = sizeof(uint32_t) / sizeof(char);
  size_t const read_times = bytecount / scaling_factor;
  auto buffer = allocator.allocate<std::uint32_t>(read_times);
  ENSURE_NOT(buffer.empty(), "allocator full");
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()),
            sizeof(buffer[0]) * buffer.size());
  file.close();
  return buffer;
}

void geometry_pipeline_t::init(geometry_pipeline_info_t &info,
                               memory::arena &allocator) {

  ENSURE(info.core, "core ptr not provided")
  ENSURE(info.renderpass, "renderpass ptr not provided")
  extent = info.extent;

  auto vertex_source = read_spirv_source(info.vertex_program_path, allocator);
  ENSURE_NOT(vertex_source.empty(), "Could not load vertex source: {}",
         info.vertex_program_path.string())

  auto fragment_source = read_spirv_source(info.fragment_program_path, allocator);
  ENSURE_NOT(fragment_source.empty(), "Could not load fragment source: {}",
         info.vertex_program_path.string())

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
      info.core->device.createShaderModule(vertexShaderModuleCreateInfo);
  vk::ShaderModule fragment_module =
      info.core->device.createShaderModule(fragmentShaderModuleCreateInfo);

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

  auto pipelineDynamicStateCreateInfo =
      vk::PipelineDynamicStateCreateInfo{};

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

  std::array<vk::DescriptorSetLayoutBinding,
             1> constexpr frame_uniform_bindings{
      vk::DescriptorSetLayoutBinding{}
          .setStageFlags(vk::ShaderStageFlagBits::eVertex)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setBinding(0)
          .setDescriptorCount(1)};

  const auto uniform_setinfo =
      vk::DescriptorSetLayoutCreateInfo{}
          .setFlags(vk::DescriptorSetLayoutCreateFlags())
          .setBindings(frame_uniform_bindings);

  setlayout =
      info.core->device.createDescriptorSetLayout(uniform_setinfo, nullptr);

  std::array<vk::DescriptorSetLayout, 1> const setlayouts{setlayout};

  auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo{}
                                      .setFlags(vk::PipelineLayoutCreateFlags())
                                      .setSetLayouts(setlayouts);

  layout = info.core->device.createPipelineLayout(pipelineLayoutCreateInfo);

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
          .setRenderPass(info.renderpass->renderpass);

  vk::ResultValue<vk::Pipeline> result =
      info.core->device.createGraphicsPipeline(nullptr,
                                               graphicsPipelineCreateInfo);
  ENSURE(result.result == vk::Result::eSuccess, "Could not create pipeline")
  pipeline = result.value;
}

} // namespace alex
