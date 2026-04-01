#include "renderpass.hpp"

#include <ranges>

namespace alex {

void renderpass_t::init(renderpass_info_t &info) {
  constexpr auto render_format = vk::Format::eR8G8B8A8Srgb;
  constexpr auto depth_format = vk::Format::eD32Sfloat;

  extent = info.extent;

  const auto color_attachment =
      vk::AttachmentDescription{}
          .setFlags(vk::AttachmentDescriptionFlags())
          .setFormat(render_format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
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
          .setFormat(depth_format)
          .setSamples(vk::SampleCountFlagBits::e1)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
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

  // @note we could also specify color and depth dependencies seperately
  //       and put them together in the renderpass as an array
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

  renderpass = info.device.createRenderPass(renderPassCreateInfo);

  for (auto [i, framebuffer] : framebuffers | std::views::enumerate) {

    std::array<vk::ImageView, 2> attachments = {info.attachments[i].color,
                                                info.attachments[i].depth};
    auto framebuffer_info = vk::FramebufferCreateInfo{}
                                .setAttachments(attachments)
                                .setWidth(info.extent.width)
                                .setHeight(info.extent.height)
                                .setLayers(1)
                                .setRenderPass(renderpass);

    framebuffer = info.device.createFramebuffer(framebuffer_info);
  }
}

} // namespace alex
