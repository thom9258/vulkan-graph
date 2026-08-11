#pragma once

#include <alex/log.hpp>

#include "entity_concept.hpp"

#include "glm_transform_hierarchy.hpp"
#include "rendering.hpp"
#include "resource_loader.hpp"
#include "utility/transform_hierarchy.hpp"

namespace game {

namespace detail {

struct static_mesh_ref_t {
  alex::memory_buffer_t *vertices{nullptr};
  std::uint32_t vertices_length{0};
  alex::memory_buffer_t *indices{nullptr};
  std::uint32_t indices_length{0};

  vk::UniqueDescriptorPool uniform_descriptor_pool;
  std::vector<alex::direct_memory_buffer_t> direct_uniforms;

  std::vector<alex::memory_buffer_t> uniforms;
  std::vector<vk::UniqueDescriptorSet> uniform_descriptorsets;

  vk::UniqueDescriptorPool diffuse_descriptor_pool;
  std::vector<vk::UniqueDescriptorSet> diffuse_descriptorsets;

  static_mesh_ref_t() = default;
  ~static_mesh_ref_t() = default;
  static_mesh_ref_t(static_mesh_ref_t &&) = default;
  static_mesh_ref_t &operator=(static_mesh_ref_t &&) = default;
  static_mesh_ref_t(const static_mesh_ref_t &) = delete;
  static_mesh_ref_t &operator=(const static_mesh_ref_t &) = delete;
};

struct static_model_ref_t {
  std::string name;
  std::vector<static_mesh_ref_t> meshes;
  std::vector<static_model_ref_t> children;
};

} // namespace detail

struct static_mesh_entity_update_info_t {
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::CommandBuffer commandbuffer;
  std::uint32_t flightframe;

  glm::mat4 camera_view;
  glm::mat4 camera_projection;

  glm_transform_hierarchy *transform_hierarchy{nullptr};
};

struct static_mesh_entity_draw_info_t {
  vk::CommandBuffer commandbuffer;
  vk::PipelineLayout geometry_pipeline_layout;
  std::uint32_t flightframe;
};

class static_mesh_entity_t {
public:
  constexpr static_mesh_entity_t(
      std::string_view name, transform_hierarchy::transform_id_t transform_id);

  constexpr ~static_mesh_entity_t() = default;

  constexpr static_mesh_entity_t(const static_mesh_entity_t &) = delete;

  constexpr static_mesh_entity_t &
  operator=(const static_mesh_entity_t &) = delete;

  constexpr static_mesh_entity_t(static_mesh_entity_t &&) = default;

  constexpr static_mesh_entity_t &operator=(static_mesh_entity_t &&) = default;

  constexpr auto name() -> std::string_view;

  constexpr auto set_name(std::string_view name) -> void;

  constexpr auto transform_id() -> transform_hierarchy::transform_id_t;

  constexpr auto
  set_transform_id(transform_hierarchy::transform_id_t transform_id) -> void;

  constexpr auto resource_update(static_mesh_entity_update_info_t &info)
      -> void;

  constexpr auto draw(static_mesh_entity_draw_info_t &info) -> void;

  constexpr auto set_model_source(alex::core_t *core,
                                  geometry_rendering_t *geometry_rendering,
                                  model_source_t &model_source) -> void;

  std::optional<detail::static_model_ref_t> _static_model_ref;

private:
  std::string _name{"unnamed-static-mesh-entity"};
  transform_hierarchy::transform_id_t _transform_id{
      transform_hierarchy::invalid_transform_id};
};

static_assert(entity_concept<static_mesh_entity_t>);

constexpr static_mesh_entity_t::static_mesh_entity_t(
    std::string_view name, transform_hierarchy::transform_id_t transform_id)
    : _name{name}, _transform_id{transform_id} {}

constexpr auto static_mesh_entity_t::name() -> std::string_view {
  return _name;
}

constexpr auto static_mesh_entity_t::set_name(std::string_view name) -> void {
  _name = name;
}

constexpr auto static_mesh_entity_t::transform_id()
    -> transform_hierarchy::transform_id_t {
  return _transform_id;
}

constexpr auto static_mesh_entity_t::set_transform_id(
    transform_hierarchy::transform_id_t transform_id) -> void {
  _transform_id = transform_id;
}

constexpr auto
static_mesh_entity_t::resource_update(static_mesh_entity_update_info_t &info)
    -> void {
  if (!_static_model_ref.has_value()) {
    ALEX_WARN("Asked to resource_update static mesh without model ref");
    return;
  }

  auto model_matrix = info.transform_hierarchy->global_location(_transform_id);

  auto update_model = [&](auto self, detail::static_model_ref_t &ref) -> void {
    for (detail::static_mesh_ref_t &ref : ref.meshes) {
      draw_info_t draw_info;
      draw_info.view = info.camera_view;
      draw_info.projection = info.camera_projection;

      // TODO: must calculate parent to child matrix relationship
      draw_info.model = model_matrix.value();
      std::memcpy(ref.direct_uniforms[info.flightframe].memory_ptr(),
                  &draw_info, sizeof(draw_info));

      alex::memory_buffer_write_info_t write_info;
      write_info.physical_device = info.physical_device;
      write_info.device = info.device;
      write_info.direct = &ref.direct_uniforms[info.flightframe];
      write_info.write_size = ref.uniforms[info.flightframe].memory_size();
      write_info.commandbuffer = info.commandbuffer;
      ref.uniforms[info.flightframe].record_write(write_info);
    }

    for (detail::static_model_ref_t &child : ref.children) {
      self(self, child);
    }
  };

  update_model(update_model, _static_model_ref.value());

  // TODO: do children aswell
}

constexpr auto static_mesh_entity_t::draw(static_mesh_entity_draw_info_t &info)
    -> void {
  if (!_static_model_ref.has_value()) {
    ALEX_WARN("Asked to draw static mesh without model ref");
    return;
  }

  auto draw_model = [&](auto self, detail::static_model_ref_t &ref) -> void {
    for (detail::static_mesh_ref_t &ref : ref.meshes) {
      vk::DescriptorSet uniform =
          ref.uniform_descriptorsets[info.flightframe].get();
      info.commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            info.geometry_pipeline_layout, 0, 1,
                                            &uniform, 0, nullptr);

      vk::DescriptorSet diffuse =
          ref.diffuse_descriptorsets[info.flightframe].get();
      info.commandbuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                            info.geometry_pipeline_layout, 1, 1,
                                            &diffuse, 0, nullptr);

      std::vector<vk::DeviceSize> offsets = {0};
      std::vector<vk::Buffer> buffers = {ref.vertices->buffer()};
      info.commandbuffer.bindVertexBuffers(0, 1, buffers.data(),
                                           offsets.data());

      vk::DeviceSize offset = 0;
      vk::Buffer buffer = ref.indices->buffer();
      info.commandbuffer.bindIndexBuffer(buffer, offset,
                                         vk::IndexType::eUint32);

      info.commandbuffer.drawIndexed(ref.indices_length, 1, 0, 0, 0);
    }

    for (detail::static_model_ref_t &child : ref.children) {
      self(self, child);
    }
  };

  draw_model(draw_model, _static_model_ref.value());
}

namespace detail {

constexpr auto create_ref_for_mesh(alex::core_t *core,
                                   geometry_rendering_t *geometry_rendering,
                                   model_source_t &model_source, mesh_t &mesh)
    -> static_mesh_ref_t {
  static_mesh_ref_t ref;
  ref.vertices = &mesh.vertices.value();
  ref.vertices_length = mesh.vertices_length;
  ref.indices = &mesh.indices.value();
  ref.indices_length = mesh.indices_length;

  core->immediate_evaluate([&](vk::CommandBuffer commandbuffer) -> void {
    draw_info_t draw_info;

    alex::direct_memory_buffer_info_t direct_uniform_info;
    direct_uniform_info.physical_device = core->physical_device();
    direct_uniform_info.device = core->device();
    direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
    direct_uniform_info.memory_size = sizeof(draw_info);

    for (std::size_t i = 0; i < alex::frames_in_flight; i++) {
      ref.direct_uniforms.emplace_back(direct_uniform_info);
      std::memcpy(ref.direct_uniforms.back().memory_ptr(), &draw_info,
                  sizeof(draw_info));
    }

    for (std::size_t i = 0; i < alex::frames_in_flight; i++) {
      alex::memory_buffer_info_t uniform_info;
      uniform_info.physical_device = core->physical_device();
      uniform_info.device = core->device();
      uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
      uniform_info.memory_size = direct_uniform_info.memory_size;
      ref.uniforms.emplace_back(uniform_info);

      alex::memory_buffer_write_info_t write_info;
      write_info.physical_device = core->physical_device();
      write_info.device = core->device();
      write_info.direct = &ref.direct_uniforms[i];
      write_info.write_size = ref.uniforms.back().memory_size();
      write_info.commandbuffer = commandbuffer;
      ref.uniforms.back().record_write(write_info);
    }

    auto allocated_frame_uniform_descriptorsets =
        core->allocate_repeated_descriptorsets(
            geometry_rendering->setlayout.frame_uniform.get(),
            vk::DescriptorType::eUniformBuffer, 2);

    ref.uniform_descriptor_pool =
        std::move(allocated_frame_uniform_descriptorsets.pool);
    ref.uniform_descriptorsets =
        std::move(allocated_frame_uniform_descriptorsets.sets);

    for (auto [i, uniform] : ref.uniforms | std::views::enumerate) {
      const auto buffer_info = vk::DescriptorBufferInfo{}
                                   .setBuffer(uniform.buffer())
                                   .setOffset(0)
                                   .setRange(uniform.memory_size());

      const std::array<vk::WriteDescriptorSet, 1> writes{
          vk::WriteDescriptorSet{}
              .setDstBinding(0)
              .setDstArrayElement(0)
              .setDstSet(ref.uniform_descriptorsets[i].get())
              .setDescriptorCount(1)
              .setDescriptorType(vk::DescriptorType::eUniformBuffer)
              .setBufferInfo(buffer_info)};

      core->device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                          nullptr);
    }

    auto allocated_diffuse_descriptorsets =
        core->allocate_repeated_descriptorsets(
            geometry_rendering->setlayout.diffuse.get(),
            vk::DescriptorType::eCombinedImageSampler, 2);

    ref.diffuse_descriptor_pool =
        std::move(allocated_diffuse_descriptorsets.pool);
    ref.diffuse_descriptorsets =
        std::move(allocated_diffuse_descriptorsets.sets);

    if (mesh.material.has_value()) {
      if (material_t *material = model_source.find_material(*mesh.material)) {
        for (vk::UniqueDescriptorSet &diffuse_set :
             ref.diffuse_descriptorsets) {
          if (material->diffuse_sampler->get() == VK_NULL_HANDLE) {
            ALEX_WARN("Mesh texture sampler for '{}' was not set by "
                      "loader!", model_source.path()
                          .string());
          }

          const auto image_info =
              vk::DescriptorImageInfo{}
                  .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                  .setSampler(material->diffuse_sampler->get())
                  .setImageView(material->diffuse->view());

          const std::array<vk::WriteDescriptorSet, 1> writes{
              vk::WriteDescriptorSet{}
                  .setDstBinding(0)
                  .setDstArrayElement(0)
                  .setDstSet(diffuse_set.get())
                  .setDescriptorCount(1)
                  .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                  .setImageInfo(image_info)};

          core->device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                              nullptr);
        }
      } else {
        ALEX_WARN("Mesh inside model loaded from '{}' has texture but it could "
                  "not be found",
                  model_source.path().string());
      }
    } else {
      ALEX_WARN("Mesh inside model loaded from '{}' has no texture",
                model_source.path().string());
    }
  });

  return ref;
}

constexpr auto create_model_ref(alex::core_t *core,
                                geometry_rendering_t *geometry_rendering,
                                model_source_t &model_source, model_t &model)
    -> detail::static_model_ref_t {
  detail::static_model_ref_t model_ref;
  model_ref.name = model.name;

  for (mesh_t &mesh : model.meshes) {
    model_ref.meshes.push_back(detail::create_ref_for_mesh(
        core, geometry_rendering, model_source, mesh));
  }

  for (model_t &child : model.children) {
    model_ref.children.push_back(
        create_model_ref(core, geometry_rendering, model_source, child));
  }

  return model_ref;
}

} // namespace detail

constexpr auto
static_mesh_entity_t::set_model_source(alex::core_t *core,
                                       geometry_rendering_t *geometry_rendering,
                                       model_source_t &model_source) -> void {
  _static_model_ref = detail::create_model_ref(
      core, geometry_rendering, model_source, model_source.root());
}

} // namespace game
