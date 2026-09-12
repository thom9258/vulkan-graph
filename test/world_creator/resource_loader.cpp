#include "resource_loader.hpp"

#include "alex/log.hpp"
#include "alex/memory_buffer.hpp"
#include "alex/texture.hpp"
#include "bitmap.hpp"

#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <expected>
#include <functional>
#include <optional>
#include <print>
#include <utility>
#include <vulkan/vulkan_enums.hpp>

namespace game {

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

  static const constexpr int texture_index{0};
  aiString aipathstring;
  material->GetTexture(type, texture_index, &aipathstring);
  std::string pathstring = aipathstring.C_Str();
  if (pathstring == "") {
    return std::nullopt;
  }

  if (pathstring.starts_with("*")) {
    ALEX_ERROR("Inline textures {} are not supported yet",
               aiTextureTypeToString(type));
    return std::nullopt;
  }

  return std::filesystem::path(pathstring);
}

constexpr auto get_first_diffuse_path =
    std::bind_front(get_first_texture_path, aiTextureType_DIFFUSE);

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

auto material_t::name() -> std::string_view { return _name; }

auto material_t::set_name(std::string_view name) -> void { _name = name; }

auto material_t::diffuse() -> material_texture_t & { return _diffuse; }

auto material_t::load_from_disk(renderable_load_from_disk_info_t &info,
                                aiMaterial *aimaterial, std::string_view mesh_name)
    -> std::optional<material_t> {
  std::filesystem::path const basedir = info.path.parent_path();
  std::optional<std::filesystem::path> diffuse_path =
      util::get_first_diffuse_path(aimaterial);

  ALEX_WARN_IF(!diffuse_path.has_value(),
               "Mesh '{}' Could not get path for diffuse texture '{}'",
               mesh_name, aimaterial->GetName().C_Str());

  if (diffuse_path.has_value()) {
    material_t material;
    material.set_name(aimaterial->GetName().C_Str());

    aimaterial->Get(AI_MATKEY_TWOSIDED, material.diffuse().two_sided);
    aimaterial->Get(AI_MATKEY_OPACITY, material.diffuse().opacity);
    aimaterial->Get(AI_MATKEY_SHININESS, material.diffuse().shininess);
    aimaterial->Get(AI_MATKEY_TRANSPARENCYFACTOR,
                    material.diffuse().transparency);

    std::filesystem::path const path = basedir / diffuse_path.value();
    const auto format = (material.diffuse().opacity == 1.0f)
                            ? bitmap_format_t::rgb
                            : bitmap_format_t::rgba;

    auto diffuse_bitmap = bitmap_t::create(path, format);
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

      material.diffuse().texture.emplace(texture_info);

      immediate_copy_bitmap_to_texture(info.core,
                                       material.diffuse().texture.value(),
                                       diffuse_bitmap.value());

      const auto features = info.core->physical_device().getFeatures();
      const auto properties = info.core->physical_device().getProperties();
      const auto max_anisotropy =
          features.samplerAnisotropy
              ? std::min(4.0f, properties.limits.maxSamplerAnisotropy)
              : 1.0f;

      int u_mapmode{0};
      aimaterial->Get(AI_MATKEY_MAPPINGMODE_U_DIFFUSE(0), u_mapmode);
      int v_mapmode{0};
      aimaterial->Get(AI_MATKEY_MAPPINGMODE_V_DIFFUSE(0), v_mapmode);

      auto find_mapmode = [](int mapmode) -> vk::SamplerAddressMode {
        switch (mapmode) {
        case aiTextureMapMode_Wrap:
          return vk::SamplerAddressMode::eRepeat;
        case aiTextureMapMode_Mirror:
          return vk::SamplerAddressMode::eMirroredRepeat;
        case aiTextureMapMode_Clamp:
          return vk::SamplerAddressMode::eClampToBorder;
        default:
          break;
        }

        return vk::SamplerAddressMode::eRepeat;
      };

      const vk::Filter filter = vk::Filter::eNearest;
      const auto sampler_info =
          vk::SamplerCreateInfo{}
              .setMagFilter(filter)
              .setMinFilter(filter)
              .setAddressModeU(find_mapmode(u_mapmode))
              .setAddressModeV(find_mapmode(v_mapmode))
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

      material.diffuse().sampler =
          info.core->device().createSamplerUnique(sampler_info);
    }

    if (material.diffuse().texture->view() == VK_NULL_HANDLE) {
      ALEX_ERROR("view before return is null");
    }

    return material;
  }

  return std::nullopt;
}

auto mesh_t::vertices() -> alex::memory_buffer_t * {
  if (_vertices.has_value()) {
    return &_vertices.value();
  }

  return nullptr;
}

auto mesh_t::vertices_length() -> std::uint32_t { return _vertices_length; }

auto mesh_t::indices() -> alex::memory_buffer_t * {
  if (_indices.has_value()) {
    return &_indices.value();
  }

  return nullptr;
}

auto mesh_t::indices_length() -> std::uint32_t { return _indices_length; }

auto mesh_t::material_name() -> std::optional<std::string> {
  return _material_name;
}

auto mesh_t::set_vertices(alex::memory_buffer_t vertices, std::uint32_t length)
    -> void {
  _vertices = std::move(vertices);
  _vertices_length = length;
}

auto mesh_t::set_indices(alex::memory_buffer_t indices, std::uint32_t length)
    -> void {
  _indices = std::move(indices);
  _indices_length = length;
}

auto mesh_t::set_material_name(std::string name) -> void {
  _material_name = name;
}

auto mesh_t::load_from_disk(renderable_load_from_disk_info_t &info,
                            const aiScene *aiscene, aiMesh *aimesh)
    -> std::optional<mesh_t> {
  std::vector<simple_vertex_t> vertices;
  vertices.reserve(aimesh->mNumVertices);
  for (unsigned int i = 0; i < aimesh->mNumVertices; i++) {
    simple_vertex_t vertex{};
    vertex.position[0] = aimesh->mVertices[i].x;
    vertex.position[1] = aimesh->mVertices[i].y;
    vertex.position[2] = aimesh->mVertices[i].z;

    // Note we are guaranteed to have normals through our process flags
    vertex.normal[0] = aimesh->mNormals[i].x;
    vertex.normal[1] = aimesh->mNormals[i].y;
    vertex.normal[2] = aimesh->mNormals[i].z;

    if (aimesh->mColors[0] != nullptr) {
      vertex.color[0] = aimesh->mColors[0][i].r;
      vertex.color[1] = aimesh->mColors[0][i].g;
      vertex.color[2] = aimesh->mColors[0][i].b;
    } else {
      vertex.color[0] = 1.0f;
      vertex.color[1] = 1.0f;
      vertex.color[2] = 1.0f;
    }
    if (aimesh->mTextureCoords[0] != nullptr) {
      vertex.texcoord[0] = aimesh->mTextureCoords[0][i].x;
      vertex.texcoord[1] = aimesh->mTextureCoords[0][i].y;
    } else {
      vertex.texcoord[0] = 0.0f;
      vertex.texcoord[1] = 0.0f;
    }

    vertices.push_back(vertex);
  }

  // NOTE we triangulate so we expect 3 indices per face
  std::vector<unsigned int> indices;
  for (unsigned int i = 0; i < aimesh->mNumFaces; i++) {
    aiFace face = aimesh->mFaces[i];
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

  mesh_t mesh;

  aiMaterial *material = aiscene->mMaterials[aimesh->mMaterialIndex];
  if (material != nullptr) {
    mesh.set_material_name(std::string(material->GetName().C_Str()));
  }

  info.core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
    alex::memory_buffer_write_info_t vertices_buffer_write_info;
    vertices_buffer_write_info.physical_device = info.core->physical_device();
    vertices_buffer_write_info.device = info.core->device();
    vertices_buffer_write_info.direct = &direct_vertices_buffer;
    vertices_buffer_write_info.write_size =
        direct_vertices_buffer.memory_size();
    vertices_buffer_write_info.commandbuffer = commandbuffer;

    mesh.set_vertices(alex::memory_buffer_t(vertices_buffer_info),
                      vertices.size());
    mesh.vertices()->record_write(vertices_buffer_write_info);

    alex::memory_buffer_write_info_t indices_buffer_write_info;
    indices_buffer_write_info.physical_device = info.core->physical_device();
    indices_buffer_write_info.device = info.core->device();
    indices_buffer_write_info.direct = &direct_indices_buffer;
    indices_buffer_write_info.write_size = direct_indices_buffer.memory_size();
    indices_buffer_write_info.commandbuffer = commandbuffer;

    mesh.set_indices(alex::memory_buffer_t(indices_buffer_info),
                     indices.size());
    mesh.indices()->record_write(indices_buffer_write_info);
  });

  return mesh;
}

auto model_t::name() -> std::string_view { return _name; }

auto model_t::children() -> std::span<model_t> { return _children; }

auto model_t::transform() -> glm::mat4 { return _transform; }

auto model_t::meshes() -> std::span<mesh_t> { return _meshes; }

auto model_t::set_name(std::string_view name) -> void { _name = name; }

auto model_t::add_child(model_t model) -> void {
  _children.push_back(std::move(model));
}

auto model_t::set_transform(glm::mat4 transform) -> void {
  _transform = transform;
}

auto model_t::add_mesh(mesh_t mesh) -> void {
  _meshes.push_back(std::move(mesh));
}

namespace {
constexpr auto find_material(std::string_view target,
                             std::span<material_t> materials) -> material_t * {
  for (material_t &material : materials) {
    if (material.name() == target)
      return &material;
  }

  return nullptr;
}
} // namespace

auto model_t::load_from_disk(renderable_load_from_disk_info_t &info,
                             std::vector<material_t> &materials,
                             const aiScene *scene, aiNode *node)
    -> std::optional<model_t> {
  model_t model;
  model.set_transform(util::to_glm(node->mTransformation));
  model.set_name(std::string(node->mName.C_Str()));

  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    std::string mesh_name = mesh->mName.C_Str();
    auto loaded_mesh = mesh_t::load_from_disk(info, scene, mesh);
    bool had_mesh = loaded_mesh.has_value();
    if (loaded_mesh.has_value()) {
      model.add_mesh(std::move(*loaded_mesh));
    }

    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    if (material != nullptr) {
      std::string material_name = material->GetName().C_Str();
      material_t *existing = find_material(material_name, materials);
      if (existing == nullptr) {
        auto loaded_material = material_t::load_from_disk(info, material, mesh_name);
        if (loaded_material.has_value()) {
          ALEX_INFO("Loaded Mesh '{}' has Material '{}'", mesh_name,
                    material_name);
          materials.push_back(std::move(*loaded_material));
        } else {
          ALEX_ERROR("Could not load Material '{}' for Mesh '{}' from disk!",
                     material_name, mesh_name);
        }
      }
    }

    if (had_mesh && material == nullptr) {
      ALEX_WARN("Got Mesh '{}' that has no Material", mesh_name);
    }
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    std::optional<model_t> child =
        model_t::load_from_disk(info, materials, scene, node->mChildren[i]);

    if (child.has_value()) {
      model.add_child(std::move(*child));
    }
  }

  return model;
}

renderable_t::renderable_t(std::filesystem::path path, model_t root)
    : _path{path}, _root{std::move(root)} {}

auto renderable_t::material_count() -> std::size_t { return _materials.size(); }

auto renderable_t::materials() -> std::span<material_t> { return _materials; }

auto renderable_t::load_from_disk(renderable_load_from_disk_info_t &info)
    -> std::expected<renderable_t, std::string> {

  if (info.core == nullptr) {
    return std::unexpected("info.core == nullptr");
  }

  if (!std::filesystem::exists(info.path)) {
    return std::unexpected(
        std::format("Invalid info.path '{}'", info.path.string()));
  }

  if (!std::filesystem::is_regular_file(info.path)) {
    return std::unexpected(
        std::format("info.path is not a file '{}'", info.path.string()));
  }

  chrono_time_point_t start = chrono_clock_t::now();

  std::uint32_t const load_flags = std::invoke([&info]() {
    std::uint32_t constexpr flags =
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs |
        aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights;

    return flags;
  });

  Assimp::Importer importer;
  const aiScene *scene = importer.ReadFile(info.path, load_flags);
  if (!scene) {
    return std::unexpected(
        std::format("Importer had no scene '{}'", importer.GetErrorString()));
  }

  if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
    return std::unexpected(
        std::format("Importer invalid scene '{}'", importer.GetErrorString()));
  }

  if (!scene->mRootNode) {
    return std::unexpected(
        std::format("Importer Missing Root '{}'", importer.GetErrorString()));
  }

  std::vector<material_t> materials;
  auto loaded_model =
      model_t::load_from_disk(info, materials, scene, scene->mRootNode);

  for (material_t &mat : materials) {
    if (mat.diffuse().texture->view() == VK_NULL_HANDLE) {
      ALEX_ERROR("Found material after loading from disk that has null handle");
    }
  }

  if (loaded_model.has_value()) {
    renderable_t model_source(info.path, std::move(*loaded_model));

    for (material_t &material : materials) {
      model_source.add_material(std::move(material));
    }

    model_source.set_loadtime(start, chrono_clock_t::now());
    return model_source;
  }

  return std::unexpected(
      std::format("Importer Load Error '{}'", importer.GetErrorString()));
}

auto renderable_t::path() const -> std::optional<std::filesystem::path> {
  return _path;
}

auto renderable_t::loadtime_seconds() const -> std::optional<double> {
  if (!_loadtime.has_value()) {
    return std::nullopt;
  }

  return std::chrono::duration<double>(_loadtime->end - _loadtime->start)
      .count();
}

auto renderable_t::root() -> model_t * {
  if (_root.has_value()) {
    return &_root.value();
  }

  return nullptr;
}

auto renderable_t::add_material(material_t material) -> material_t * {
  _materials.push_back(std::move(material));
  return &_materials.back();
}

auto renderable_t::set_loadtime(chrono_time_point_t start,
                                chrono_time_point_t end) -> void {
  _loadtime.emplace();
  _loadtime->start = start;
  _loadtime->end = end;
}

auto renderable_t::find_material(std::string_view name) -> material_t * {
  for (material_t &material : _materials) {
    if (material.name() == name) {
      return &material;
    }
  }

  return nullptr;
}

} // namespace game
