#pragma once

#include <alex/arena.hpp>
#include <alex/drawing.hpp>
#include <alex/log.hpp>

#define SIMPLE_GEOMETRY_IMPLEMENTATION
#include <simple_geometry.h>

#include <ranges>

constexpr auto load_cube(alex::memory::arena &arena, float r, float g, float b)
    -> std::span<alex::vertex_t> {
  sg_status status;
  sg_cube_info info;
  info.width = 1.0f;
  info.height = 1.0f;
  info.depth = 1.0f;

  size_t vertices_size{0};
  status = sg_cube_vertices(&info, &vertices_size, nullptr, nullptr, nullptr);
  ALEX_ERROR_IF(!sg_success(status), "Could not load vertices size")
  auto positions = arena.allocate<sg_position>(vertices_size);
  ALEX_ERROR_IF(positions.empty(), "Could not allocate positions buffer")

  status = sg_cube_vertices(&info, &vertices_size, positions.data(), nullptr,
                            nullptr);
  ALEX_ERROR_IF(!sg_success(status), "Could not load vertices")

  auto normals = arena.allocate<sg_normal>(vertices_size);
  ALEX_ERROR_IF(normals.empty(), "Could not allocate normals buffer")
  status =
      sg_cube_vertices(&info, &vertices_size, nullptr, normals.data(), nullptr);
  ALEX_ERROR_IF(!sg_success(status), "Could not load vertices")

  auto vertices = arena.allocate<alex::vertex_t>(vertices_size);
  ALEX_ERROR_IF(vertices.empty(), "Could not allocate vertices buffer")

  for (auto [i, vertex] : vertices | std::views::enumerate) {
    vertex.position[0] = positions[i].x;
    vertex.position[1] = positions[i].y;
    vertex.position[2] = positions[i].z;
    vertex.normal[0] = normals[i].x;
    vertex.normal[1] = normals[i].y;
    vertex.normal[2] = normals[i].z;
    vertex.color[0] = r;
    vertex.color[1] = g;
    vertex.color[2] = b;
    vertex.texcoord[0] = 0.0f;
    vertex.texcoord[1] = 0.0f;
  }

  return vertices;
}
