#include "overlaypass_builder.hpp"
#include <vulkan/vulkan_structs.hpp>

#include <ranges>

namespace alex {

overlaypass_info_t::overlaypass_info_t(vk::Device device) : device{device} {}

overlaypass_info_t &overlaypass_info_t::set_extent(vk::Extent3D extent) {
  this->extent = extent;
  return *this;
}

overlaypass_info_t &overlaypass_info_t::set_color_attachments(
    flightframe_array_t<vk::ImageView> attachments) {
  color_attachments = attachments;
  return *this;
}

overlaypass_info_t &overlaypass_info_t::set_color_format(vk::Format format) {
  color_format = format;
  return *this;
}

overlaypass_info_t &overlaypass_info_t::set_loadop(vk::AttachmentLoadOp op) {
  load_op = op;
  return *this;
}

overlaypass_t::overlaypass_t(overlaypass_info_t &info) {
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

  const auto color_reference =
      vk::AttachmentReference{}.setAttachment(0).setLayout(
          vk::ImageLayout::eColorAttachmentOptimal);

  auto subpass = vk::SubpassDescription{}
                     .setFlags(vk::SubpassDescriptionFlags())
                     .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                     .setInputAttachments({})
                     .setResolveAttachments({})
                     .setColorAttachments(color_reference);

  auto color_dependency =
      vk::SubpassDependency{}
          .setSrcSubpass(vk::SubpassExternal)
          .setDstSubpass(0)
          .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setSrcAccessMask(vk::AccessFlags())
          .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  std::array<vk::AttachmentDescription, 1> attachments{color_attachment};

  std::array<vk::SubpassDependency, 1> dependencies{color_dependency};
  auto renderPassCreateInfo = vk::RenderPassCreateInfo{}
                                  .setFlags(vk::RenderPassCreateFlags())
                                  .setAttachments(attachments)
                                  .setDependencies(dependencies)
                                  .setSubpasses(subpass);

  _renderpass = info.device.createRenderPassUnique(renderPassCreateInfo);

  for (auto [i, framebuffer] : _framebuffers | std::views::enumerate) {

    std::array<vk::ImageView, 1> attachments = {info.color_attachments[i]};
    auto framebuffer_info = vk::FramebufferCreateInfo{}
                                .setAttachments(attachments)
                                .setWidth(info.extent.width)
                                .setHeight(info.extent.height)
                                .setLayers(1)
                                .setRenderPass(_renderpass.get());

    framebuffer = info.device.createFramebufferUnique(framebuffer_info);
  }
}

auto overlaypass_t::renderpass() -> vk::RenderPass { return _renderpass.get(); }

auto overlaypass_t::framebuffer(std::uint32_t frame_in_flight)
    -> vk::Framebuffer {
  return _framebuffers[frame_in_flight].get();
}

auto overlaypass_t::extent() -> vk::Extent3D { return _extent; }

} // namespace alex
