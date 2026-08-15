#pragma once

#include <fstream>
#include <streambuf>
#include <optional>
#include <filesystem>

namespace game {

constexpr auto slurp_file(std::filesystem::path path)
    -> std::optional<std::string> {

  std::ifstream t(path);
  if (!t.is_open()) {
    return std::nullopt;
  }
  std::string str;

  t.seekg(0, std::ios::end);
  str.reserve(t.tellg());
  t.seekg(0, std::ios::beg);

  str.assign((std::istreambuf_iterator<char>(t)),
             std::istreambuf_iterator<char>());
  return str;
}

}    
