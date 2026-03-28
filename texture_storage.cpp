#include "texture_storage.hpp"
#include "ensure.hpp"
#include "find_memory_type.hpp"
#include <vulkan/vulkan_enums.hpp>

namespace alex {

void texture_storage_t::init(texture_storage_info_t &info,
                             memory::arena &allocator) {

  auto allocated_textures =
      allocator.allocate<std::optional<stored_texture_t>>(info.capacity);
  ENSURE_NOT(allocated_textures.empty(), "allocator is full")
  textures = allocated_textures;
}

auto texture_storage_t::add(std::string_view name, texture_t texture)
    -> texture_t * {
  for (std::optional<stored_texture_t> &location : textures) {
    if (!location.has_value()) {
      location.emplace(name, texture);
      return &location.value().texture;
    }
  }

  return nullptr;
}

auto texture_storage_t::find(std::string_view name) -> texture_t * {
  for (std::optional<stored_texture_t> &location : textures) {
    if (location.has_value()) {
      if (location.value().name == name) {
        return &location.value().texture;
      }
    }
  }

  return nullptr;
}

[[nodiscard]]
auto texture_storage_t::remove(std::string_view name)
    -> std::optional<texture_t> {

  std::optional<texture_t> found;

  for (std::optional<stored_texture_t> &location : textures) {
    if (location.has_value()) {
      if (location.value().name == name) {
        found = location.value().texture;
        location = std::nullopt;
      }
    }
  }

  return found;
}

} // namespace alex
