#include "core.hpp"
#include "ensure.hpp"
#include "log.hpp"
#include "fixed_vector.hpp"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <optional>
#include <ranges>

namespace alex {

void context_t::init(context_info_t &info, memory::arena &allocator) {
  if (info.instance_extensions.empty()) {
    LOG_WARN("No Vulkan Instance Extensions were provided");
  }


  fixed_vector_t<const char *> extensions;
  constexpr const std::size_t max_extensions{16};
  extensions.init(allocator.allocate < const char*>(max_extensions));
  for (const char *extension : info.instance_extensions) {
    extensions.put(extension);
  }

  extensions.put(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  LOG_INFO("Loaded Extensions ({}):", extensions.length());
  for (std::size_t i = 0; i < extensions.length(); i++) {
    LOG_INFO("  {}", extensions[i]);
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
                                .setEnabledExtensionCount(extensions.length())
                                .setPpEnabledExtensionNames(extensions.data());

  if (info.enable_validation) {
    LOG_INFO("Loaded Validation Layers ({}):", validation_layers.size());
    for (auto layer : validation_layers) {
      LOG_INFO("  {}", layer);
    }
    instanceCreateInfo.setPEnabledLayerNames(validation_layers);
  }

  instance = vk::createInstance(instanceCreateInfo);
}

auto get_queue_family(vk::PhysicalDevice device, vk::SurfaceKHR surface)
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

std::optional<std::uint64_t>
rate_physical_device(vk::PhysicalDevice physical_device,
                     vk::SurfaceKHR surface) {

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

void core_t::init(core_info_t &info, memory::arena &allocator) {
  ENSURE(info.context, "context must be set");
  uint32_t physical_device_count{0};
  vk::Result result = info.context->instance.enumeratePhysicalDevices(
      &physical_device_count, nullptr);
  ENSURE(result == vk::Result::eSuccess,
         "Could not enumerate physical devices");

  auto physical_devices =
      allocator.allocate<vk::PhysicalDevice>(physical_device_count);
  ENSURE_NOT(physical_devices.empty(), "Arena could not allocate");

  result = info.context->instance.enumeratePhysicalDevices(
      &physical_device_count, physical_devices.data());

  ENSURE(result == vk::Result::eSuccess,
         "Could not enumerate physical devices");

  auto scores =
      allocator.allocate<std::optional<std::uint64_t>>(physical_devices.size());
  ENSURE_NOT(scores.empty(), "Arena could not allocate");

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
      physical_device = device;
    }
    vk::PhysicalDeviceProperties properties = device.getProperties();
    LOG_INFO("  {}) {}, score: {}", properties.deviceName.data(),
             vk::to_string(properties.deviceType), score.value());
  }
  ENSURE(best_score > 0, "Could not get a suitable physical_device")

  {
    vk::PhysicalDeviceProperties properties = physical_device.getProperties();
    LOG_INFO("Chosen Physical Device: {}", properties.deviceName.data());
  }

  {
    std::optional<std::uint32_t> family =
        get_queue_family(physical_device, info.surface);
    ENSURE(family, "Could not get a queue family for physical_device")
    queuefamily_index = family.value();
  }

  std::array<float, 1> constexpr queue_priorities{1.0f};
  auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo{}
                                   .setFlags({})
                                   .setQueueFamilyIndex(queuefamily_index)
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

  device = physical_device.createDevice(deviceCreateInfo);

  constexpr int queue_index = 0;
  queue = device.getQueue(queuefamily_index, queue_index);

  auto commandPoolCreateInfo =
      vk::CommandPoolCreateInfo{}
          .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
          .setQueueFamilyIndex(queuefamily_index);
  commandpool = device.createCommandPool(commandPoolCreateInfo, nullptr);
}

} // namespace alex
