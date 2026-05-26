#pragma once

#include <alex/core.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/texture.hpp>

#include "mesh.hpp"
#include "include_assimp.hpp"
#include "include_glm.hpp"

#include <expected>
#include <optional>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace game {


struct material_t {
	std::optional<alex::texture_t> diffuse;
};

struct mesh_t {
  std::optional<alex::memory_buffer_t> vertices;
  std::uint32_t vertices_length{0};
  std::optional<alex::memory_buffer_t> indices;
  std::uint32_t indices_length{0};
};

struct model_t {
  std::string name;
  std::vector<model_t> children;
  glm::mat4 transform;
  std::vector<mesh_t> meshes;
  std::vector<material_t> materials;
};

struct model_load_info_t {
  alex::core_t* core{nullptr};
  std::filesystem::path path{};

  struct {
    bool generate_smooth_normals{false};
    bool limit_bone_weights{true};
    bool flip_uvs{true};
    bool fix_infacing_normals{false};
  } mesh_adapters;
};

class load_error_t {
public:
  enum class code_t {
    incomplete_info,
    invalid_path,
    no_scene,
    incomplete_scene,
    missing_root,
  };

  explicit load_error_t(code_t code, const char *error);
  explicit load_error_t() = default;

  auto code() const -> code_t;
  auto error() const -> std::string_view;

private:
  code_t _code;
  std::string _error{""};
};

constexpr auto to_string(load_error_t::code_t e) -> std::string_view {
  switch (e) {
    using enum load_error_t::code_t;
  case incomplete_info:
    return "incomplete info";
  case invalid_path:
    return "invalid path";
  case no_scene:
    return "no scene";
  case incomplete_scene:
    return "incomplete scene";
  case missing_root:
    return "missing root";
  }

  std::unreachable();
}

class model_source_t {
public:
  model_source_t(std::filesystem::path path, model_t root);
  model_source_t(const model_source_t &) = delete;
  model_source_t(model_source_t &&) = default;
  model_source_t &operator=(const model_source_t &) = delete;
  model_source_t &operator=(model_source_t &&) = default;
  ~model_source_t() = default;

  static auto create(model_load_info_t &info)
      -> std::expected<model_source_t, load_error_t>;

  auto path() const -> std::filesystem::path;
  auto loadtime_seconds() const -> double;
  auto root() -> model_t &;

private:

  using chrono_clock_t = std::chrono::high_resolution_clock;
  using chrono_time_point_t = std::chrono::time_point<chrono_clock_t>;
  chrono_time_point_t start;
  chrono_time_point_t end;
  std::filesystem::path _path;
  model_t _root;
};

} // namespace game
