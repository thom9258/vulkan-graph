#include "resources.hpp"

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

      model_load_info_t info;
      info.core = _core;
      info.path = *path;
      info.texture.filter = vk::Filter::eNearest;

      auto model_source = model_source_t::create(info);
      if (!model_source.has_value()) {
        ALEX_ERROR(
            "Model '{}' could not be loaded at path '{}', [error:{} code:{}]",
            *name, *path, model_source.error().error(),
            to_string(model_source.error().code()));
        continue;
      }

      ALEX_INFO("Loaded '{}' from path '{}' in {}s", *name, *path,
                model_source->loadtime_seconds().value());
      _models.insert({*name, std::move(*model_source)});
    }
  }
}

auto resources_t::get_model(std::string_view name) -> model_source_t * {
  auto found = _models.find(std::string(name));
  if (found == _models.end()) {
    return nullptr;
  }

  return &found->second;
}

auto resources_t::get_all_model_names() -> std::vector<std::string> {
  std::vector<std::string> names;
  for (auto &[name, model_source] : _models) {
    names.push_back(name);
  }

  return names;
}

} // namespace game
