#include "texture_storage.hpp"
#include "find_memory_type.hpp"
#include <ranges>
#include <vulkan/vulkan_enums.hpp>

namespace alex {

void texture_storage_t::add(std::string_view name,
                            std::span<texture_t> textures) {
  stored_texture_t texture_to_store;
  texture_to_store.name = std::string(name);
  for (texture_t &texture : textures) {
    texture_to_store.textures.push_back(std::move(texture));
  }

  m_stored_textures.push_back(std::move(texture_to_store));
}

auto texture_storage_t::find(std::string_view name) -> std::span<texture_t> {
  for (stored_texture_t &location : m_stored_textures) {
    if (location.name == name) {
      return location.textures;
    }
  }

  return {};
}

[[nodiscard]]
auto texture_storage_t::remove(std::string_view name)
    -> std::vector<texture_t> {

  auto found = std::ranges::find_if(
      m_stored_textures, [&](stored_texture_t &st) { return st.name == name; });

  if (found == std::ranges::end(m_stored_textures)) {
    return {};
  }

  std::ranges::swap(*found, m_stored_textures.back());
  stored_texture_t removed = std::move(m_stored_textures.back());
  m_stored_textures.pop_back();
  return std::move(removed.textures);
}

} // namespace alex
