#pragma once

#include <vulkan/vulkan.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL_video.h>

#include <chrono>
#include <functional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace sdl {

namespace {
constexpr auto default_error_handler(std::string_view msg) {
  std::println("Fatal sdl error: {}", msg);
  std::exit(-1);
}
} // namespace

struct window_extent_t {
  int width{0};
  int height{0};

  constexpr auto aspect() const -> float;
};

constexpr auto window_extent_t::aspect() const -> float {
  if (width == 0 || height == 0) {
    return 0;
  }

  return static_cast<float>(width) / static_cast<float>(height);
}

struct window_info_t {
  std::string name{"sdl window"};
  int x{-1};
  int y{-1};
  int width{800};
  int height{600};
  std::function<void(std::string_view)> error_handler{default_error_handler};
};

class window_t {
public:
  constexpr window_t(window_info_t &info);
  constexpr ~window_t();
  constexpr window_t(window_t &&) = default;
  constexpr window_t &operator=(window_t &&) = default;
  constexpr window_t(const window_t &) = delete;
  constexpr window_t &operator=(const window_t &) = delete;

  constexpr auto mark_next_frame() -> void;

  constexpr auto get_events() -> std::span<SDL_Event>;

  constexpr auto instance_extensions() const -> std::vector<const char *>;

  constexpr auto create_window_surface(vk::Instance instance) const
      -> vk::UniqueSurfaceKHR;

  constexpr auto window_extent() const -> window_extent_t;

  constexpr auto deltatime_seconds() const -> double;

  constexpr auto totaltime_seconds() const -> double;

private:
  using chrono_clock_t = std::chrono::high_resolution_clock;
  using chrono_time_point_t = std::chrono::time_point<chrono_clock_t>;

  SDL_Window *_window{nullptr};

  std::vector<SDL_Event> _current_events;
  chrono_time_point_t _start_frame_time;
  chrono_time_point_t _current_frame_time;
  chrono_time_point_t _last_frame_time;
};

constexpr window_t::window_t(window_info_t &info) {
  if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
    info.error_handler("Could not init sdl!");
  }

  SDL_Vulkan_LoadLibrary(nullptr);
  _window = SDL_CreateWindow(info.name.data(), info.x, info.y, info.width,
                             info.height, 0 | SDL_WINDOW_VULKAN);

  if (_window == nullptr) {
    info.error_handler("Could not create window!");
  }

  _start_frame_time = chrono_clock_t::now();
}

constexpr window_t::~window_t() {
  SDL_DestroyWindow(_window);
  SDL_Quit();
}

constexpr auto window_t::mark_next_frame() -> void {
  {
    std::vector<SDL_Event> events{};
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      events.push_back(event);
    }
    _current_events = events;
  }
  {
    _last_frame_time = _current_frame_time;
    _current_frame_time = chrono_clock_t::now();
  }
}

constexpr auto window_t::get_events() -> std::span<SDL_Event> {
  return _current_events;
}

constexpr auto window_t::instance_extensions() const
    -> std::vector<const char *> {
  uint32_t count{0};
  SDL_Vulkan_GetInstanceExtensions(_window, &count, nullptr);

  std::vector<const char *> extensions(count);
  SDL_Vulkan_GetInstanceExtensions(_window, &count, extensions.data());
  return extensions;
}

constexpr auto window_t::create_window_surface(vk::Instance instance) const
    -> vk::UniqueSurfaceKHR {

  vk::UniqueSurfaceKHR surface;
  {
    VkSurfaceKHR tmp;
    SDL_Vulkan_CreateSurface(_window, instance, &tmp);
    surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(tmp), instance);
  }

  return surface;
}

constexpr auto window_t::window_extent() const -> window_extent_t {
  window_extent_t extent;
  SDL_GetWindowSize(_window, &extent.width, &extent.height);
  return extent;
}

constexpr auto window_t::deltatime_seconds() const -> double {
  return std::chrono::duration<double>(_current_frame_time - _last_frame_time)
      .count();
}

constexpr auto window_t::totaltime_seconds() const -> double {
  return std::chrono::duration<double>(_current_frame_time - _start_frame_time)
      .count();
}

} // namespace sdl
