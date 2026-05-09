#include "presentation_context.hpp"
#include "core.hpp"
#include "ensure.hpp"
#include "log.hpp"
#include "presentation_context.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <algorithm>
#include <array>
#include <ranges>

namespace alex {

static constexpr auto get_best_swapchain_surface_format(
    const std::span<vk::SurfaceFormatKHR> availables) -> vk::SurfaceFormatKHR {
  for (const auto &available : availables) {
    if (available.format == vk::Format::eB8G8R8A8Srgb &&
        available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return available;
    }
  }

  return availables.front();
}

presenter_t::presenter_t(presenter_info_t &info) {
  window_extent = info.window_extent;

  vk::SurfaceCapabilitiesKHR window_capabilities =
      info.physical_device.getSurfaceCapabilitiesKHR(info.surface);

  const bool window_size_is_undefined =
      window_capabilities.currentExtent.width ==
      std::numeric_limits<uint32_t>::max();

  if (window_size_is_undefined) {
    LOG_CRITICAL("Provided window surface has a undefined size!");
    return;
  }

  std::uint32_t surface_format_count{0};
  vk::Result result = info.physical_device.getSurfaceFormatsKHR(
      info.surface, &surface_format_count, nullptr);
  ENSURE(result == vk::Result::eSuccess, "could not get surfac format count!")
  std::vector<vk::SurfaceFormatKHR> available_surface_formats(
      surface_format_count);

  result = info.physical_device.getSurfaceFormatsKHR(
      info.surface, &surface_format_count, available_surface_formats.data());
  ENSURE(result == vk::Result::eSuccess, "could not get surfac formats!")

  format = get_best_swapchain_surface_format(available_surface_formats);

  vk::PresentModeKHR const present_mode = (info.enable_vsync)
                                              ? vk::PresentModeKHR::eFifo
                                              : vk::PresentModeKHR::eImmediate;

  const auto supports_identity =
      static_cast<bool>(window_capabilities.supportedTransforms &
                        vk::SurfaceTransformFlagBitsKHR::eIdentity);

  const vk::SurfaceTransformFlagBitsKHR preTransform =
      (supports_identity) ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                          : window_capabilities.currentTransform;

  // this spaghetti determines the best compositealpha flags....
  vk::CompositeAlphaFlagBitsKHR compositeAlpha =
      (window_capabilities.supportedCompositeAlpha &
       vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
          ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
      : (window_capabilities.supportedCompositeAlpha &
         vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
          ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
      : (window_capabilities.supportedCompositeAlpha &
         vk::CompositeAlphaFlagBitsKHR::eInherit)
          ? vk::CompositeAlphaFlagBitsKHR::eInherit
          : vk::CompositeAlphaFlagBitsKHR::eOpaque;

  std::uint32_t image_count{0};
  if (window_capabilities.maxImageCount > 0) {
    image_count =
        std::clamp(frames_in_flight, window_capabilities.minImageCount,
                   window_capabilities.maxImageCount);
  } else {
    image_count = window_capabilities.minImageCount;
  }

  auto swapChainCreateInfo =
      vk::SwapchainCreateInfoKHR{}
          .setFlags(vk::SwapchainCreateFlagsKHR())
          .setSurface(info.surface)
          .setMinImageCount(image_count)
          .setImageFormat(format.format)
          .setImageColorSpace(format.colorSpace)
          .setImageExtent(info.window_extent)
          .setImageArrayLayers(1)
          .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment |
                         vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eTransferDst |
                         vk::ImageUsageFlagBits::eSampled)
          .setClipped(true)
          .setPreTransform(preTransform)
          .setCompositeAlpha(compositeAlpha)
          .setPresentMode(present_mode)
          .setImageSharingMode(vk::SharingMode::eExclusive);

  _swapchain =
      info.device.createSwapchainKHRUnique(swapChainCreateInfo, nullptr);

  {
    std::uint32_t image_count{0};
    vk::Result result = info.device.getSwapchainImagesKHR(
        _swapchain.get(), &image_count, nullptr);

    ENSURE(result == vk::Result::eSuccess,
           "could not get swapchain image count!")

    _images.resize(image_count);
    result = info.device.getSwapchainImagesKHR(_swapchain.get(), &image_count,
                                               _images.data());
    ENSURE(result == vk::Result::eSuccess, "could not get surfac formats!")
  }

  {
    _imageviews.resize(_images.size());
    auto subresourceRange = vk::ImageSubresourceRange{}
                                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                .setBaseMipLevel(0)
                                .setLevelCount(1)
                                .setBaseArrayLayer(0)
                                .setLayerCount(1);

    auto componentMapping = vk::ComponentMapping{}
                                .setR(vk::ComponentSwizzle::eIdentity)
                                .setG(vk::ComponentSwizzle::eIdentity)
                                .setB(vk::ComponentSwizzle::eIdentity)
                                .setA(vk::ComponentSwizzle::eIdentity);

    auto imageViewCreateInfo = vk::ImageViewCreateInfo{}
                                   .setSubresourceRange(subresourceRange)
                                   .setViewType(vk::ImageViewType::e2D)
                                   .setFormat(format.format)
                                   .setComponents(componentMapping);

    for (auto [i, view] : _imageviews | std::views::enumerate) {
      imageViewCreateInfo.setImage(_images[i]);
      view = info.device.createImageViewUnique(imageViewCreateInfo);
    }
  }

  {
    auto commandbuffer_alloc_info =
        vk::CommandBufferAllocateInfo{}
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandPool(info.commandpool)
            .setCommandBufferCount(frames_in_flight);

    auto commandbuffers =
        info.device.allocateCommandBuffersUnique(commandbuffer_alloc_info);

    for (auto [i, commandbuffer] : commandbuffers | std::views::enumerate) {
      _sync.commandbuffers[i] = std::move(commandbuffer);
    }
  }
  {
    _sync.render_finished.resize(_images.size());
    auto create_info = vk::SemaphoreCreateInfo{};
    for (vk::UniqueSemaphore &semaphore : _sync.render_finished) {
      semaphore = info.device.createSemaphoreUnique(create_info, nullptr);
    }
  }
  {
    auto create_info = vk::SemaphoreCreateInfo{};
    for (vk::UniqueSemaphore &semaphore : _sync.image_available) {
      semaphore = info.device.createSemaphoreUnique(create_info, nullptr);
    }
  }
  // Note: fences starts off as signaled because we preemptively need to wait
  // for the first frame fence
  auto fence_create_info =
      vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled);

  for (vk::UniqueFence &fence : _sync.in_flight) {
    fence = info.device.createFenceUnique(fence_create_info, nullptr);
  }
}

next_frame_info_t presenter_t::wait_for_next_frame(vk::Device device) {
  std::array<vk::Fence, 1> const fences{
      _sync.in_flight[_sync.flightframe].get()};
  const auto max_wait = std::numeric_limits<unsigned int>::max();
  vk::Result wait_result = device.waitForFences(fences, true, max_wait);
  ENSURE(wait_result == vk::Result::eSuccess, "Could not wait for swapchain")
  device.resetFences(fences);

  const auto max_acquire_wait = std::numeric_limits<uint64_t>::max();
  vk::ResultValue<uint32_t> result = device.acquireNextImageKHR(
      _swapchain.get(), max_acquire_wait,
      _sync.image_available[_sync.flightframe].get(), nullptr);

  ENSURE(result.result == vk::Result::eSuccess,
         "Could not acquire next image from swapchain")
  _sync.image_index = result.value;

  vk::CommandBuffer commandbuffer =
      _sync.commandbuffers[_sync.flightframe].get();
  commandbuffer.reset();

  next_frame_info_t next_frame_info;
  next_frame_info.presentation_commandbuffer = commandbuffer;
  next_frame_info.flightframe = _sync.flightframe;
  return next_frame_info;
}

// TODO: this goes into "presentation" job
void presenter_t::present(presentation_info_t &info) {

  {
    auto range = vk::ImageSubresourceRange{}
                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                     .setBaseMipLevel(0)
                     .setLevelCount(1)
                     .setBaseArrayLayer(0)
                     .setLayerCount(1);

    auto barrier = vk::ImageMemoryBarrier{}
                       .setImage(_images[_sync.image_index])
                       .setSubresourceRange(range)
                       .setOldLayout(vk::ImageLayout::eUndefined)
                       .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                       .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                       .setDstAccessMask(vk::AccessFlags())
                       .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    info.commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                       vk::PipelineStageFlagBits::eTransfer,
                                       vk::DependencyFlags(), nullptr, nullptr,
                                       barrier);
  }

  {
    auto range = vk::ImageSubresourceRange{}
                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                     .setBaseMipLevel(0)
                     .setLevelCount(1)
                     .setBaseArrayLayer(0)
                     .setLayerCount(1);

    auto barrier = vk::ImageMemoryBarrier{}
                       .setImage(info.image)
                       .setSubresourceRange(range)
                       .setOldLayout(info.layout)
                       .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                       .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                       .setDstAccessMask(vk::AccessFlags())
                       .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                       .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    info.commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                       vk::PipelineStageFlagBits::eTransfer,
                                       vk::DependencyFlags(), nullptr, nullptr,
                                       barrier);
  }

  auto src_subresource = vk::ImageSubresourceLayers{}
                             .setAspectMask(vk::ImageAspectFlagBits::eColor)
                             .setBaseArrayLayer(0)
                             .setLayerCount(1)
                             .setMipLevel(0);
  const std::array<vk::Offset3D, 2> src_offsets{info.source_offset_start,
                                                info.source_offset_end};

  auto dst_subresource = vk::ImageSubresourceLayers{}
                             .setAspectMask(vk::ImageAspectFlagBits::eColor)
                             .setBaseArrayLayer(0)
                             .setLayerCount(1)
                             .setMipLevel(0);

  const std::array<vk::Offset3D, 2> dst_offsets{info.destination_offset_start,
                                                info.destination_offset_end};

  auto image_blit = vk::ImageBlit{}
                        .setSrcOffsets(src_offsets)
                        .setSrcSubresource(src_subresource)
                        .setDstOffsets(dst_offsets)
                        .setDstSubresource(dst_subresource);

  info.commandbuffer.blitImage(info.image, vk::ImageLayout::eTransferSrcOptimal,
                               _images[_sync.image_index],
                               vk::ImageLayout::eTransferDstOptimal, image_blit,
                               info.blit_filter);

  // Here we transfer the color attachment of the renderpass into
  // transfersrc so we can blit it to the swapchain
  auto to_present_range = vk::ImageSubresourceRange{}
                              .setAspectMask(vk::ImageAspectFlagBits::eColor)
                              .setBaseMipLevel(0)
                              .setLevelCount(1)
                              .setBaseArrayLayer(0)
                              .setLayerCount(1);

  auto to_present_barrier =
      vk::ImageMemoryBarrier{}
          .setImage(_images[_sync.image_index])
          .setSubresourceRange(to_present_range)
          .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
          .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
          .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
          .setDstAccessMask(vk::AccessFlags())
          .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
          .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

  info.commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                     vk::PipelineStageFlagBits::eTransfer,
                                     vk::DependencyFlags(), nullptr, nullptr,
                                     to_present_barrier);

  info.commandbuffer.end();
  std::array<vk::Semaphore, 1> const wait_semaphores{
      _sync.image_available[_sync.flightframe].get()};

  std::array<vk::Semaphore, 1> const signal_semaphores{
      _sync.render_finished[_sync.image_index].get()};

  std::array<vk::PipelineStageFlags, 1> const wait_dst_stage_masks{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  auto submit_info = vk::SubmitInfo{}
                         .setWaitSemaphores(wait_semaphores)
                         .setWaitDstStageMask(wait_dst_stage_masks)
                         .setCommandBuffers({info.commandbuffer})
                         .setSignalSemaphores(signal_semaphores);

  info.queue.submit(submit_info, _sync.in_flight[_sync.flightframe].get());

  std::array<vk::SwapchainKHR, 1> const swapchains{_swapchain.get()};
  std::array<uint32_t, 1> const image_indices{_sync.image_index};
  auto present_info = vk::PresentInfoKHR{}
                          .setWaitSemaphores(signal_semaphores)
                          .setSwapchains(swapchains)
                          .setImageIndices(image_indices);

  vk::Result result = info.queue.presentKHR(present_info);
  ENSURE(result == vk::Result::eSuccess, "could not present frame")
  _sync.flightframe = (_sync.flightframe + 1) % frames_in_flight;
}

} // namespace alex
