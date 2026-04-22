#include "core.hpp"

namespace alex {

uint32_t
find_memory_type(vk::PhysicalDeviceMemoryProperties const &memoryProperties,
                 uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask);

} // namespace alex
