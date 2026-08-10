#include "resource_loader.hpp"

#include "alex/memory_buffer.hpp"
#include "alex/texture.hpp"
#include "ps1_style_renderer/bitmap.hpp"

#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <expected>
#include <functional>
#include <optional>
#include <print>
#include <utility>
#include <vulkan/vulkan_enums.hpp>

namespace game {

load_error_t::load_error_t(code_t code, const char *error)
    : _code{code}, _error{error} {}

auto load_error_t::code() const -> code_t { return _code; }

auto load_error_t::error() const -> std::string_view { return _error; }

constexpr inline auto load_error_incomplete_info(const char *error)
    -> load_error_t {
  return load_error_t(load_error_t::code_t::incomplete_info, error);
}

constexpr inline auto load_error_invalid_path(const char *error)
    -> load_error_t {
  return load_error_t(load_error_t::code_t::invalid_path, error);
}

constexpr inline auto load_error_no_scene(const char *error) -> load_error_t {
  return load_error_t(load_error_t::code_t::no_scene, error);
}

constexpr inline auto load_error_incomplete_scene(const char *error)
    -> load_error_t {
  return load_error_t(load_error_t::code_t::incomplete_scene, error);
}

constexpr inline auto load_error_missing_root(const char *error)
    -> load_error_t {
  return load_error_t(load_error_t::code_t::missing_root, error);
}

namespace util {

template <typename TVertex>
auto unindex_vertices(std::span<TVertex> vertices,
                      std::span<std::uint32_t> indices)
    -> std::vector<TVertex> {
  std::vector<TVertex> unindexed;
  unindexed.reserve(indices.size());

  for (std::uint32_t index : indices)
    unindexed.push_back(vertices[index]);
  return unindexed;
}

constexpr auto to_glm(aiMatrix4x4 from) -> glm::mat4 {
  glm::mat4 to;
  // the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
  to[0][0] = from.a1;
  to[1][0] = from.a2;
  to[2][0] = from.a3;
  to[3][0] = from.a4;
  to[0][1] = from.b1;
  to[1][1] = from.b2;
  to[2][1] = from.b3;
  to[3][1] = from.b4;
  to[0][2] = from.c1;
  to[1][2] = from.c2;
  to[2][2] = from.c3;
  to[3][2] = from.c4;
  to[0][3] = from.d1;
  to[1][3] = from.d2;
  to[2][3] = from.d3;
  to[3][3] = from.d4;
  return to;
}

constexpr auto to_glm(aiVector3f other) -> glm::vec3 {
  glm::vec3 v;
  v[0] = other.x;
  v[1] = other.y;
  v[2] = other.z;
  return v;
}

constexpr auto to_glm(aiQuaternion other) -> glm::quat {
  glm::quat q;
  q.w = other.w;
  q.x = other.x;
  q.y = other.y;
  q.z = other.z;
  return q;
}

constexpr auto get_first_texture_path(aiTextureType type, aiMaterial *material)
    -> std::optional<std::filesystem::path> {
  if (material == nullptr) {
    return std::nullopt;
  }
  int const constexpr first_texture_index{0};
  aiString aipathstring;
  material->GetTexture(type, first_texture_index, &aipathstring);
  std::string pathstring = aipathstring.C_Str();
  if (pathstring == "") {
    return std::nullopt;
  }

  if (pathstring.starts_with("*")) {
    return std::nullopt;
  }

  return std::filesystem::path(pathstring);
}

constexpr auto get_first_diffuse_path =
    std::bind_front(get_first_texture_path, aiTextureType_DIFFUSE);

constexpr auto get_first_specular_path =
    std::bind_front(get_first_texture_path, aiTextureType_SPECULAR);

constexpr auto get_first_ambient_path =
    std::bind_front(get_first_texture_path, aiTextureType_AMBIENT);

} // namespace util

constexpr auto immediate_copy_bitmap_to_texture(alex::core_t *core,
                                                alex::texture_t &texture,
                                                game::bitmap_t &bitmap)
    -> void {

  alex::direct_memory_buffer_t staging_buffer =
      bitmap.make_direct_buffer(core->physical_device(), core->device());

  core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
    // Transition image to color override
    {
      auto range = vk::ImageSubresourceRange{}
                       .setAspectMask(vk::ImageAspectFlagBits::eColor)
                       .setBaseMipLevel(0)
                       .setLevelCount(1)
                       .setBaseArrayLayer(0)
                       .setLayerCount(1);

      auto barrier = vk::ImageMemoryBarrier{}
                         .setOldLayout(vk::ImageLayout::eUndefined)
                         .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                         .setImage(texture.image())
                         .setSubresourceRange(range)
                         .setSrcAccessMask(vk::AccessFlags())
                         .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }

    // Copy buffer into image
    {
      auto layer = vk::ImageSubresourceLayers{}
                       .setAspectMask(vk::ImageAspectFlagBits::eColor)
                       .setMipLevel(0)
                       .setBaseArrayLayer(0)
                       .setLayerCount(1);

      const auto offset = vk::Offset3D{}.setX(0).setY(0).setZ(0);

      const auto extent = vk::Extent3D{}
                              .setWidth(bitmap.width())
                              .setHeight(bitmap.height())
                              .setDepth(1);

      auto region = vk::BufferImageCopy{}
                        .setBufferOffset(0)
                        .setBufferRowLength(0)
                        .setBufferImageHeight(0)
                        .setImageSubresource(layer)
                        .setImageOffset(offset)
                        .setImageExtent(extent);

      commandbuffer.copyBufferToImage(staging_buffer.buffer(), texture.image(),
                                      vk::ImageLayout::eTransferDstOptimal,
                                      region);
    }

    // Transfer image to shader readonly optimal
    {
      const auto source_range =
          vk::ImageSubresourceRange{}
              .setAspectMask(vk::ImageAspectFlagBits::eColor)
              .setBaseMipLevel(0)
              .setLevelCount(1)
              .setBaseArrayLayer(0)
              .setLayerCount(1);

      auto barrier = vk::ImageMemoryBarrier{}
                         .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                         .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                         .setImage(texture.image())
                         .setSubresourceRange(source_range)
                         .setSrcAccessMask(vk::AccessFlags())
                         .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }
  });
}

constexpr auto create_texture_sampler(alex::core_t *core) {
  const auto features = core->physical_device().getFeatures();
  const auto properties = core->physical_device().getProperties();
  const auto max_anisotropy =
      features.samplerAnisotropy
          ? std::min(4.0f, properties.limits.maxSamplerAnisotropy)
          : 1.0f;

  const vk::Filter filter = vk::Filter::eNearest;
  const auto sampler_info =
      vk::SamplerCreateInfo{}
          .setMagFilter(filter)
          .setMinFilter(filter)
          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
          .setAddressModeW(vk::SamplerAddressMode::eRepeat)
          .setAnisotropyEnable(features.samplerAnisotropy)
          .setMaxAnisotropy(max_anisotropy)
          .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
          .setUnnormalizedCoordinates(false)
          .setCompareEnable(false)
          .setCompareOp(vk::CompareOp::eAlways)
          .setMipmapMode(vk::SamplerMipmapMode::eLinear)
          .setMipLodBias(0.0f)
          .setMinLod(0.0f)
          .setMaxLod(0.0f);

  return core->device().createSamplerUnique(sampler_info);
}

constexpr auto load_material(model_load_info_t &info, const aiScene *scene,
                             aiMaterial *material) -> material_t {

  std::filesystem::path const basedir = info.path.parent_path();
  material_t out;
  out.name = material->GetName().C_Str();

  std::optional<std::filesystem::path> diffuse_path =
      util::get_first_diffuse_path(material);

  if (diffuse_path.has_value()) {
    std::filesystem::path const path = basedir / diffuse_path.value();
    auto diffuse_bitmap = bitmap_t::create(path, bitmap_format_t::rgba);
    if (diffuse_bitmap.has_value()) {
      alex::texture_info_t texture_info;
      texture_info.physical_device = info.core->physical_device();
      texture_info.device = info.core->device();
      texture_info.extent =
          vk::Extent2D{static_cast<std::uint32_t>(diffuse_bitmap->width()),
                       static_cast<std::uint32_t>(diffuse_bitmap->height())};
      texture_info.format = game::to_vk_format(diffuse_bitmap->format());
      texture_info.tiling = vk::ImageTiling::eOptimal;
      texture_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
      texture_info.usage = vk::ImageUsageFlagBits::eSampled |
                           vk::ImageUsageFlagBits::eTransferDst;

      out.diffuse.emplace(texture_info);
      immediate_copy_bitmap_to_texture(info.core, out.diffuse.value(),
                                       diffuse_bitmap.value());
    }
  }
  std::optional<std::filesystem::path> specular_path =
      util::get_first_specular_path(material);

  if (specular_path.has_value()) {
    std::filesystem::path const path = basedir / specular_path.value();
  }

  std::optional<std::filesystem::path> ambient_path =
      util::get_first_ambient_path(material);

  if (ambient_path.has_value()) {
    std::filesystem::path const path = basedir / ambient_path.value();
  }

  return out;
}

constexpr auto load_mesh(model_load_info_t &info, const aiScene *scene,
                         aiMesh *mesh) -> mesh_t {
  std::vector<simple_vertex_t> vertices;
  vertices.reserve(mesh->mNumVertices);
  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    simple_vertex_t vertex{};
    vertex.position[0] = mesh->mVertices[i].x;
    vertex.position[1] = mesh->mVertices[i].y;
    vertex.position[2] = mesh->mVertices[i].z;

    // Note we are guaranteed to have normals through our process flags
    vertex.normal[0] = mesh->mNormals[i].x;
    vertex.normal[1] = mesh->mNormals[i].y;
    vertex.normal[2] = mesh->mNormals[i].z;

    if (mesh->mColors[0] != nullptr) {
      vertex.color[0] = mesh->mColors[0][i].r;
      vertex.color[1] = mesh->mColors[0][i].g;
      vertex.color[2] = mesh->mColors[0][i].b;
    } else {
      vertex.color[0] = 1.0f;
      vertex.color[1] = 1.0f;
      vertex.color[2] = 1.0f;
    }
    if (mesh->mTextureCoords[0] != nullptr) {
      vertex.texcoord[0] = mesh->mTextureCoords[0][i].x;
      vertex.texcoord[1] = mesh->mTextureCoords[0][i].y;
    } else {
      vertex.texcoord[0] = 0.0f;
      vertex.texcoord[1] = 0.0f;
    }

    vertices.push_back(vertex);
  }

  // NOTE we triangulate so we expect 3 indices per face
  std::vector<unsigned int> indices;
  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    aiFace face = mesh->mFaces[i];
    for (std::size_t j = 0; j < face.mNumIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  alex::direct_memory_buffer_info_t direct_vertices_buffer_info;
  direct_vertices_buffer_info.physical_device = info.core->physical_device();
  direct_vertices_buffer_info.device = info.core->device();
  direct_vertices_buffer_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_vertices_buffer_info.memory_size =
      sizeof(vertices[0]) * vertices.size();

  alex::direct_memory_buffer_t direct_vertices_buffer(
      direct_vertices_buffer_info);
  std::memcpy(direct_vertices_buffer.memory_ptr(), vertices.data(),
              direct_vertices_buffer.memory_size());

  alex::memory_buffer_info_t vertices_buffer_info;
  vertices_buffer_info.physical_device = info.core->physical_device();
  vertices_buffer_info.device = info.core->device();
  vertices_buffer_info.buffer_type = alex::memory_buffer_type_t::vertices;
  vertices_buffer_info.memory_size = direct_vertices_buffer_info.memory_size;

  alex::direct_memory_buffer_info_t direct_indices_buffer_info;
  direct_indices_buffer_info.physical_device = info.core->physical_device();
  direct_indices_buffer_info.device = info.core->device();
  direct_indices_buffer_info.buffer_type = alex::memory_buffer_type_t::basic;
  direct_indices_buffer_info.memory_size = sizeof(indices[0]) * indices.size();

  alex::direct_memory_buffer_t direct_indices_buffer(
      direct_indices_buffer_info);
  std::memcpy(direct_indices_buffer.memory_ptr(), indices.data(),
              direct_indices_buffer.memory_size());

  alex::memory_buffer_info_t indices_buffer_info;
  indices_buffer_info.physical_device = info.core->physical_device();
  indices_buffer_info.device = info.core->device();
  indices_buffer_info.buffer_type = alex::memory_buffer_type_t::indices;

  indices_buffer_info.memory_size = direct_indices_buffer_info.memory_size;

  mesh_t result;

  info.core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
    alex::memory_buffer_write_info_t vertices_buffer_write_info;
    vertices_buffer_write_info.physical_device = info.core->physical_device();
    vertices_buffer_write_info.device = info.core->device();
    vertices_buffer_write_info.direct = &direct_vertices_buffer;
    vertices_buffer_write_info.write_size =
        direct_vertices_buffer.memory_size();
    vertices_buffer_write_info.commandbuffer = commandbuffer;

    result.vertices.emplace(vertices_buffer_info);
    result.vertices.value().record_write(vertices_buffer_write_info);
    result.vertices_length = vertices.size();

    alex::memory_buffer_write_info_t indices_buffer_write_info;
    indices_buffer_write_info.physical_device = info.core->physical_device();
    indices_buffer_write_info.device = info.core->device();
    indices_buffer_write_info.direct = &direct_indices_buffer;
    indices_buffer_write_info.write_size = direct_indices_buffer.memory_size();
    indices_buffer_write_info.commandbuffer = commandbuffer;

    result.indices.emplace(indices_buffer_info);
    result.indices.value().record_write(indices_buffer_write_info);
    result.indices_length = indices.size();
  });

  return result;
}

constexpr auto load_model(model_load_info_t &info,
                          std::vector<material_t> &materials,
                          const aiScene *scene, aiNode *node)
    -> std::optional<model_t> {

  model_t model;
  model.transform = util::to_glm(node->mTransformation);
  model.name = std::string(node->mName.C_Str());

  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    model.meshes.emplace_back(load_mesh(info, scene, mesh));

    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    if (material != nullptr) {
      materials.emplace_back(load_material(info, scene, material));
    }
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    std::optional<model_t> child =
        load_model(info, materials, scene, node->mChildren[i]);
    if (child.has_value())
      model.children.push_back(std::move(*child));
  }

  return model;
}

model_source_t::model_source_t(std::filesystem::path path, model_t root)
    : _path{path}, _root{std::move(root)} {}

auto model_source_t::create(model_load_info_t &info)
    -> std::expected<model_source_t, load_error_t> {

  if (info.core == nullptr) {
    return std::unexpected(load_error_incomplete_info("info.core == nullptr"));
  }

  if (!std::filesystem::exists(info.path)) {
    return std::unexpected(load_error_invalid_path(info.path.string().c_str()));
  }

  if (!std::filesystem::is_regular_file(info.path)) {
    return std::unexpected(load_error_invalid_path(info.path.string().c_str()));
  }

  chrono_time_point_t start = chrono_clock_t::now();

  std::uint32_t const load_flags = std::invoke([&info]() {

#if 0
    std::uint32_t flags = aiProcess_GenNormals | aiProcess_Triangulate ;

    if (info.mesh.generate_smooth_normals) {
      flags |= aiProcess_GenSmoothNormals;
    }
    if (info.mesh.limit_bone_weights) {
      flags |= aiProcess_LimitBoneWeights;
    }
    if (info.mesh.fix_infacing_normals) {
      //flags |= aiProcess_FixInfacingNormals;
    }
    if (info.mesh.flip_uvs) {
      flags |= aiProcess_FlipUVs;
    }
#endif
    std::uint32_t constexpr flags =
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs |
        aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights;

    return flags;
  });

  Assimp::Importer importer;
  const aiScene *scene = importer.ReadFile(info.path, load_flags);
  if (!scene) {
    return std::unexpected(load_error_no_scene(importer.GetErrorString()));
  }

  if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
    return std::unexpected(
        load_error_incomplete_scene(importer.GetErrorString()));
  }

  if (!scene->mRootNode) {
    return std::unexpected(load_error_missing_root(importer.GetErrorString()));
  }

  std::vector<material_t> materials;
  std::optional<model_t> model =
      load_model(info, materials, scene, scene->mRootNode);
  if (model.has_value()) {
    model_source_t model_source(info.path, std::move(*model));

    for (material_t &material : materials) {
      model_source.add_material(std::move(material));
    }

    model_source.set_loadtime(start, chrono_clock_t::now());
    return model_source;
  }

  return std::unexpected(load_error_missing_root(importer.GetErrorString()));
}

auto model_source_t::path() const -> std::filesystem::path { return _path; }

auto model_source_t::loadtime_seconds() const -> std::optional<double> {
  if (!_loadtime.has_value()) {
    return std::nullopt;
  }

  return std::chrono::duration<double>(_loadtime->end - _loadtime->start)
      .count();
}

auto model_source_t::root() -> model_t & { return _root; }

auto model_source_t::add_material(material_t &&material) -> material_t * {
  _materials.push_back(std::move(material));
  return &_materials.back();
}

auto model_source_t::set_loadtime(chrono_time_point_t start,
                                  chrono_time_point_t end) -> void {
  _loadtime.emplace();
  _loadtime->start = start;
  _loadtime->end = end;
}

auto model_source_t::find_material(std::string_view name) -> material_t * {
  for (material_t &material : _materials) {
    if (material.name == name) {
      return &material;
    }
  }

  return nullptr;
}

} // namespace game
