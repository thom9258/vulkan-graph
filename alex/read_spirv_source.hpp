#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

#include "ensure.hpp"

namespace alex {

[[nodiscard]] constexpr 
std::vector<std::uint32_t> read_spirv_source(std::filesystem::path path) {

  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return {};
  }

  size_t const bytecount = static_cast<size_t>(file.tellg());
  constexpr size_t scaling_factor = sizeof(uint32_t) / sizeof(char);
  size_t const read_times = bytecount / scaling_factor;
  std::vector<std::uint32_t> buffer;
  buffer.resize(read_times);
  ALEX_ERROR_IF(buffer.empty(), "allocator full");
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()),
            sizeof(buffer[0]) * buffer.size());
  file.close();
  return buffer;
}

}
