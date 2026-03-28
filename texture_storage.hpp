#pragma once

#include "core.hpp"
#include "texture.hpp"
#include <string_view>

namespace alex {

struct stored_texture_t {
  std::string_view name{""};
  texture_t texture;
};

struct texture_storage_info_t {
  std::size_t capacity{16};
};

struct texture_storage_t {
  void init(texture_storage_info_t &info, memory::arena &allocator);
  auto add(std::string_view name, texture_t texture) -> texture_t *;
  auto find(std::string_view name) -> texture_t *;
  [[nodiscard]]
  auto remove(std::string_view name) -> std::optional<texture_t>;

  std::span<std::optional<stored_texture_t>> textures;
};

} // namespace alex
