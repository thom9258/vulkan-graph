#pragma once

#include "drawing.hpp"
#include "arena.hpp"
#include "ensure.hpp"

#define SIMPLE_GEOMETRY_IMPLEMENTATION
#include <simple_geometry.h>

#include <ranges>

std::span<alex::vertex_t> load_cube(alex::memory::arena &arena) {
  sg_status status;
  sg_cube_info info;
  info.width = 1.0f;
  info.height = 1.0f;
  info.depth = 1.0f;

  size_t vertices_size{0};
  status = sg_cube_vertices(&info, &vertices_size, nullptr, nullptr, nullptr);
  ENSURE(sg_success(status), "Could not load vertices size")
  auto positions = arena.allocate<sg_position>(vertices_size);
  ENSURE_NOT(positions.empty(), "Could not allocate positions buffer")

  status = sg_cube_vertices(&info, &vertices_size, positions.data(), nullptr,
                            nullptr);
  ENSURE(sg_success(status), "Could not load vertices")

  auto normals = arena.allocate<sg_normal>(vertices_size);
  ENSURE_NOT(normals.empty(), "Could not allocate normals buffer")
  status = sg_cube_vertices(&info, &vertices_size, nullptr, normals.data(),
                            nullptr);
  ENSURE(sg_success(status), "Could not load vertices")

  auto vertices = arena.allocate<alex::vertex_t>(vertices_size);
  ENSURE_NOT(vertices.empty(), "Could not allocate vertices buffer")

  for (auto [i, vertex] : vertices | std::views::enumerate) {
	  vertex.position[0] = positions[i].x;
	  vertex.position[1] = positions[i].y;
	  vertex.position[2] = positions[i].z;
	  vertex.normal[0] = normals[i].x;
	  vertex.normal[1] = normals[i].y;
	  vertex.normal[2] = normals[i].z;
	  vertex.color[0] = 1.0f;
	  vertex.color[1] = 0.0f;
	  vertex.color[2] = 0.0f;
	  vertex.texcoord[0] = 0.0f;
	  vertex.texcoord[1] = 0.0f;
  }

  return vertices;
}
