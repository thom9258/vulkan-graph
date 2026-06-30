#pragma once

#include <alex/core.hpp>

#include "ps1_style_renderer/resource_loader.hpp"

namespace game {

class static_resources_t {
public:
  static_resources_t(alex::core_t *core);

  auto load_chest() -> void;
  auto chest_model() -> model_source_t *;
  auto chest_texture_sampler() -> vk::Sampler;
  auto chest_texture() -> alex::texture_t *;

private:
  alex::core_t *_core{nullptr};

  struct chest_t {
    model_source_t model;
    alex::texture_t diffuse_texture;
    vk::UniqueSampler diffuse_texture_sampler;
  };

  std::optional<chest_t> _chest;
};

} // namespace game
