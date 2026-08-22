#pragma once

#include "include_glm.hpp"

#include <glaze/json.hpp>
#include <glaze/json/lazy.hpp>

#include <array>
#include <optional>
#include <string_view>

namespace game::serialization::utility {

constexpr auto read_vec3(glz::lazy_json_view<glz::opts{}> json,
                         std::string_view name) -> std::optional<glm::vec3> {
  std::array<float, 3> data;
  auto error = glz::read_json(data, json[name]);
  if (error) {
    return std::nullopt;
  }

  return glm::vec3(data[0], data[1], data[2]);
}

} // namespace game::serialization::utility
