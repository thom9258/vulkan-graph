#pragma once

#include "texture.hpp"
#include <string_view>
#include <vector>

namespace alex {

struct stored_texture_t {
  std::string name{""};
  std::vector<texture_t> textures;
};

struct texture_storage_info_t {
  std::size_t capacity{16};
};

struct texture_storage_t {
  texture_storage_t() = default;
  texture_storage_t(const texture_storage_t&) = delete;
  texture_storage_t& operator=(const texture_storage_t&) = delete;
  texture_storage_t(texture_storage_t&&) = default;
  texture_storage_t& operator=(texture_storage_t&&) = default;

  void add(std::string_view name, std::span<texture_t> textures);
  auto find(std::string_view name) -> std::span<texture_t>;
  [[nodiscard]]
  auto remove(std::string_view name) -> std::vector<texture_t>;

  std::vector<stored_texture_t> m_stored_textures;
};

} // namespace alex
