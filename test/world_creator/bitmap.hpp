#pragma once

#include <alex/memory_buffer.hpp>
#include <alex/texture.hpp>

#include <filesystem>
#include <optional>

namespace game {

enum class bitmap_format_t { rgb, rgba };

constexpr auto to_vk_format(bitmap_format_t fmt) -> vk::Format {
  switch (fmt) {
    using enum bitmap_format_t;
  case rgb:
    return vk::Format::eR8G8B8Srgb;

  case rgba:
    return vk::Format::eR8G8B8A8Srgb;
  };

  std::unreachable();
};

struct bitmap_t {
  static auto create(std::filesystem::path path, bitmap_format_t format)
      -> std::optional<bitmap_t>;

  ~bitmap_t() = default;
  bitmap_t(const bitmap_t &) = delete;
  bitmap_t(bitmap_t &&) = default;
  bitmap_t &operator=(const bitmap_t &) = delete;
  bitmap_t &operator=(bitmap_t &&) = delete;

  auto memory_size() const -> std::size_t;

  auto make_direct_buffer(vk::PhysicalDevice physical_device, vk::Device device)
      -> alex::direct_memory_buffer_t;

  auto width() const -> int;
  auto height() const -> int;
  auto channels() const -> int;
  auto format() const -> bitmap_format_t;
  auto pixels() -> unsigned char *;

private:
  explicit bitmap_t() = default;
  struct pixels_deleter_t {
    static auto operator()(unsigned char *pixels) -> void;
  };

  int _width{0};
  int _height{0};
  int _channels{0};

  using pixels_pointer_t = std::unique_ptr<unsigned char, pixels_deleter_t>;
  pixels_pointer_t _pixels{nullptr, pixels_deleter_t{}};
};

} // namespace game
