#pragma once

#include <cstdint>

namespace alex {

struct draw_info_t {
    std::uint32_t vertices_count = 0;
    std::uint32_t instance_count = 1;
    std::uint32_t first_vertex = 0;
    std::uint32_t first_instance = 0;
};

struct vertex_t {
  float position[3];
  float normal[3];
  float color[3];
  float texcoord[2];
};

}
