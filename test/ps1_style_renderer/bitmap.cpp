#include "bitmap.hpp"

#include "thirdparty/stb_image.h"

#include <utility>

namespace game {

constexpr auto to_stb_format(bitmap_format_t fmt) -> int {
  switch (fmt) {
    using enum bitmap_format_t;
  case rgba:
    return STBI_rgb_alpha;
  case rgb:
    return STBI_rgb;
  };

  std::unreachable();
};

auto bitmap_t::create(std::filesystem::path path, bitmap_format_t format)
    -> std::optional<bitmap_t> {
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  if (!std::filesystem::is_regular_file(path)) {
    return std::nullopt;
  }

  stbi_set_flip_vertically_on_load(true);

  bitmap_t bitmap;
  bitmap._pixels = pixels_pointer_t(
      {stbi_load(path.string().c_str(), &bitmap._width, &bitmap._height,
                 &bitmap._channels, to_stb_format(format)),
       pixels_deleter_t{}});
  if (bitmap.pixels() == nullptr) {
    return std::nullopt;
  }

  return bitmap;
}

auto bitmap_t::pixels_deleter_t::operator()(unsigned char *pixels) -> void {
  if (pixels != nullptr) {
    stbi_image_free(pixels);
  }
}

auto bitmap_t::memory_size() const -> std::size_t {
  return _width * _height * _channels;
}

auto bitmap_t::width() const -> int { return _width; }

auto bitmap_t::height() const -> int { return _height; }

auto bitmap_t::channels() const -> int { return _channels; }

auto bitmap_t::format() const -> bitmap_format_t {
  switch (channels()) {
  case 3:
    return bitmap_format_t::rgb;
  case 4:
    return bitmap_format_t::rgba;
  default:
    return bitmap_format_t::rgb;
  };

  std::unreachable();
}

auto bitmap_t::pixels() -> unsigned char * { return _pixels.get(); }

auto bitmap_t::make_direct_buffer(vk::PhysicalDevice physical_device,
                                  vk::Device device)
    -> alex::direct_memory_buffer_t {
  alex::direct_memory_buffer_info_t staging_buffer_info;
  staging_buffer_info.buffer_type = alex::memory_buffer_type_t::basic;
  staging_buffer_info.physical_device = physical_device;
  staging_buffer_info.device = device;
  staging_buffer_info.memory_size = memory_size();
  alex::direct_memory_buffer_t staging_buffer(staging_buffer_info);
  std::memcpy(staging_buffer.memory_ptr(), pixels(), memory_size());
  return staging_buffer;
}

} // namespace game
