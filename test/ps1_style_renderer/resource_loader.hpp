#pragma once

#include <alex/core.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/texture.hpp>

#include "alex/flightframe_array.hpp"
#include "bitmap.hpp"
#include "include_assimp.hpp"
#include "include_glm.hpp"
#include "mesh.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan_handles.hpp>

namespace game {

struct material_properties_t {
  bool two_sided{false};
  float transparency{0.0f};
  float shininess{0.0f};
  float opacity{1.0f};
};

struct material_t {
  std::string name;

  std::optional<alex::texture_t> diffuse;
  std::optional<vk::UniqueSampler> diffuse_sampler;
  std::optional<material_properties_t> diffuse_properties;

  std::optional<alex::texture_t> specular;
  std::optional<vk::UniqueSampler> specular_sampler;

  std::optional<alex::texture_t> ambient;
  std::optional<vk::UniqueSampler> ambient_sampler;
};

struct mesh_t {
  std::optional<alex::memory_buffer_t> vertices;
  std::uint32_t vertices_length{0};
  std::optional<alex::memory_buffer_t> indices;
  std::uint32_t indices_length{0};
  std::optional<std::string> material;
};

struct model_t {
  std::string name;
  std::vector<model_t> children;
  glm::mat4 transform;
  std::vector<mesh_t> meshes;
};

struct model_load_info_t {
  alex::core_t *core{nullptr};
  std::filesystem::path path{};

  struct {
    bool generate_smooth_normals{false};
    bool limit_bone_weights{true};
    bool flip_uvs{true};
    bool fix_infacing_normals{true};
  } mesh;

  struct {
    vk::Filter filter{vk::Filter::eLinear};
  } texture;
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
  using chrono_clock_t = std::chrono::high_resolution_clock;
  using chrono_time_point_t = std::chrono::time_point<chrono_clock_t>;

  model_source_t(std::filesystem::path path, model_t root);
  model_source_t(const model_source_t &) = delete;
  model_source_t(model_source_t &&) = default;
  model_source_t &operator=(const model_source_t &) = delete;
  model_source_t &operator=(model_source_t &&) = default;
  ~model_source_t() = default;

  static auto create(model_load_info_t &info)
      -> std::expected<model_source_t, load_error_t>;

  auto path() const -> std::filesystem::path;
  auto loadtime_seconds() const -> std::optional<double>;
  auto root() -> model_t &;
  auto find_material(std::string_view name) -> material_t *;

private:
  auto add_material(material_t &&material) -> material_t *;
  auto set_loadtime(chrono_time_point_t start, chrono_time_point_t end) -> void;

  struct loadtime_t {
    chrono_time_point_t start;
    chrono_time_point_t end;
  };

  std::optional<loadtime_t> _loadtime;
  std::filesystem::path _path;
  model_t _root;
  std::vector<material_t> _materials;
};

} // namespace game
