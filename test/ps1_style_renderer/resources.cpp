#include "resources.hpp"

#include "ps1_style_renderer/resource_loader.hpp"
#include "slurp_file.hpp"

#include <alex/log.hpp>

#include <glaze/json.hpp>

#include <string>

namespace game {

resources_t::resources_t(alex::core_t *core, std::filesystem::path manifest)
    : _core{core}, _manifest{manifest} {

  auto file = slurp_file(manifest);
  if (!file) {
    ALEX_ERROR("Could not find asset manifest at path: {}", manifest.string());
    return;
  }

  auto json = glz::lazy_json(*file);
  if (!json.has_value()) {
    ALEX_ERROR("Could not parse content of asset manifest at path: {}",
               manifest.string());
    return;
  }

  for (auto model : json->root()["models"]) {
    auto name = model["name"].get<std::string>();
    auto path = model["path"].get<std::string>();
    if (name.has_value() && path.has_value()) {

      renderable_load_from_disk_info_t info;
      info.core = _core;
      info.path = *path;
      info.texture_filter = vk::Filter::eNearest;

      auto model_source = renderable_t::load_from_disk(info);
      if (!model_source.has_value()) {
        ALEX_ERROR("Model '{}' could not be loaded at path '{}', [error: {}]",
                   *name, *path, model_source.error());
        continue;
      }

      ALEX_INFO("Loaded '{}' from path '{}' in {}s", *name, *path,
                model_source->loadtime_seconds().value());
      _renderables.insert({*name, std::move(*model_source)});
    }
  }
}

auto resources_t::get_renderable(std::string_view name) -> renderable_t * {
  auto found = _renderables.find(std::string(name));
  if (found == _renderables.end()) {
    return nullptr;
  }

  return &found->second;
}

auto resources_t::get_all_renderable_names() -> std::vector<std::string> {
  std::vector<std::string> names;
  for (auto &[name, model_source] : _renderables) {
    names.push_back(name);
  }

  return names;
}

} // namespace game
