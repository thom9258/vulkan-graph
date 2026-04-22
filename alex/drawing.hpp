#pragma once

#include <cstdint>

namespace alex {

struct vertex_t {
  float position[3];
  float normal[3];
  float color[3];
  float texcoord[2];
};

}
