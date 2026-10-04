#include "renderer.hpp"
#include "world_creator/ui/entity_hierarchy.hpp"
#include "world_creator/ui/game_manager.hpp"
#include "world_creator/ui/imgui_context.hpp"
#include "world_creator/ui/level_settings.hpp"

namespace game {

renderer_t::renderer_t(renderer_info_t &info)
    : _physical_device{info.physical_device}, _device{info.device},
      _render_extent{info.render_extent}, _display_extent{info.display_extent} {

  construct_geometry_renderer();
  construct_debugui_renderer();
  construct_frame();
}

auto renderer_t::construct_geometry_renderer() -> void {
  {
    const auto bindings = std::vector<vk::DescriptorSetLayoutBinding>({
        vk::DescriptorSetLayoutBinding{}
            .setStageFlags(vk::ShaderStageFlagBits::eVertex)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setBinding(0)
            .setDescriptorCount(1),
    });

    const auto setinfo = vk::DescriptorSetLayoutCreateInfo{}
                             .setFlags(vk::DescriptorSetLayoutCreateFlags())
                             .setBindings(bindings);

    _geometry.frame_uniform_setlayout =
        _device.createDescriptorSetLayoutUnique(setinfo, nullptr);
  }
  {
    const auto bindings = std::vector<vk::DescriptorSetLayoutBinding>({
        vk::DescriptorSetLayoutBinding{}
            .setStageFlags(vk::ShaderStageFlagBits::eFragment)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setBinding(0)
            .setDescriptorCount(1),
    });

    const auto setinfo = vk::DescriptorSetLayoutCreateInfo{}
                             .setFlags(vk::DescriptorSetLayoutCreateFlags())
                             .setBindings(bindings);

    _geometry.light_uniform_setlayout =
        _device.createDescriptorSetLayoutUnique(setinfo, nullptr);
  }
  {
    const auto bindings = std::vector<vk::DescriptorSetLayoutBinding>(
        {vk::DescriptorSetLayoutBinding{}
             .setStageFlags(vk::ShaderStageFlagBits::eFragment)
             .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
             .setBinding(0)

             .setDescriptorCount(1)});
    const auto setinfo = vk::DescriptorSetLayoutCreateInfo{}
                             .setFlags(vk::DescriptorSetLayoutCreateFlags())
                             .setBindings(bindings);

    _geometry.diffuse_setlayout =
        _device.createDescriptorSetLayoutUnique(setinfo, nullptr);
  }
  {
    alex::texture_info_t colorattachment_info;
    colorattachment_info.physical_device = _physical_device;
    colorattachment_info.device = _device;
    colorattachment_info.extent.setWidth(_render_extent.width)
        .setHeight(_render_extent.height);

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
      _geometry.color_attachments.emplace_back(colorattachment_info);
    }
  }
  {
    alex::texture_info_t depthattachment_info;
    depthattachment_info.physical_device = _physical_device;
    depthattachment_info.device = _device;
    depthattachment_info.extent.setWidth(_render_extent.width)
        .setHeight(_render_extent.height);

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
      _geometry.depth_attachments.emplace_back(depthattachment_info);
    }
  }
  const auto extent = vk::Extent3D{}
                          .setWidth(_render_extent.width)
                          .setHeight(_render_extent.height)
                          .setDepth(1);

  {
    auto const get_view = [](alex::texture_t &texture) {
      return texture.view();
    };

    auto color_views = _geometry.color_attachments |
                       std::views::transform(get_view) |
                       std::ranges::to<std::vector>();

    alex::flightframe_array_t<vk::ImageView> colorattachment_views;
    for (auto [i, view] : colorattachment_views | std::views::enumerate) {
      view = _geometry.color_attachments[i].view();
    }

    alex::flightframe_array_t<vk::ImageView> depthattachment_views;
    for (auto [i, view] : depthattachment_views | std::views::enumerate) {
      view = _geometry.depth_attachments[i].view();
    }
    auto geometrypass_info = alex::geometrypass_info_t(_device)
                                 .set_color_attachments(colorattachment_views)
                                 .set_color_format(vk::Format::eR8G8B8A8Srgb)
                                 .set_color_clearvalue(0.5f, 0.0f, 0.0f, 1.0f)
                                 .set_depth_format(vk::Format::eD32Sfloat)
                                 .set_depth_attachments(depthattachment_views)
                                 .set_depth_clearvalue(1.0f)
                                 .set_extent(extent)
                                 .set_loadop(vk::AttachmentLoadOp::eClear);

    _geometry.renderpass.emplace(geometrypass_info);
  }
  {
    auto geometry_pipeline_info =
        alex::pipeline_info_t(_device)
            .set_extent(extent)
            .set_polygon_mode(vk::PolygonMode::eFill)
            .set_cull_mode(vk::CullModeFlagBits::eBack)
            .set_front_face(vk::FrontFace::eCounterClockwise)
            .set_renderpass(_geometry.renderpass->renderpass())
            .set_vertex_program_path("./geometry.vert.spv")
            .set_fragment_program_path("./geometry.frag.spv")
            .add_setlayout(_geometry.frame_uniform_setlayout.get())
            .add_setlayout(_geometry.diffuse_setlayout.get())
            .add_setlayout(_geometry.light_uniform_setlayout.get())
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

    _geometry.pipeline.emplace(geometry_pipeline_info);
  }
}

auto renderer_t::construct_debugui_renderer() -> void {
  {
    alex::texture_info_t colorattachment_info;
    colorattachment_info.physical_device = _physical_device;
    colorattachment_info.device = _device;
    colorattachment_info.extent = _display_extent;
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
      _debugui.color_attachments.emplace_back(colorattachment_info);
    }
  }
  {
    alex::flightframe_array_t<vk::ImageView> colorattachment_views;
    for (auto [i, view] : colorattachment_views | std::views::enumerate) {
      view = _debugui.color_attachments[i].view();
    }

    const auto extent = vk::Extent3D{}
                            .setWidth(_display_extent.width)
                            .setHeight(_display_extent.height)
                            .setDepth(1);

    auto renderpass_info = alex::overlaypass_info_t(_device)
                               .set_color_attachments(colorattachment_views)
                               .set_color_format(vk::Format::eR8G8B8A8Srgb)
                               .set_extent(extent)
                               .set_loadop(vk::AttachmentLoadOp::eDontCare);

    _debugui.renderpass.emplace(renderpass_info);
  }
}

auto renderer_t::construct_frame() -> void {
  for (vk::UniqueSemaphore &semaphore : _frame.rendergraph_semaphores) {
    semaphore = _device.createSemaphoreUnique(vk::SemaphoreCreateInfo{});
  }
}

auto renderer_t::draw_frame(renderer_draw_frame_info_t &info)
    -> draw_frame_result_t {
  _frame.rendergraphs[info.flightframe] = alex::graph_t();
  alex::graph_t &graph = _frame.rendergraphs[info.flightframe];

  constexpr const auto upload_task_id = alex::task_id_t(0);
  constexpr const auto geometry_task_id = alex::task_id_t(1);
  constexpr const auto debugui_task_id = alex::task_id_t(2);
  constexpr const auto blit_geometry_to_debugui_task_id = alex::task_id_t(3);

  graph.add_dependency(alex::dependency_info_t{
      .device = _device, .parent = upload_task_id, .child = geometry_task_id});

  graph.add_dependency(
      alex::dependency_info_t{.device = _device,
                              .parent = geometry_task_id,
                              .child = blit_geometry_to_debugui_task_id});

  graph.add_dependency(
      alex::dependency_info_t{.device = _device,
                              .parent = blit_geometry_to_debugui_task_id,
                              .child = debugui_task_id});

  graph.set_end(debugui_task_id);

  graph.add_task(upload_task_id,
                 std::make_unique<alex::simple_task_t>(
                     "upload", [&](vk::CommandBuffer commandbuffer) {
                       for (entity_t &entity : info.world->entities()) {
                         if (auto *static_mesh =
                                 entity.get<static_mesh_entity_t>()) {
                           ALEX_ERROR("HAVE NOT IMPLEMENTED UPDATE YET");
                           // static_mesh_entity_update_info_t update_info;
                           // update_info.physical_device = _physical_device;
                           // update_info.device = _device;
                           // update_info.commandbuffer = commandbuffer;
                           // update_info.flightframe = info.flightframe;
                           // update_info.transform_hierarchy =
                           // &world.transform_hierarchy();
                           // update_info.camera_view = world.camera().view();
                           // update_info.camera_projection =
                           // world.camera().projection();
                           // static_mesh->resource_update(update_info);
                         }
                       }
                     }));

  graph.add_task(
      blit_geometry_to_debugui_task_id,
      std::make_unique<alex::simple_task_t>(
          "blit geometry to ui texture", [&](vk::CommandBuffer commandbuffer) {
            // Transition ui texture image to transfer dst
            {
              auto range = vk::ImageSubresourceRange{}
                               .setAspectMask(vk::ImageAspectFlagBits::eColor)
                               .setBaseMipLevel(0)
                               .setLevelCount(1)
                               .setBaseArrayLayer(0)
                               .setLayerCount(1);

              auto barrier =
                  vk::ImageMemoryBarrier{}
                      .setImage(
                          _debugui.color_attachments[info.flightframe].image())
                      .setSubresourceRange(range)
                      .setOldLayout(vk::ImageLayout::eUndefined)
                      .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                      .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                      .setDstAccessMask(vk::AccessFlags())
                      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

              commandbuffer.pipelineBarrier(
                  vk::PipelineStageFlagBits::eTransfer,
                  vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
                  nullptr, nullptr, barrier);
            }

            // Transition geometry rendertarget to transfer src
            {
              auto range = vk::ImageSubresourceRange{}
                               .setAspectMask(vk::ImageAspectFlagBits::eColor)
                               .setBaseMipLevel(0)
                               .setLevelCount(1)
                               .setBaseArrayLayer(0)
                               .setLayerCount(1);

              auto barrier =
                  vk::ImageMemoryBarrier{}
                      .setImage(
                          _geometry.color_attachments[info.flightframe].image())
                      .setSubresourceRange(range)
                      .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
                      .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                      .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                      .setDstAccessMask(vk::AccessFlags())
                      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

              commandbuffer.pipelineBarrier(
                  vk::PipelineStageFlagBits::eTransfer,
                  vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
                  nullptr, nullptr, barrier);
            }

            // Blit geometry texture to ui texture
            {
              auto src_subresource =
                  vk::ImageSubresourceLayers{}
                      .setAspectMask(vk::ImageAspectFlagBits::eColor)
                      .setBaseArrayLayer(0)
                      .setLayerCount(1)
                      .setMipLevel(0);
              const std::array<vk::Offset3D, 2> src_offsets{
                  vk::Offset3D{0, 0, 0},
                  vk::Offset3D{static_cast<std::int32_t>(_render_extent.width),
                               static_cast<std::int32_t>(_render_extent.height),
                               1}};

              auto dst_subresource =
                  vk::ImageSubresourceLayers{}
                      .setAspectMask(vk::ImageAspectFlagBits::eColor)
                      .setBaseArrayLayer(0)
                      .setLayerCount(1)
                      .setMipLevel(0);

              const std::array<vk::Offset3D, 2> dst_offsets{
                  vk::Offset3D{0, 0, 0},
                  vk::Offset3D{
                      static_cast<std::int32_t>(_display_extent.width),
                      static_cast<std::int32_t>(_display_extent.height), 1}};

              auto image_blit = vk::ImageBlit{}
                                    .setSrcOffsets(src_offsets)
                                    .setSrcSubresource(src_subresource)
                                    .setDstOffsets(dst_offsets)
                                    .setDstSubresource(dst_subresource);

              commandbuffer.blitImage(
                  _geometry.color_attachments[info.flightframe].image(),
                  vk::ImageLayout::eTransferSrcOptimal,
                  _debugui.color_attachments[info.flightframe].image(),
                  vk::ImageLayout::eTransferDstOptimal, image_blit,
                  vk::Filter::eNearest);
            }
          }));

  graph.add_task(
      geometry_task_id,
      std::make_unique<alex::simple_task_t>(
          "geometry", [&](vk::CommandBuffer commandbuffer) {
            const auto render_area =
                vk::Rect2D{}
                    .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                    .setExtent(_render_extent);

            auto clearvalues = _geometry.renderpass->clearvalues();

            const auto renderpass_begin_info =
                vk::RenderPassBeginInfo{}
                    .setRenderPass(_geometry.renderpass->renderpass())
                    .setFramebuffer(
                        _geometry.renderpass->framebuffer(info.flightframe))
                    .setRenderArea(render_area)
                    .setClearValues(clearvalues);

            commandbuffer.beginRenderPass(renderpass_begin_info,
                                          vk::SubpassContents::eInline);

            auto viewport =
                vk::Viewport{}
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<float>(_render_extent.width))
                    .setHeight(static_cast<float>(_render_extent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);

            auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
                {_render_extent.width, _render_extent.height});

            commandbuffer.setViewport(0, viewport);
            commandbuffer.setScissor(0, scissor);
            if (_geometry.pipeline->pipeline() == VK_NULL_HANDLE) {
                ALEX_ERROR("GEOMETRY PIPELINE WAS NULL HANDLE");
            }
            commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       _geometry.pipeline->pipeline());

            for (entity_t &entity : info.world->entities()) {
              if (auto *static_mesh = entity.get<static_mesh_entity_t>()) {
                ALEX_ERROR("HAVE NOT IMPLEMENTED DRAW YET");
                // static_mesh_entity_draw_info_t info;
                // info.commandbuffer = commandbuffer;
                // info.flightframe = info.flightframe;
                // info.geometry_pipeline_layout = _geometry.pipeline->layout();
                // static_mesh->draw(info);
              }
            }
            commandbuffer.endRenderPass();
          }));

  graph.add_task(
      debugui_task_id,
      std::make_unique<alex::simple_task_t>(
          "debugui", [&](vk::CommandBuffer commandbuffer) {
            const auto render_area =
                vk::Rect2D{}
                    .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                    .setExtent(_render_extent);

            const auto renderpass_begin_info =
                vk::RenderPassBeginInfo{}
                    .setRenderPass(_debugui.renderpass->renderpass())
                    .setFramebuffer(
                        _debugui.renderpass->framebuffer(info.flightframe))
                    .setRenderArea(render_area);

            commandbuffer.beginRenderPass(renderpass_begin_info,
                                          vk::SubpassContents::eInline);

            auto viewport =
                vk::Viewport{}
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<float>(_display_extent.width))
                    .setHeight(static_cast<float>(_display_extent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);

            auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
                {_display_extent.width, _display_extent.height});

            commandbuffer.setViewport(0, viewport);
            commandbuffer.setScissor(0, scissor);

            info.game_manager->draw();

            if (info.game_manager->show_entity_hierarchy()) {
              info.entity_hierarchy->draw(*info.world);
            }

            if (info.game_manager->show_level_settings()) {
              info.level_settings->draw();
              _geometry.renderpass->set_color_clearvalue(
                  info.world->background_color().r(),
                  info.world->background_color().g(),
                  info.world->background_color().b());
            }

            ImGui::Render();
            ImDrawData *draw_data = ImGui::GetDrawData();
            info.imgui_context->render_draw_data(draw_data, commandbuffer);

            commandbuffer.endRenderPass();
          }));

  info.imgui_context->new_frame();

  vk::Semaphore graph_finished_semaphore =
      _frame.rendergraph_semaphores[info.flightframe].get();

  auto graph_evaluate_info = alex::graph_evaluate_info_t{};
  graph_evaluate_info.device = info.device;
  graph_evaluate_info.commandpool = info.commandpool;
  graph_evaluate_info.queue = info.queue;
  graph_evaluate_info.sync_semaphore = graph_finished_semaphore;
  graph.evaluate(graph_evaluate_info);

  draw_frame_result_t result;
  result.semaphore = graph_finished_semaphore;
  result.image = _debugui.color_attachments[info.flightframe].image();
  result.extent = _display_extent;
  return result;
}

auto renderer_t::debugui_renderpass() -> vk::RenderPass {
  return _debugui.renderpass->renderpass();
}

} // namespace game
