#pragma once

#include "presentation_context.hpp"
#include "texture.hpp"
#include "texture_storage.hpp"

#include <vulkan/vulkan_enums.hpp>

#include <filesystem>
#include <functional>
#include <variant>
#include <vector>

namespace alex::graph {

enum class resource_usage_t { write, read };

struct texture_info_t {
  texture_info_t(std::string_view name);
  texture_info_t &set_format(vk::Format format);
  texture_info_t &set_extent(vk::Extent3D extent);
  texture_info_t &set_aspect_flags(vk::ImageAspectFlags flags);

  std::string name;
  vk::Format format;
  vk::Extent3D extent;
  vk::ImageAspectFlags aspect_flags;
};

enum class attachment_type_t { color, depth };

struct attachment_info_t {
  attachment_info_t(std::string_view name, attachment_type_t type);
  attachment_info_t &set_format(vk::Format format);
  attachment_info_t &set_extent(vk::Extent3D extent);
  attachment_info_t &set_aspect_flags(vk::ImageAspectFlags flags);

  std::string name;
  attachment_type_t type;
  vk::Format format;
  vk::Extent3D extent;
  vk::ImageAspectFlags aspect_flags;
};

struct renderpass_info_t {
  renderpass_info_t(std::string_view name);
  renderpass_info_t &set_extent(vk::Extent3D extent);
  renderpass_info_t &set_vertex_program_path(std::filesystem::path path);
  renderpass_info_t &set_fragment_program_path(std::filesystem::path path);
  renderpass_info_t &set_depth_attachment(std::string_view name);
  renderpass_info_t &set_color_attachment(std::string_view name);
  renderpass_info_t &add_input(std::string_view name, resource_usage_t usage);
  renderpass_info_t &add_output(std::string_view name);
  renderpass_info_t &add_dependency(std::string_view name);

  struct name_and_usage_t {
    std::string name;
    resource_usage_t usage;
  };

  std::string name;
  vk::Extent3D extent;
  std::filesystem::path vertex_program_path;
  std::filesystem::path fragment_program_path;
  std::optional<std::string> depth_attachment;
  std::optional<std::string> color_attachment;
  std::vector<name_and_usage_t> inputs;
  std::vector<std::string> outputs;
  std::vector<std::string> dependencies;
};

struct uploadpass_info_t {
  uploadpass_info_t(std::string_view name);
  uploadpass_info_t &add_dependency(std::string_view dependency);

  std::string name{""};
  std::vector<std::string> dependencies;
};

struct graph_info_t {
  graph_info_t(vk::PhysicalDevice physical_device, vk::Device device, vk::CommandPool commandpool);
  attachment_info_t &add_attachment(std::string_view name,
                                    attachment_type_t type);
  texture_info_t &add_texture(std::string_view name);
  renderpass_info_t &add_framepass(std::string_view name);
  uploadpass_info_t &add_uploadpass(std::string_view name);

  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::CommandPool commandpool;
  std::vector<texture_info_t> texture_infos;
  std::vector<attachment_info_t> attachment_infos;
  std::vector<renderpass_info_t> framepass_infos;
  std::vector<uploadpass_info_t> uploadpass_infos;
};

} // namespace alex::graph
