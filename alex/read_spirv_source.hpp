#pragma once

#include <filesystem>
#include <fstream>

#include "ensure.hpp"
#include "arena.hpp"

namespace alex {

[[nodiscard]] constexpr 
std::span<uint32_t> read_spirv_source(std::filesystem::path path,
									  memory::arena &allocator) {

  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return {};
  }

  size_t const bytecount = static_cast<size_t>(file.tellg());
  constexpr size_t scaling_factor = sizeof(uint32_t) / sizeof(char);
  size_t const read_times = bytecount / scaling_factor;
  auto buffer = allocator.allocate<std::uint32_t>(read_times);
  ALEX_ERROR_IF(buffer.empty(), "allocator full");
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()),
            sizeof(buffer[0]) * buffer.size());
  file.close();
  return buffer;
}

}
