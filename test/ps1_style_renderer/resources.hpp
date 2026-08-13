#pragma once

#include <alex/core.hpp>

#include "ps1_style_renderer/resource_loader.hpp"

#include <filesystem>
#include <map>

namespace game {

class resources_t {
public:
  explicit resources_t(alex::core_t *core, std::filesystem::path manifest);

private:
  alex::core_t *_core{nullptr};
  std::filesystem::path _manifest;
  std::map<std::string, model_source_t> _models;
};

} // namespace game
