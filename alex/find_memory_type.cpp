#include "find_memory_type.hpp"
#include "log.hpp"

namespace alex {

uint32_t
find_memory_type(vk::PhysicalDeviceMemoryProperties const &memoryProperties,
               uint32_t typeBits, vk::MemoryPropertyFlags requirementsMask) {
  uint32_t typeIndex = uint32_t(~0);
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags &
                            requirementsMask) == requirementsMask)) {
      typeIndex = i;
      break;
    }
    typeBits >>= 1;
  }

  ALEX_ERROR_IF(typeIndex == uint32_t(~0), "No texture type");
  return typeIndex;
}

}
