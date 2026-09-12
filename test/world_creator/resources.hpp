#pragma once

#include <alex/core.hpp>

#include "resource_loader.hpp"

#include <filesystem>
#include <map>

namespace game {

class resources_t {
public:
  explicit resources_t(alex::core_t *core, std::filesystem::path manifest);

  auto get_renderable(std::string_view name) -> renderable_t*;

  auto get_all_renderable_names() -> std::vector<std::string>;

private:
  alex::core_t *_core{nullptr};
  std::filesystem::path _manifest;
  std::map<std::string, renderable_t> _renderables;
};

} // namespace game
