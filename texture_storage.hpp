#pragma once

#include "core.hpp"
#include "texture.hpp"
#include <string_view>
#include <span>

namespace alex {

struct stored_texture_t {
  std::string_view name{""};
  std::span<texture_t> textures;
};

struct texture_storage_info_t {
  std::size_t capacity{16};
};

struct texture_storage_t {
  void init(texture_storage_info_t &info, memory::arena &allocator);
  auto add(std::string_view name, std::span<texture_t> textures) -> std::span<texture_t>;
  auto find(std::string_view name) -> std::span<texture_t>;
  [[nodiscard]]
  auto remove(std::string_view name) -> std::span<texture_t>;

  std::span<stored_texture_t> m_stored_textures;
};

} // namespace alex
