#include <alex/core.hpp>
#include <alex/geometrypass_builder.hpp>
#include <alex/memory_buffer.hpp>
#include <alex/pipeline_builder.hpp>
#include <alex/presentation_context.hpp>
#include <alex/texture_storage.hpp>

#include <alex/task_graph.hpp>

#include "alex/flightframe_array.hpp"

#include "../utility/button.hpp"
#include "../utility/sdl.hpp"
#include "../utility/scenestack.hpp"

#include "chest_scene.hpp"
#include "bitmap.hpp"
#include "ecs/manager.hpp"
#include "ecs/system_model.hpp"
#include "ecs/system_orbit_camera.hpp"

#include "resource_loader.hpp"

#include <chrono>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

using namespace std::literals;

auto constexpr print_model_names(int indent, game::model_t &model) -> void {
  for (int i = 0; i < indent; i++) {
    std::print(" ");
  }

  std::println("{}", model.name);
  for (game::model_t &child : model.children) {
    print_model_names(indent++, child);
  }
}

std::size_t constexpr mb = 1'000'000;

int main() {
  constexpr std::size_t total_memory{10 * mb};
  std::vector<std::uint8_t> memory(total_memory);
  alex::memory::arena init_arena(memory);

  auto program_start_time = std::chrono::high_resolution_clock::now();

  sdl::window_info_t window_info{};
  window_info.name = "ps1_game";
  window_info.x = -1;
  window_info.y = -1;
  window_info.width = 320 * 2;
  window_info.height = 240 * 2;

  sdl::window_t window(window_info);

  std::vector<const char *> window_extensions = window.instance_extensions();

  alex::context_info_t context_info;
  context_info.instance_extensions = window_extensions;
  context_info.enable_validation = true;

  alex::context_t context(context_info);

  vk::UniqueSurfaceKHR window_surface =
      window.create_window_surface(context.instance());

  alex::core_info_t core_info;
  core_info.surface = window_surface.get();
  core_info.instance = context.instance();
  vk::Extent3D render_extent(static_cast<std::int32_t>(window_info.width / 2),
                             static_cast<std::int32_t>(window_info.height / 2),
                             1);

  alex::core_t core(core_info);

  /* ****************************************
   * Setup presentation context
   */
  sdl::window_extent_t window_extent = window.window_extent();
  alex::presenter_info_t presenter_info;
  presenter_info.physical_device = core.physical_device();
  presenter_info.device = core.device();
  presenter_info.commandpool = core.commandpool();
  presenter_info.enable_vsync = false;
  presenter_info.window_surface = window_surface.get();
  presenter_info.window_extent.width = window_extent.width;
  presenter_info.window_extent.height = window_extent.height;
  alex::presenter_t presenter(presenter_info);

  /* ****************************************
   * Create Scenes
   */
  scene::scenestack_t scenestack;
  scenestack.put(std::make_unique<game::chest_scene>());

  scenestack.try_pop();

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

  vk::UniqueDescriptorSetLayout geometry_pipeline_frame_uniform_setlayout =
      core.device().createDescriptorSetLayoutUnique(frame_uniform_setinfo,
                                                    nullptr);

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

  vk::UniqueDescriptorSetLayout geometry_pipeline_diffuse_setlayout =
      core.device().createDescriptorSetLayoutUnique(diffuse_setinfo, nullptr);

  /* ****************************************
   * Setup rendering
   */

  alex::texture_info_t colorattachment_info;
  colorattachment_info.physical_device = core.physical_device();
  colorattachment_info.device = core.device();
  colorattachment_info.extent.setWidth(render_extent.width)
      .setHeight(render_extent.height);

  colorattachment_info.format = vk::Format::eR8G8B8A8Srgb;
  colorattachment_info.tiling = vk::ImageTiling::eOptimal;
  colorattachment_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
  colorattachment_info.property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  colorattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                               vk::ImageUsageFlagBits::eTransferSrc |
                               vk::ImageUsageFlagBits::eSampled |
                               vk::ImageUsageFlagBits::eColorAttachment;

  std::vector<alex::texture_t> colorattachments;
  for (auto _ :
       std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
    colorattachments.emplace_back(colorattachment_info);
  }

  alex::texture_info_t depthattachment_info;
  depthattachment_info.physical_device = core.physical_device();
  depthattachment_info.device = core.device();
  depthattachment_info.extent.setWidth(render_extent.width)
      .setHeight(render_extent.height);

  depthattachment_info.format = vk::Format::eD32Sfloat;
  depthattachment_info.tiling = vk::ImageTiling::eOptimal;
  depthattachment_info.aspect_flags = vk::ImageAspectFlagBits::eDepth;
  depthattachment_info.property_flags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;

  depthattachment_info.usage = vk::ImageUsageFlagBits::eTransferDst |
                               vk::ImageUsageFlagBits::eTransferSrc |
                               vk::ImageUsageFlagBits::eSampled |
                               vk::ImageUsageFlagBits::eDepthStencilAttachment;

  std::vector<alex::texture_t> depthattachments;
  for (auto _ :
       std::views::iota(0) | std::views::take(alex::frames_in_flight)) {
    depthattachments.emplace_back(depthattachment_info);
  }

  alex::flightframe_array_t<vk::ImageView> colorattachment_views;
  for (auto [i, view] : colorattachment_views | std::views::enumerate) {
    view = colorattachments[i].view();
  }

  alex::flightframe_array_t<vk::ImageView> depthattachment_views;
  for (auto [i, view] : depthattachment_views | std::views::enumerate) {
    view = depthattachments[i].view();
  }

  auto geometrypass_info = alex::geometrypass_info_t(core.device())
                               .set_color_attachments(colorattachment_views)
                               .set_color_format(vk::Format::eR8G8B8A8Srgb)
                               .set_color_clearvalue(0.0f, 0.0f, 0.0f, 1.0f)
                               .set_depth_format(vk::Format::eD32Sfloat)
                               .set_depth_attachments(depthattachment_views)
                               .set_depth_clearvalue(1.0f)
                               .set_extent(render_extent)
                               .set_loadop(vk::AttachmentLoadOp::eClear);

  alex::geometrypass_t geometry_pass(geometrypass_info);

  auto geometry_pipeline_info =
      alex::pipeline_info_t(core.device())
          .set_extent(render_extent)
          .set_polygon_mode(vk::PolygonMode::eFill)
          .set_cull_mode(vk::CullModeFlagBits::eBack)
          .set_front_face(vk::FrontFace::eCounterClockwise)
          .set_renderpass(geometry_pass.renderpass())
          .set_vertex_program_path("./geometry.vert.spv")
          .set_fragment_program_path("./geometry.frag.spv")
          .add_setlayout(geometry_pipeline_frame_uniform_setlayout.get())
          .add_setlayout(geometry_pipeline_diffuse_setlayout.get())
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

  alex::pipeline_t geometry_pipeline(geometry_pipeline_info, init_arena);

  alex::flightframe_array_t<vk::UniqueSemaphore> taskgraph_semaphores;
  for (vk::UniqueSemaphore &semaphore : taskgraph_semaphores) {
    semaphore = core.create_semaphore();
  }

  alex::flightframe_array_t<alex::graph_t> taskgraphs;

  /* ****************************************
   * Setup ecs
   */
  std::array<ecs::manager_t::entity_t, max_entities> entity_memory;
  std::array<component_drawable_t, max_entities> mesh_components;
  std::array<component_transform_t, max_entities> transform_components;
  std::array<component_orbit_camera_t, orbit_camera_components_max>
      orbit_camera_components;

  ecs::manager_t manager(entity_memory, mesh_components, transform_components,
                         orbit_camera_components);

  /* ****************************************
   * Load resources
   */
  // TODO: we must add the ability to provide a staging scratch buffer
  //       for optimal memory usage
  game::model_load_info_t chest_load_info;
  chest_load_info.core = &core;
  chest_load_info.path = "/home/th/Assets/ChestWowStyle/Chest.obj";
  // chest_load_info.path = "/home/th/Assets/Fox/glTF/Fox.gltf";
  auto chest = game::model_source_t::create(chest_load_info);
  if (!chest.has_value()) {
    std::println("resource load error: [code: {}] {}",
                 game::to_string(chest.error().code()), chest.error().error());
    return 1;
  }

  std::println("model: (loadtime: {}s) {}", chest.value().loadtime_seconds(),
               chest.value().root().name);

  print_model_names(1, chest.value().root());

  auto chest_diffuse_bitmap = game::bitmap_t::create(
      "/home/th/Assets/ChestWowStyle/diffuse.tga", game::bitmap_format_t::rgb);

  if (!chest_diffuse_bitmap.has_value()) {
    std::println("Could not load chest diffuse bitmap");
    return 1;
  }

  alex::direct_memory_buffer_t chest_diffuse_buffer =
      chest_diffuse_bitmap->make_direct_buffer(core.physical_device(),
                                               core.device());

  const vk::Format chest_diffuse_texture_format =
      game::to_vk_format(chest_diffuse_bitmap->format());

  alex::texture_info_t chest_diffuse_texture_info;
  chest_diffuse_texture_info.physical_device = core.physical_device();
  chest_diffuse_texture_info.device = core.device();
  chest_diffuse_texture_info.extent =
      vk::Extent2D{static_cast<std::uint32_t>(chest_diffuse_bitmap->width()),
                   static_cast<std::uint32_t>(chest_diffuse_bitmap->height())};
  chest_diffuse_texture_info.format = chest_diffuse_texture_format;

  chest_diffuse_texture_info.aspect_flags = vk::ImageAspectFlagBits::eColor;
  chest_diffuse_texture_info.usage =
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

  alex::texture_t chest_diffuse_texture(chest_diffuse_texture_info);

  core.immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
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
                         .setImage(chest_diffuse_texture.image())
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
                              .setWidth(chest_diffuse_bitmap->width())
                              .setHeight(chest_diffuse_bitmap->height())
                              .setDepth(1);

      auto region = vk::BufferImageCopy{}
                        .setBufferOffset(0)
                        .setBufferRowLength(0)
                        .setBufferImageHeight(0)
                        .setImageSubresource(layer)
                        .setImageOffset(offset)
                        .setImageExtent(extent);

      commandbuffer.copyBufferToImage(
          chest_diffuse_buffer.buffer(), chest_diffuse_texture.image(),
          vk::ImageLayout::eTransferDstOptimal, region);
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
                         .setImage(chest_diffuse_texture.image())
                         .setSubresourceRange(source_range)
                         .setSrcAccessMask(vk::AccessFlags())
                         .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

      commandbuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::DependencyFlags(), nullptr, nullptr,
                                    barrier);
    }
  });

  // Construct a sampler for the texture
  const auto features = core.physical_device().getFeatures();
  const auto properties = core.physical_device().getProperties();
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

  vk::UniqueSampler diffuse_texture_sampler =
      core.device().createSamplerUnique(sampler_info);

  auto allocated_diffuse_sampler_descriptorsets =
      core.allocate_repeated_descriptorsets(
          geometry_pipeline_diffuse_setlayout.get(),
          vk::DescriptorType::eCombinedImageSampler, 2);

  /* ****************************************
   * Setup entities
   */
  ecs::entity_id_t chest_entity = manager.new_entity();
  {
    auto *transform =
        manager.add_component<component_transform_t>(chest_entity);
    transform->mat = glm::scale(glm::mat4(1.0f), glm::vec3(0.04f));

    auto *mesh = manager.add_component<component_drawable_t>(chest_entity);
    mesh->vertices =
        &(chest.value().root().children.at(0).meshes.at(0).vertices.value());
    mesh->vertices_length =
        chest.value().root().children.at(0).meshes.at(0).vertices_length;

    mesh->indices =
        &(chest.value().root().children.at(0).meshes.at(0).indices.value());
    mesh->indices_length =
        chest.value().root().children.at(0).meshes.at(0).indices_length;

    core.immediate_evaluate([&](vk::CommandBuffer commandbuffer) {
      draw_info_t draw_info;

      alex::direct_memory_buffer_info_t direct_uniform_info;
      direct_uniform_info.physical_device = core.physical_device();
      direct_uniform_info.device = core.device();
      direct_uniform_info.buffer_type = alex::memory_buffer_type_t::basic;
      direct_uniform_info.memory_size = sizeof(draw_info);

      for (auto &uniform : mesh->direct_uniforms) {
        uniform = alex::direct_memory_buffer_t(direct_uniform_info);
        std::memcpy(uniform->memory_ptr(), &draw_info, sizeof(draw_info));
      };

      for (auto [i, uniform] : mesh->uniforms | std::views::enumerate) {
        alex::memory_buffer_info_t uniform_info;
        uniform_info.physical_device = core.physical_device();
        uniform_info.device = core.device();
        uniform_info.buffer_type = alex::memory_buffer_type_t::uniform;
        uniform_info.memory_size = direct_uniform_info.memory_size;
        uniform = alex::memory_buffer_t(uniform_info);

        alex::memory_buffer_write_info_t write_info;
        write_info.physical_device = core.physical_device();
        write_info.device = core.device();
        write_info.direct = &mesh->direct_uniforms[i].value();
        write_info.write_size = uniform->memory_size();
        write_info.commandbuffer = commandbuffer;
        uniform->record_write(write_info);
      }

      auto allocated_frame_uniform_descriptorsets =
          core.allocate_repeated_descriptorsets(
              geometry_pipeline_frame_uniform_setlayout.get(),
              vk::DescriptorType::eUniformBuffer, 2);

      mesh->uniform_descriptor_pool =
          std::move(allocated_frame_uniform_descriptorsets.pool);
      mesh->uniform_descriptorsets =
          std::move(allocated_frame_uniform_descriptorsets.sets);

      for (auto [i, uniform] : mesh->uniforms | std::views::enumerate) {
        const auto buffer_info = vk::DescriptorBufferInfo{}
                                     .setBuffer(uniform->buffer())
                                     .setOffset(0)
                                     .setRange(uniform->memory_size());

        const std::array<vk::WriteDescriptorSet, 1> writes{
            vk::WriteDescriptorSet{}
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDstSet(mesh->uniform_descriptorsets[i].get())
                .setDescriptorCount(1)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setBufferInfo(buffer_info)};

        core.device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                           nullptr);
      }

      auto allocated_diffuse_descriptorsets =
          core.allocate_repeated_descriptorsets(
              geometry_pipeline_diffuse_setlayout.get(),
              vk::DescriptorType::eCombinedImageSampler, 2);

      mesh->diffuse_descriptor_pool =
          std::move(allocated_diffuse_descriptorsets.pool);
      mesh->diffuse_descriptorsets =
          std::move(allocated_diffuse_descriptorsets.sets);

      for (vk::UniqueDescriptorSet &diffuse_set :
           mesh->diffuse_descriptorsets) {
        const auto image_info =
            vk::DescriptorImageInfo{}
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSampler(diffuse_texture_sampler.get())
                .setImageView(chest_diffuse_texture.view());

        const std::array<vk::WriteDescriptorSet, 1> writes{
            vk::WriteDescriptorSet{}
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDstSet(diffuse_set.get())
                .setDescriptorCount(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setImageInfo(image_info)};

        core.device().updateDescriptorSets(writes.size(), writes.data(), 0,
                                           nullptr);
      }
    });
  }

  ecs::entity_id_t orbit_camera_entity = manager.new_entity();
  {
    auto *orbit_camera =
        manager.add_component<component_orbit_camera_t>(orbit_camera_entity);

    float const camera_radius = 4.0f;
    glm::vec3 const target(0.0f, 0.5f, 0.0f);
    orbit_camera->camera = OrbitCamera(target, camera_radius);

    const float aspect = window_extent.aspect();
    const float near_plane = 0.1f, far_plane = 200.0f;
    orbit_camera->projection = std::invoke([&]() {
      glm::mat4 p =
          glm::perspective(glm::radians(70.f), aspect, near_plane, far_plane);
      p[1][1] *= -1.0f;
      return p;
    });
  }

  {
    auto now = std::chrono::high_resolution_clock::now();
    auto initialization_time =
        std::chrono::duration_cast<std::chrono::duration<double>>(
            now - program_start_time);
    std::println("Initialization time: {}", initialization_time);
  }

  bool running = true;
  while (running) {
    window.mark_next_frame();
    std::span<SDL_Event> events = window.get_events();
    double const deltatime = window.deltatime_seconds();

    for (SDL_Event event : events) {
      switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          running = false;
          break;
        }
        break;
      }
    }

    ecs::system::orbit_camera_update_info_t orbit_camera_update_info;
    orbit_camera_update_info.manager = &manager;
    orbit_camera_update_info.sdl_events = events;
    orbit_camera_update_info.deltatime = deltatime;

    ecs::system::orbit_camera_update(orbit_camera_update_info);

    alex::next_frame_info_t next_frame_info =
        presenter.wait_for_next_frame(core.device());

    auto upload_task_id = alex::task_id_t(0);
    auto geometry_task_id = alex::task_id_t(1);

    taskgraphs[next_frame_info.flightframe] = alex::graph_t();
    alex::graph_t &graph = taskgraphs[next_frame_info.flightframe];

    graph.add_task(upload_task_id,
                   std::make_unique<alex::simple_task_t>(
                       "upload", [&](vk::CommandBuffer commandbuffer) {
                         ecs::system::models_upload_info_t system_upload_info;
                         system_upload_info.manager = &manager;
                         system_upload_info.physical_device =
                             core.physical_device();
                         system_upload_info.device = core.device();
                         system_upload_info.commandbuffer = commandbuffer;
                         system_upload_info.flightframe =
                             next_frame_info.flightframe;

                         ecs::system::models_upload(system_upload_info);
                       }));

    graph.add_task(
        geometry_task_id,
        std::make_unique<alex::simple_task_t>(
            "geometry", [&](vk::CommandBuffer commandbuffer) {
              const auto render_area =
                  vk::Rect2D{}
                      .setOffset(vk::Offset2D{}.setX(0.0f).setY(0.0f))
                      .setExtent(vk::Extent2D(render_extent.width,
                                              render_extent.height));

              auto clearvalues = geometry_pass.clearvalues();

              const auto renderpass_begin_info =
                  vk::RenderPassBeginInfo{}
                      .setRenderPass(geometry_pass.renderpass())
                      .setFramebuffer(geometry_pass.framebuffer(
                          next_frame_info.flightframe))
                      .setRenderArea(render_area)
                      .setClearValues(clearvalues);

              commandbuffer.beginRenderPass(renderpass_begin_info,
                                            vk::SubpassContents::eInline);

              auto viewport =
                  vk::Viewport{}
                      .setX(0)
                      .setY(0)
                      .setWidth(static_cast<float>(render_extent.width))
                      .setHeight(static_cast<float>(render_extent.height))
                      .setMinDepth(0.0f)
                      .setMaxDepth(1.0f);

              auto scissor = vk::Rect2D{}.setOffset({0, 0}).setExtent(
                  {render_extent.width, render_extent.height});

              commandbuffer.setViewport(0, viewport);
              commandbuffer.setScissor(0, scissor);
              commandbuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                         geometry_pipeline.pipeline());

              ecs::system::models_draw_info_t system_draw_info;
              system_draw_info.manager = &manager;
              system_draw_info.commandbuffer = commandbuffer;
              system_draw_info.flightframe = next_frame_info.flightframe;
              system_draw_info.geometry_pipeline_layout =
                  geometry_pipeline.layout();
              ecs::system::models_draw(system_draw_info);

              commandbuffer.endRenderPass();
            }));

    graph.add_dependency(alex::dependency_info_t{.device = core.device(),
                                                 .parent = upload_task_id,
                                                 .child = geometry_task_id});

    graph.set_end(geometry_task_id);

    vk::Semaphore graph_finished_semaphore =
        taskgraph_semaphores[next_frame_info.flightframe].get();

    auto graph_evaluate_info = alex::graph_evaluate_info_t{};
    graph_evaluate_info.device = core.device();
    graph_evaluate_info.commandpool = core.commandpool();
    graph_evaluate_info.queue = core.queue();
    graph_evaluate_info.sync_semaphore = graph_finished_semaphore;
    graph.evaluate(graph_evaluate_info);

    alex::presentation_info_t presentation_info;
    presentation_info.source_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.source_offset_end =
        vk::Offset3D{static_cast<std::int32_t>(render_extent.width),
                     static_cast<std::int32_t>(render_extent.height), 1};

    presentation_info.destination_offset_start = vk::Offset3D{0, 0, 0};
    presentation_info.destination_offset_end = vk::Offset3D{
        static_cast<std::int32_t>(presenter.window_extent.width),
        static_cast<std::int32_t>(presenter.window_extent.height), 1};

    presentation_info.blit_filter = vk::Filter::eNearest;
    presentation_info.image =
        colorattachments[next_frame_info.flightframe].image();
    presentation_info.layout = vk::ImageLayout::eColorAttachmentOptimal;
    presentation_info.queue = core.queue();
    presentation_info.wait_semaphore = graph_finished_semaphore;
    presenter.present(presentation_info);
  }

  core.device().waitIdle();

  return 0;
}
