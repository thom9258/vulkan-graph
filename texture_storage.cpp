#include "texture_storage.hpp"
#include "ensure.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

void texture_storage_t::init(texture_storage_info_t &info,
                             memory::arena &allocator) {
  m_stored_textures =
      allocator.allocate<stored_texture_t>(info.capacity);
  ENSURE_NOT(m_stored_textures.empty(), "allocator is full")
}

auto texture_storage_t::add(std::string_view name, std::span<texture_t> textures)
    -> std::span<texture_t> {
  for (stored_texture_t &location : m_stored_textures) {
    if (location.textures.empty()) {
      location.name = name;
      location.textures = textures;
      return location.textures;
    }
  }

  return {};
}

auto texture_storage_t::find(std::string_view name)
    -> std::span<texture_t> {
  for (stored_texture_t &location : m_stored_textures) {
    if (location.name == name) {
      return location.textures;
    }
  }

  return {};
}

[[nodiscard]]
auto texture_storage_t::remove(std::string_view name)
    -> std::span<texture_t> {

  for (stored_texture_t &location : m_stored_textures) {
    if (location.name == name) {
      std::span<texture_t> found = location.textures;
      location.name = "";
      location.textures = {};
      return found;
    }
  }

  return {};
}

} // namespace alex
