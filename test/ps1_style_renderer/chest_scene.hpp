#pragma once

#include "../utility/scenestack.hpp"

#include <print>

namespace game {

class chest_scene : public scene::scene_t {
public:
  constexpr chest_scene() {};

  constexpr ~chest_scene() override {};

  constexpr auto load() -> void override { std::println("{} Loaded", _name); };

  constexpr auto input() -> void override {};

  constexpr auto update(double deltatime) -> void override {
    std::println("{} Updated (dt:{})", _name, deltatime);
  };

  constexpr auto draw() -> void override {};

  constexpr auto unload() -> void override {
    std::println("{} Unloaded", _name);
  };

private:
  const std::string_view _name{"Chest Scene"};
};

} // namespace game
