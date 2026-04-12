#include "texture_storage.hpp"
#include "ensure.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>
#include <ranges>

namespace alex {

void texture_storage_t::add(std::string_view name, std::span<texture_t> textures)
     {
	stored_texture_t texture_to_store;
	texture_to_store.name = std::string(name);
	texture_to_store.textures = textures | std::ranges::to<std::vector>();
	m_stored_textures.push_back(texture_to_store);
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
