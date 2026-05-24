#include "geometrypass_builder.hpp"
#include <vulkan/vulkan_structs.hpp>

#include <ranges>

namespace alex {

geometrypass_info_t::geometrypass_info_t(vk::Device device) : device{device} {}

geometrypass_info_t &geometrypass_info_t::set_extent(vk::Extent3D extent) {
  this->extent = extent;
  return *this;
}

geometrypass_info_t &geometrypass_info_t::set_color_attachments(
    flightframe_array_t<vk::ImageView> attachments) {
  color_attachments = attachments;
  return *this;
}

geometrypass_info_t &geometrypass_info_t::set_depth_attachments(
    flightframe_array_t<vk::ImageView> attachments) {
  depth_attachments = attachments;
  return *this;
}

geometrypass_info_t &geometrypass_info_t::set_color_format(vk::Format format) {
  color_format = format;
  return *this;
}

geometrypass_info_t &geometrypass_info_t::set_depth_format(vk::Format format) {
  depth_format = format;
  return *this;
}

geometrypass_info_t &
geometrypass_info_t::set_color_clearvalue(float r, float g, float b, float a) {
  clearvalues[0] = vk::ClearValue{}.setColor({r, g, b, a});
  return *this;
}

geometrypass_info_t &geometrypass_info_t::set_depth_clearvalue(float d) {
  clearvalues[1] = vk::ClearValue{}.setDepthStencil({d, 0});
  return *this;
}

geometrypass_info_t &geometrypass_info_t::set_loadop(vk::AttachmentLoadOp op) {
  load_op = op;
  return *this;
}

geometrypass_t::geometrypass_t(geometrypass_info_t &info) {
  _clearvalues = info.clearvalues;
  _extent = info.extent;

  const auto color_attachment =
      vk::AttachmentDescription{}
          .setFlags(vk::AttachmentDescriptionFlags())
          .setFormat(info.color_format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(info.load_op)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          // NOTE these are important, as they determine the layout of the image
          // before and after the renderpass
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

  const auto depth_attachment =
      vk::AttachmentDescription{}
          .setFlags(vk::AttachmentDescriptionFlags())
          .setFormat(info.depth_format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(info.load_op)
          .setStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          // NOTE these are important, as they determine the layout of the image
          // before and after the renderpass
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

  _renderpass = info.device.createRenderPassUnique(renderPassCreateInfo);
  for (auto [i, framebuffer] : _framebuffers | std::views::enumerate) {

    std::array<vk::ImageView, 2> attachments = {info.color_attachments[i],
                                                info.depth_attachments[i]};
    auto framebuffer_info = vk::FramebufferCreateInfo{}
                                .setAttachments(attachments)
                                .setWidth(info.extent.width)
                                .setHeight(info.extent.height)
                                .setLayers(1)
                                .setRenderPass(_renderpass.get());

    framebuffer = info.device.createFramebufferUnique(framebuffer_info);
  }
}

auto geometrypass_t::renderpass() -> vk::RenderPass {
  return _renderpass.get();
}

auto geometrypass_t::framebuffer(std::uint32_t frame_in_flight)
    -> vk::Framebuffer {
  return _framebuffers[frame_in_flight].get();
}

auto geometrypass_t::extent() -> vk::Extent3D { return _extent; }

auto geometrypass_t::clearvalues() -> std::array<vk::ClearValue, 2> {
  return _clearvalues;
}

} // namespace alex
