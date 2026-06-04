#pragma once

#include <memory>
#include <vector>

namespace scene {

class scene_t {
public:
  constexpr virtual ~scene_t() = default;

  constexpr virtual auto load() -> void {};
  constexpr virtual auto input() -> void {};
  constexpr virtual auto update(double deltatime) -> void {};
  constexpr virtual auto draw() -> void {};
  constexpr virtual auto unload() -> void {};
};

class scenestack_t {
public:
  constexpr auto tick(double deltatime) -> void {
    if (_scenes.empty()) {
      return;
    }

    auto &top = _scenes.back();
    top->input();
    top->update(deltatime);
    top->draw();
  }

  constexpr auto top() -> scene_t * {
    if (_scenes.empty()) {
      return nullptr;
    }

    return _scenes.back().get();
  }

  constexpr auto put(std::unique_ptr<scene_t> scene) -> scene_t * {
    unload_top_if_it_exists();
    _scenes.push_back(std::move(scene));
	_scenes.back()->load();
    return _scenes.back().get();
  }

  constexpr auto try_pop() -> std::unique_ptr<scene_t> {
    if (_scenes.empty()) {
      return nullptr;
    }

    auto popped = std::move(_scenes.back());
    _scenes.pop_back();
    if (popped != nullptr) {
      popped->unload();
    }

    load_top_if_it_exists();
    return popped;
  }

  constexpr auto size() -> std::size_t { return _scenes.size(); }

private:
  constexpr auto unload_top_if_it_exists() -> void {
    if (_scenes.empty()) {
      return;
    }
    _scenes.back()->unload();
  }

  constexpr auto load_top_if_it_exists() -> void {
    if (_scenes.empty()) {
      return;
    }
    _scenes.back()->load();
  }

  std::vector<std::unique_ptr<scene_t>> _scenes;
};

} // namespace scene
