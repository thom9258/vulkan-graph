#pragma once

#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/overlaypass_builder.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture.hpp>

#include "include_glm.hpp"
#include "mesh.hpp"

#include <ranges>

namespace game {

struct draw_info_t {
  glm::mat4 view;
  glm::mat4 projection;
  glm::mat4 model;
};

struct geometry_rendering_t {
  vk::Extent3D _extent;

  struct attachments_t {
    std::vector<alex::texture_t> color;
    std::vector<alex::texture_t> depth;
  } attachments;

  struct setlayout_t {
    vk::UniqueDescriptorSetLayout diffuse;
    vk::UniqueDescriptorSetLayout frame_uniform;
  } setlayout;

  std::optional<alex::geometrypass_t> renderpass;
  std::optional<alex::pipeline_t> pipeline;

  constexpr geometry_rendering_t(alex::core_t &core, vk::Extent3D extent)
      : _extent{extent} {

    /* ****************************************
     * Create DescriptorSet Layout for geometry pipeline
     */
    std::vector<vk::DescriptorSetLayoutBinding> frame_uniform_bindings({
        vk::DescriptorSetLayoutBinding{}
            .setStageFlags(vk::ShaderStageFlagBits::eVertex)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setBinding(0)
            .setDescriptorCount(1),
    });

    const auto frame_uniform_setinfo =
        vk::DescriptorSetLayoutCreateInfo{}
            .setFlags(vk::DescriptorSetLayoutCreateFlags())
            .setBindings(frame_uniform_bindings);

    setlayout.frame_uniform = core.device().createDescriptorSetLayoutUnique(
        frame_uniform_setinfo, nullptr);

    std::vector<vk::DescriptorSetLayoutBinding> diffuse_bindings(
        {vk::DescriptorSetLayoutBinding{}
             .setStageFlags(vk::ShaderStageFlagBits::eFragment)
             .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
             .setBinding(0)

             .setDescriptorCount(1)});
    const auto diffuse_setinfo =
        vk::DescriptorSetLayoutCreateInfo{}
            .setFlags(vk::DescriptorSetLayoutCreateFlags())
            .setBindings(diffuse_bindings);

    setlayout.diffuse =
        core.device().createDescriptorSetLayoutUnique(diffuse_setinfo, nullptr);

    alex::texture_info_t colorattachment_info;
    colorattachment_info.physical_device = core.physical_device();
    colorattachment_info.device = core.device();
    colorattachment_info.extent.setWidth(_extent.width)
        .setHeight(_extent.height);

    colorattachment_info.format = vk::Format::eR8G8B8A8Srgb;
    colorattachment_info.tiling = vk::ImageTiling::eOptimal;
    colorattachment_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
    colorattachment_info.property_flags =
        vk::MemoryPropertyFlagBits::eDeviceLocal;
    colorattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                                 vk::ImageUsageFlagBits::eTransferSrc |
                                 vk::ImageUsageFlagBits::eSampled |
                                 vk::ImageUsageFlagBits::eColorAttachment;

    for (auto _ :
         std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
      attachments.color.emplace_back(colorattachment_info);
    }

    alex::texture_info_t depthattachment_info;
    depthattachment_info.physical_device = core.physical_device();
    depthattachment_info.device = core.device();
    depthattachment_info.extent.setWidth(_extent.width)
        .setHeight(_extent.height);

    depthattachment_info.format = vk::Format::eD32Sfloat;
    depthattachment_info.tiling = vk::ImageTiling::eOptimal;
    depthattachment_info.aspect_flags = vk::ImageAspectFlagBits::eDepth;
    depthattachment_info.property_flags =
        vk::MemoryPropertyFlagBits::eDeviceLocal;

    depthattachment_info.usage =
        vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eDepthStencilAttachment;

    for (auto _ :
         std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
      attachments.depth.emplace_back(depthattachment_info);
    }

    auto const get_view = [](alex::texture_t &texture) {
      return texture.view();
    };

    auto color_views = attachments.color | std::views::transform(get_view) |
                       std::ranges::to<std::vector>();

    alex::flightframe_array_t<vk::ImageView> colorattachment_views;
    for (auto [i, view] : colorattachment_views | std::views::enumerate) {
      view = attachments.color[i].view();
    }

    alex::flightframe_array_t<vk::ImageView> depthattachment_views;
    for (auto [i, view] : depthattachment_views | std::views::enumerate) {
      view = attachments.depth[i].view();
    }

    auto geometrypass_info = alex::geometrypass_info_t(core.device())
                                 .set_color_attachments(colorattachment_views)
                                 .set_color_format(vk::Format::eR8G8B8A8Srgb)
                                 .set_color_clearvalue(0.5f, 0.0f, 0.0f, 1.0f)
                                 .set_depth_format(vk::Format::eD32Sfloat)
                                 .set_depth_attachments(depthattachment_views)
                                 .set_depth_clearvalue(1.0f)
                                 .set_extent(_extent)
                                 .set_loadop(vk::AttachmentLoadOp::eClear);

    renderpass.emplace(geometrypass_info);

    auto geometry_pipeline_info =
        alex::pipeline_info_t(core.device())
            .set_extent(_extent)
            .set_polygon_mode(vk::PolygonMode::eFill)
            .set_cull_mode(vk::CullModeFlagBits::eBack)
            .set_front_face(vk::FrontFace::eCounterClockwise)
            .set_renderpass(renderpass->renderpass())
            .set_vertex_program_path("./geometry.vert.spv")
            .set_fragment_program_path("./geometry.frag.spv")
            .add_setlayout(setlayout.frame_uniform.get())
            .add_setlayout(setlayout.diffuse.get())
            .add_vertex_input_binding(
                vk::VertexInputBindingDescription{}
                    .setBinding(0)
                    .setStride(sizeof(game::simple_vertex_t))
                    .setInputRate(vk::VertexInputRate::eVertex))
            .add_vertex_input_attribute(
                vk::VertexInputAttributeDescription{}
                    .setBinding(0)
                    .setLocation(0)
                    .setFormat(vk::Format::eR32G32B32Sfloat)
                    .setOffset(offsetof(game::simple_vertex_t, position)))
            .add_vertex_input_attribute(
                vk::VertexInputAttributeDescription{}
                    .setBinding(0)
                    .setLocation(1)
                    .setFormat(vk::Format::eR32G32B32Sfloat)
                    .setOffset(offsetof(game::simple_vertex_t, normal)))
            .add_vertex_input_attribute(
                vk::VertexInputAttributeDescription{}
                    .setBinding(0)
                    .setLocation(2)
                    .setFormat(vk::Format::eR32G32B32Sfloat)
                    .setOffset(offsetof(game::simple_vertex_t, color)))
            .add_vertex_input_attribute(
                vk::VertexInputAttributeDescription{}
                    .setBinding(0)
                    .setLocation(3)
                    .setFormat(vk::Format::eR32G32Sfloat)
                    .setOffset(offsetof(game::simple_vertex_t, texcoord)));

    std::size_t constexpr mb = 1'000'000;
    constexpr std::size_t total_memory{10 * mb};
    std::vector<std::uint8_t> memory(total_memory);
    alex::memory::arena init_arena(memory);
    pipeline.emplace(geometry_pipeline_info, init_arena);
  }
};

struct debugui_rendering_t {
  vk::Extent3D _extent;

  struct attachments_t {
    std::vector<alex::texture_t> color;
  } attachments;

  std::optional<alex::overlaypass_t> renderpass;

  constexpr debugui_rendering_t(alex::core_t &core, vk::Extent3D extent)
      : _extent{extent} {
    alex::texture_info_t colorattachment_info;
    colorattachment_info.physical_device = core.physical_device();
    colorattachment_info.device = core.device();
    colorattachment_info.extent.setWidth(_extent.width)
        .setHeight(_extent.height);

    colorattachment_info.format = vk::Format::eR8G8B8A8Srgb;
    colorattachment_info.tiling = vk::ImageTiling::eOptimal;
    colorattachment_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
    colorattachment_info.property_flags =
        vk::MemoryPropertyFlagBits::eDeviceLocal;
    colorattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                                 vk::ImageUsageFlagBits::eTransferSrc |
                                 vk::ImageUsageFlagBits::eSampled |
                                 vk::ImageUsageFlagBits::eColorAttachment;

    for (auto _ :
         std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
      attachments.color.emplace_back(colorattachment_info);
    }

    auto const get_view = [](alex::texture_t &texture) {
      return texture.view();
    };

    auto color_views = attachments.color | std::views::transform(get_view) |
                       std::ranges::to<std::vector>();

    alex::flightframe_array_t<vk::ImageView> colorattachment_views;
    for (auto [i, view] : colorattachment_views | std::views::enumerate) {
      view = attachments.color[i].view();
    }

    auto renderpass_info = alex::overlaypass_info_t(core.device())
                               .set_color_attachments(colorattachment_views)
                               .set_color_format(vk::Format::eR8G8B8A8Srgb)
                               .set_extent(_extent)
                               .set_loadop(vk::AttachmentLoadOp::eDontCare);

    renderpass.emplace(renderpass_info);
  }
};

} // namespace game
