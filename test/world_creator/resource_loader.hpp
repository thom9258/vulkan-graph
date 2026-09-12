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

enum class model_load_flags_t {
  generate_smooth_normals,
  limit_bone_weights,
  flip_uvs,
  fix_infacing_normals
};

struct renderable_load_from_disk_info_t {
  alex::core_t *core{nullptr};
  std::filesystem::path path{};
  std::vector<model_load_flags_t> model_load_flags;
  vk::Filter texture_filter{vk::Filter::eLinear};
};

struct material_texture_t {
  std::optional<alex::texture_t> texture;
  std::optional<vk::UniqueSampler> sampler;
  bool two_sided{false};
  float transparency{0.0f};
  float shininess{0.0f};
  float opacity{1.0f};
};

class material_t {
public:
  static auto load_from_disk(renderable_load_from_disk_info_t &info,
                             aiMaterial *material,
                             std::string_view mesh_name)
      -> std::optional<material_t>;

  auto name() -> std::string_view;

  auto set_name(std::string_view name) -> void;

  auto diffuse() -> material_texture_t &;

private:
  std::string _name;
  material_texture_t _diffuse;
};

class mesh_t {
public:
  static auto load_from_disk(renderable_load_from_disk_info_t &info,
                             const aiScene *aiscene, aiMesh *aimesh)
      -> std::optional<mesh_t>;

  auto vertices() -> alex::memory_buffer_t *;
  auto vertices_length() -> std::uint32_t;
  auto indices() -> alex::memory_buffer_t *;
  auto indices_length() -> std::uint32_t;
  auto material_name() -> std::optional<std::string>;
  auto set_vertices(alex::memory_buffer_t vertices, std::uint32_t length)
      -> void;
  auto set_indices(alex::memory_buffer_t indices, std::uint32_t length) -> void;
  auto set_material_name(std::string name) -> void;

private:
  std::optional<alex::memory_buffer_t> _vertices;
  std::uint32_t _vertices_length{0};
  std::optional<alex::memory_buffer_t> _indices;
  std::uint32_t _indices_length{0};
  std::optional<std::string> _material_name;
};

class model_t {
public:
  static auto load_from_disk(renderable_load_from_disk_info_t &info,
                             std::vector<material_t> &materials,
                             const aiScene *scene, aiNode *node)
      -> std::optional<model_t>;

  model_t() = default;
  model_t(const model_t &) = delete;
  model_t(model_t &&) = default;
  model_t &operator=(const model_t &) = delete;
  model_t &operator=(model_t &&) = default;
  ~model_t() = default;

  auto name() -> std::string_view;
  auto children() -> std::span<model_t>;
  auto transform() -> glm::mat4;
  auto meshes() -> std::span<mesh_t>;

  auto set_name(std::string_view name) -> void;
  auto add_child(model_t model) -> void;
  auto set_transform(glm::mat4 transform) -> void;
  auto add_mesh(mesh_t mesh) -> void;

private:
  std::string _name;
  std::vector<model_t> _children;
  glm::mat4 _transform;
  std::vector<mesh_t> _meshes;
};

class renderable_t {
public:
  using chrono_clock_t = std::chrono::high_resolution_clock;
  using chrono_time_point_t = std::chrono::time_point<chrono_clock_t>;

  renderable_t(std::filesystem::path path, model_t root);
  renderable_t(const renderable_t &) = delete;
  renderable_t(renderable_t &&) = default;
  renderable_t &operator=(const renderable_t &) = delete;
  renderable_t &operator=(renderable_t &&) = default;
  ~renderable_t() = default;

  static auto load_from_disk(renderable_load_from_disk_info_t &info)
      -> std::expected<renderable_t, std::string>;

  auto path() const -> std::optional<std::filesystem::path>;

  auto root() -> model_t *;

  auto find_material(std::string_view name) -> material_t *;

  auto material_count() -> std::size_t;

  auto materials() -> std::span<material_t>;

  auto loadtime_seconds() const -> std::optional<double>;

private:
  auto add_material(material_t material) -> material_t *;
  auto set_loadtime(chrono_time_point_t start, chrono_time_point_t end) -> void;

  struct loadtime_t {
    chrono_time_point_t start;
    chrono_time_point_t end;
  };

  std::optional<loadtime_t> _loadtime;
  std::optional<std::filesystem::path> _path;
  std::optional<model_t> _root;
  std::vector<material_t> _materials;
};

} // namespace game
