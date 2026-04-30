#include "graph_builder.hpp"

namespace alex::graph {

texture_info_t::texture_info_t(std::string_view name)
    : name{std::string(name)} {}

texture_info_t &texture_info_t::set_format(vk::Format format) {
  this->format = format;
  return *this;
}

texture_info_t &texture_info_t::set_extent(vk::Extent3D extent) {
  this->extent = extent;
  return *this;
}

texture_info_t &texture_info_t::set_aspect_flags(vk::ImageAspectFlags flags) {
  this->aspect_flags = flags;
  return *this;
}

attachment_info_t::attachment_info_t(std::string_view name,
                                     attachment_type_t type)
    : name{std::string(name)}, type{type} {}

attachment_info_t &attachment_info_t::set_format(vk::Format format) {
  this->format = format;
  return *this;
}

attachment_info_t &attachment_info_t::set_extent(vk::Extent3D extent) {
  this->extent = extent;
  return *this;
}

attachment_info_t &
attachment_info_t::set_aspect_flags(vk::ImageAspectFlags flags) {
  this->aspect_flags = flags;
  return *this;
}

renderpass_info_t::renderpass_info_t(std::string_view name) : name{name} {}

renderpass_info_t &
renderpass_info_t::set_color_attachment(std::string_view name) {
  color_attachment = std::string(name);
  return *this;
}

renderpass_info_t &
renderpass_info_t::set_depth_attachment(std::string_view name) {
  depth_attachment = std::string(name);
  return *this;
}

renderpass_info_t &renderpass_info_t::add_input(std::string_view name,
                                                resource_usage_t usage) {
  inputs.emplace_back(std::string(name), usage);
  return *this;
}

renderpass_info_t &renderpass_info_t::add_dependency(std::string_view name) {
  dependencies.emplace_back(std::string(name));
  return *this;
}

renderpass_info_t &renderpass_info_t::add_output(std::string_view name) {
  outputs.emplace_back(name);
  return *this;
}

renderpass_info_t &renderpass_info_t::set_extent(vk::Extent3D extent) {
  this->extent = extent;
  return *this;
}

renderpass_info_t &
renderpass_info_t::set_vertex_program_path(std::filesystem::path path) {
  vertex_program_path = path;
  return *this;
}

renderpass_info_t &
renderpass_info_t::set_fragment_program_path(std::filesystem::path path) {
  fragment_program_path = path;
  return *this;
}

uploadpass_info_t::uploadpass_info_t(std::string_view name) : name{name} {}

uploadpass_info_t &
uploadpass_info_t::add_dependency(std::string_view dependency) {
  dependencies.emplace_back(dependency);
  return *this;
}

graph_info_t::graph_info_t(vk::PhysicalDevice physical_device,
                           vk::Device device, vk::CommandPool commandpool)
    : physical_device{physical_device}, device{device}, commandpool{commandpool} {}

renderpass_info_t &graph_info_t::add_framepass(std::string_view name) {
  framepass_infos.emplace_back(name);
  return framepass_infos.back();
}

uploadpass_info_t &graph_info_t::add_uploadpass(std::string_view name) {
  uploadpass_infos.emplace_back(name);
  return uploadpass_infos.back();
}

texture_info_t &graph_info_t::add_texture(std::string_view name) {
  texture_infos.emplace_back(name);
  return texture_infos.back();
}

attachment_info_t &graph_info_t::add_attachment(std::string_view name,
                                                attachment_type_t type) {
  attachment_infos.emplace_back(name, type);
  return attachment_infos.back();
}

} // namespace alex::graph
