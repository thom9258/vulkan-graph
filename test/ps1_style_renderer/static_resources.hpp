#pragma once

#include <alex/core.hpp>

#include "ps1_style_renderer/resource_loader.hpp"

namespace game {

class static_resources_t {
public:
  static_resources_t(alex::core_t *core);

  auto chest_model() -> model_source_t *;
  auto chest_texture() -> alex::texture_t *;
  auto chest_texture_sampler() -> vk::Sampler;

private:
  alex::core_t *_core{nullptr};

  auto load_chest_model() -> void;
  auto load_chest_texture() -> void;
  auto load_chest_texture_sampler() -> void;
	

  struct chest_t {
    std::optional<model_source_t> model;
    std::optional<alex::texture_t> diffuse_texture;
    std::optional<vk::UniqueSampler> diffuse_texture_sampler;
  } _chest;
};

} // namespace game
