#include "imgui_context.hpp"

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>

#include <print>

namespace game {

static void imgui_check_vk_result(VkResult err) {
  if (err == VK_SUCCESS) {
    return;
  }

  std::println("[ImGui vulkan] Error: VkResult = {}",
               vk::to_string(vk::Result(err)));
}

auto imgui_context_t::new_frame() -> void {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

auto imgui_context_t::process_event(const SDL_Event *e) -> bool {
  return ImGui_ImplSDL2_ProcessEvent(e);
}

auto imgui_context_t::render_draw_data(ImDrawData *draw_data,
                                       VkCommandBuffer command_buffer,
                                       VkPipeline pipeline) -> void {
  ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer, pipeline);
}

imgui_context_t::imgui_context_t(imgui_context_info_t &info) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  ImGui::GetIO().ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
  ImGui::StyleColorsDark();

  float main_scale =
      ImGui_ImplSDL2_GetContentScaleForDisplay(0) * info.ui_scale;
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);
  style.FontScaleDpi = main_scale;

  auto const pool_sizes = std::vector<vk::DescriptorPoolSize>{
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eSampler)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eCombinedImageSampler)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eSampledImage)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageImage)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformTexelBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageTexelBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageBuffer)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eUniformBufferDynamic)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eStorageBufferDynamic)
          .setDescriptorCount(1000),
      vk::DescriptorPoolSize{}
          .setType(vk::DescriptorType::eInputAttachment)
          .setDescriptorCount(1000),
  };

  auto const pool_info =
      vk::DescriptorPoolCreateInfo{}
          .setPoolSizes(pool_sizes)
          .setMaxSets(10000)
          .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

  _descriptor_pool = info.core->create_descriptorpool(pool_info);

  ImGui_ImplSDL2_InitForVulkan(info.window->window());
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = info.context->instance();
  init_info.PhysicalDevice = info.core->physical_device();
  init_info.Device = info.core->device();
  init_info.QueueFamily = info.core->queuefamily_index();
  init_info.Queue = info.core->queue();
  init_info.DescriptorPool = _descriptor_pool.get();
  init_info.MinImageCount = info.presenter->swapchain_image_count();
  init_info.ImageCount = info.presenter->swapchain_image_count();
  init_info.Allocator = nullptr;
  init_info.PipelineInfoMain.RenderPass =
      info.debugui_rendering->renderpass->renderpass();
  init_info.PipelineInfoMain.Subpass = 0;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.CheckVkResultFn = imgui_check_vk_result;
  ImGui_ImplVulkan_Init(&init_info);
}

imgui_context_t::~imgui_context_t() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

} // namespace game
