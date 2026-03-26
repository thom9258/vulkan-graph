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

vk::SurfaceFormatKHR get_best_swapchain_surface_format(
    const std::span<vk::SurfaceFormatKHR> availables) {
  for (const auto &available : availables) {
    if (available.format == vk::Format::eB8G8R8A8Srgb &&
        available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return available;
    }
  }

  return availables.front();
}

void presentation_context_t::init(presentation_context_info_t &info,
                                  memory::arena &allocator) {
  ENSURE(info.core, "Core ptr not provided")
  vk::SurfaceCapabilitiesKHR window_capabilities =
      info.core->physical_device.getSurfaceCapabilitiesKHR(info.surface);

  const bool window_size_is_undefined =
      window_capabilities.currentExtent.width ==
      std::numeric_limits<uint32_t>::max();

  if (window_size_is_undefined) {
    LOG_CRITICAL("Provided window surface has a undefined size!");
    return;
  }

  std::uint32_t surface_format_count{0};
  vk::Result result = info.core->physical_device.getSurfaceFormatsKHR(
      info.surface, &surface_format_count, nullptr);
  ENSURE(result == vk::Result::eSuccess, "could not get surfac format count!")
  auto available_surface_formats =
      allocator.allocate<vk::SurfaceFormatKHR>(surface_format_count);
  ENSURE_NOT(available_surface_formats.empty(), "out of memory!")
  result = info.core->physical_device.getSurfaceFormatsKHR(
      info.surface, &surface_format_count, available_surface_formats.data());
  ENSURE(result == vk::Result::eSuccess, "could not get surfac formats!")

  format = get_best_swapchain_surface_format(available_surface_formats);
  LOG_INFO("Swapchain format: {}", vk::to_string(format.format));

  vk::PresentModeKHR constexpr present_mode = vk::PresentModeKHR::eFifo;

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

  LOG_INFO("Swapchain image count: {}", image_count);

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

  swapchain =
      info.core->device.createSwapchainKHR(swapChainCreateInfo, nullptr);

  {
    std::uint32_t image_count{0};
    vk::Result result = info.core->device.getSwapchainImagesKHR(
        swapchain, &image_count, nullptr);

    ENSURE(result == vk::Result::eSuccess,
           "could not get swapchain image count!")
    auto allocated_images = allocator.allocate<vk::Image>(image_count);
    ENSURE_NOT(allocated_images.empty(), "out of memory!")

    result = info.core->device.getSwapchainImagesKHR(swapchain, &image_count,
                                                     allocated_images.data());
    ENSURE(result == vk::Result::eSuccess, "could not get surfac formats!")
    images = allocated_images;
  }

  {
    auto allocated_imageviews =
        allocator.allocate<vk::ImageView>(images.size());
    ENSURE_NOT(allocated_imageviews.empty(), "out of memory!")
    imageviews = allocated_imageviews;
  }
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

  for (auto [i, view] : imageviews | std::views::enumerate) {
    imageViewCreateInfo.setImage(images[i]);
    imageviews[i] = info.core->device.createImageView(imageViewCreateInfo);
  }

  auto commandbuffer_alloc_info =
      vk::CommandBufferAllocateInfo{}
          .setLevel(vk::CommandBufferLevel::ePrimary)
          .setCommandPool(info.core->commandpool)
          .setCommandBufferCount(frames_in_flight);

  result = info.core->device.allocateCommandBuffers(&commandbuffer_alloc_info,
                                                    sync.commandbuffers.data());
  ENSURE(result == vk::Result::eSuccess, "could not allocate commandbuffers");

  auto semaphore_create_info = vk::SemaphoreCreateInfo{};

  for (vk::Semaphore &semaphore : sync.image_available) {
    result = info.core->device.createSemaphore(&semaphore_create_info, nullptr,
                                               &semaphore);
    ENSURE(result == vk::Result::eSuccess, "could not allocate semaphore");
  }

  for (vk::Semaphore &semaphore : sync.render_finished) {
    result = info.core->device.createSemaphore(&semaphore_create_info, nullptr,
                                               &semaphore);
    ENSURE(result == vk::Result::eSuccess, "could not allocate semaphore");
  }

  // Note: fences starts off as signaled because we preemptively need to wait
  // for the first frame fence
  auto fence_create_info =
      vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled);

  for (vk::Fence &fence : sync.in_flight) {
    result = info.core->device.createFence(&fence_create_info, nullptr, &fence);
    ENSURE(result == vk::Result::eSuccess, "could not allocate fence");
  }
}

next_frame_info_t
presentation_context_t::wait_for_next_frame(vk::Device device) {
  // TODO: this goes into a "swapchain wait for next frame" job
  std::array<vk::Fence, 1> const fences{sync.in_flight[sync.flightframe]};
  const auto max_wait = std::numeric_limits<unsigned int>::max();
  vk::Result wait_result = device.waitForFences(fences, true, max_wait);
  ENSURE(wait_result == vk::Result::eSuccess, "Could not wait for swapchain")
  device.resetFences(fences);

  const auto max_acquire_wait = std::numeric_limits<uint64_t>::max();
  vk::ResultValue<uint32_t> result = device.acquireNextImageKHR(
      swapchain, max_acquire_wait, sync.image_available[sync.flightframe],
      nullptr);

  ENSURE(result.result == vk::Result::eSuccess,
         "Could not acquire next image from swapchain")
  sync.image_index = result.value;

  vk::CommandBuffer &commandbuffer = sync.commandbuffers[sync.flightframe];
  commandbuffer.reset();

  next_frame_info_t next_frame_info;
  next_frame_info.presentation_commandbuffer = commandbuffer;
  next_frame_info.flightframe = sync.flightframe;
  next_frame_info.swapchain_frameindex = sync.image_index;
  return next_frame_info;
}

// TODO: this goes into "presentation" job
void presentation_context_t::present(presentation_info_t &info) {

  auto range = vk::ImageSubresourceRange{}
                   .setAspectMask(vk::ImageAspectFlagBits::eColor)
                   .setBaseMipLevel(0)
                   .setLevelCount(1)
                   .setBaseArrayLayer(0)
                   .setLayerCount(1);

  auto barrier = vk::ImageMemoryBarrier{}
                     .setImage(images[sync.image_index])
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
                               images[sync.image_index],
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
            .setImage(images[sync.image_index])
            .setSubresourceRange(to_present_range)
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
            .setDstAccessMask(vk::AccessFlags())
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    info.commandbuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr,
        nullptr, to_present_barrier);


  info.commandbuffer.end();
  std::array<vk::Semaphore, 1> const wait_semaphores{
      sync.image_available[sync.flightframe]};

  std::array<vk::Semaphore, 1> const signal_semaphores{
      sync.render_finished[sync.flightframe]};

  std::array<vk::PipelineStageFlags, 1> const wait_dst_stage_masks{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  auto submit_info = vk::SubmitInfo{}
                         .setWaitSemaphores(wait_semaphores)
                         .setWaitDstStageMask(wait_dst_stage_masks)
                         .setCommandBuffers({info.commandbuffer})
                         .setSignalSemaphores(signal_semaphores);

  info.queue.submit(submit_info, sync.in_flight[sync.flightframe]);

  std::array<vk::SwapchainKHR, 1> const swapchains{swapchain};
  std::array<uint32_t, 1> const image_indices{sync.image_index};
  auto present_info = vk::PresentInfoKHR{}
                          .setWaitSemaphores(signal_semaphores)
                          .setSwapchains(swapchains)
                          .setImageIndices(image_indices);

  vk::Result result = info.queue.presentKHR(present_info);
  ENSURE(result == vk::Result::eSuccess, "could not present frame")
  sync.flightframe = (sync.flightframe + 1) % frames_in_flight;
}

} // namespace alex
