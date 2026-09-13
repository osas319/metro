#pragma once

#include <string>

namespace metro::app {

struct StationManifest {
  std::string name;
  std::string line;
  std::string meshDirectory;
  std::string materialDirectory;
  std::string textureDirectory;
  std::string lightmapDirectory;

  static bool load(const std::string& path, StationManifest& out);
};

} // namespace metro::app
