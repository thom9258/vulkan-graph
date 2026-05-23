#include "core.hpp"
#include "ensure.hpp"
#include "fixed_vector.hpp"
#include "log.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <optional>
#include <ranges>

namespace alex {

context_t::context_t(context_info_t &info) {
  if (info.instance_extensions.empty()) {
    LOG_WARN("No Vulkan Instance Extensions were provided");
  }

  std::vector<const char *> extensions;
  for (const char *extension : info.instance_extensions) {
    extensions.push_back(extension);
  }

  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  LOG_INFO("Loaded Extensions ({}):", extensions.size());
  for (const char *extension : extensions) {
    LOG_INFO("  {}", extension);
  }

  std::array<const char *, 1> validation_layers{
      "VK_LAYER_KHRONOS_validation",
  };

  auto applicationInfo = vk::ApplicationInfo{}
                             .setPApplicationName(info.name.data())
                             .setPEngineName("engine")
                             .setApplicationVersion(VK_MAKE_VERSION(1, 1, 0))
                             .setEngineVersion(VK_MAKE_VERSION(1, 1, 0))
                             .setApiVersion(VK_MAKE_VERSION(1, 1, 0));

  auto instanceCreateInfo = vk::InstanceCreateInfo{}
                                .setPApplicationInfo(&applicationInfo)
                                .setEnabledExtensionCount(extensions.size())
                                .setPpEnabledExtensionNames(extensions.data());

  if (info.enable_validation) {
    LOG_INFO("Loaded Validation Layers ({}):", validation_layers.size());
    for (auto layer : validation_layers) {
      LOG_INFO("  {}", layer);
    }
    instanceCreateInfo.setPEnabledLayerNames(validation_layers);
  }

  _instance = vk::createInstanceUnique(instanceCreateInfo);
}

auto context_t::instance() -> vk::Instance { return _instance.get(); }

static constexpr auto get_queue_family(vk::PhysicalDevice device,
                                       vk::SurfaceKHR surface)
    -> std::optional<std::uint32_t> {

  for (auto [i, family] :
       device.getQueueFamilyProperties() | std::views::enumerate) {

    bool const has_graphics =
        static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics);
    bool const has_transfer =
        static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eTransfer);
    bool const has_compute =
        static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eCompute);
    bool const has_present = device.getSurfaceSupportKHR(i, surface);

    if (has_graphics && has_transfer && has_compute && has_present) {
      return i;
    }
  }

  return std::nullopt;
}

static constexpr auto rate_physical_device(vk::PhysicalDevice physical_device,
                                           vk::SurfaceKHR surface)
    -> std::optional<std::uint64_t> {
  std::optional<std::uint64_t> score{0};
  vk::PhysicalDeviceProperties properties = physical_device.getProperties();
  if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
    score.value() += 200;
  } else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
    score.value() += 10;
  }

  vk::PhysicalDeviceFeatures features = physical_device.getFeatures();
  if (!features.samplerAnisotropy) {
    score = std::nullopt;
  }
  if (!features.fillModeNonSolid) {
    score = std::nullopt;
  }

  vk::PhysicalDeviceMemoryProperties memory_properties =
      physical_device.getMemoryProperties();
  for (std::size_t i = 0; i < memory_properties.memoryTypeCount; i++) {
    if (memory_properties.memoryTypes[i].propertyFlags |
        vk::MemoryPropertyFlagBits::eDeviceLocal) {
      score.value() += 100;
    }
  }

  std::size_t constexpr gb = 1'000'000'000;

  for (std::size_t i = 0; i < memory_properties.memoryHeapCount; i++) {
    if (memory_properties.memoryHeaps[i].size > vk::DeviceSize(4 * gb)) {
      score.value() += 300;
    } else if (memory_properties.memoryHeaps[i].size > vk::DeviceSize(1 * gb)) {
      score.value() += 100;
    }
  }

  std::optional<std::uint32_t> family =
      get_queue_family(physical_device, surface);

  if (!family.has_value()) {
    return std::nullopt;
  }

  return score;
}

core_t::core_t(core_info_t &info) {
  uint32_t physical_device_count{0};
  vk::Result result =
      info.instance.enumeratePhysicalDevices(&physical_device_count, nullptr);
  ENSURE(result == vk::Result::eSuccess,
         "Could not enumerate physical devices");

  std::vector<vk::PhysicalDevice> physical_devices(physical_device_count);
  result = info.instance.enumeratePhysicalDevices(&physical_device_count,
                                                  physical_devices.data());

  ENSURE(result == vk::Result::eSuccess,
         "Could not enumerate physical devices");

  using score_t = std::optional<std::uint64_t>;
  std::vector<score_t> scores(physical_devices.size());
  for (auto [i, physical_device] : physical_devices | std::views::enumerate) {
    scores[i] = rate_physical_device(physical_device, info.surface);
  }

  std::uint32_t best_score{0};
  LOG_INFO("Physical Devices ({}):", physical_device_count);
  for (auto [device, score] : std::views::zip(physical_devices, scores)) {

    if (!score.has_value())
      continue;

    if (score.value() > best_score) {
      best_score = score.value();
      _physical_device = device;
    }

    vk::PhysicalDeviceProperties properties = device.getProperties();
    LOG_INFO("  {}) {}, score: {}", properties.deviceName.data(),
             vk::to_string(properties.deviceType), score.value());
  }
  ENSURE(best_score > 0, "Could not get a suitable physical_device")

  {
    vk::PhysicalDeviceProperties properties = _physical_device.getProperties();
    LOG_INFO("Chosen Physical Device: {}", properties.deviceName.data());
  }

  {
    std::optional<std::uint32_t> family =
        get_queue_family(_physical_device, info.surface);
    ENSURE(family, "Could not get a queue family for physical_device")
    _queuefamily_index = family.value();
  }

  std::array<float, 1> constexpr queue_priorities{1.0f};
  auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo{}
                                   .setFlags({})
                                   .setQueueFamilyIndex(_queuefamily_index)
                                   .setQueuePriorities(queue_priorities)
                                   .setQueueCount(1);

  std::array<const char *, 1> constexpr device_extensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  const auto features = vk::PhysicalDeviceFeatures{}
                            .setFillModeNonSolid(true)
                            .setSamplerAnisotropy(true);

  auto deviceCreateInfo =
      vk::DeviceCreateInfo{}
          .setQueueCreateInfoCount(1)
          .setQueueCreateInfos(deviceQueueCreateInfo)
          .setPEnabledFeatures(&features)
          .setPpEnabledExtensionNames(device_extensions.data())
          .setEnabledExtensionCount(device_extensions.size());

  _device = _physical_device.createDeviceUnique(deviceCreateInfo);

  constexpr int queue_index = 0;
  _queue = _device->getQueue(_queuefamily_index, queue_index);

  auto commandPoolCreateInfo =
      vk::CommandPoolCreateInfo{}
          .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
          .setQueueFamilyIndex(queuefamily_index());

  _commandpool =
      device().createCommandPoolUnique(commandPoolCreateInfo, nullptr);
}

auto core_t::physical_device() -> vk::PhysicalDevice {
  return _physical_device;
}

auto core_t::device() -> vk::Device { return _device.get(); }

auto core_t::queue() -> vk::Queue { return _queue; }

auto core_t::queuefamily_index() -> std::uint32_t { return _queuefamily_index; }

auto core_t::commandpool() -> vk::CommandPool { return _commandpool.get(); }

auto core_t::create_descriptorpool(vk::DescriptorPoolCreateInfo info)
    -> vk::UniqueDescriptorPool {
  return device().createDescriptorPoolUnique(info);
}

auto core_t::create_commandbuffer() -> vk::UniqueCommandBuffer {
  auto init_commandbuffer_alloc_info =
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandpool())
          .setLevel(vk::CommandBufferLevel::ePrimary)
          .setCommandBufferCount(1);

  return std::move(
      device()
          .allocateCommandBuffersUnique(init_commandbuffer_alloc_info)
          .front());
}

auto core_t::create_fence_signaled() -> vk::UniqueFence {
  return device().createFenceUnique(
      vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
}

auto core_t::create_fence() -> vk::UniqueFence {
  return device().createFenceUnique(vk::FenceCreateInfo{});
}

auto core_t::create_semaphore() -> vk::UniqueSemaphore {
  return device().createSemaphoreUnique(vk::SemaphoreCreateInfo{});
}

} // namespace alex
