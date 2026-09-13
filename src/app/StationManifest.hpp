#pragma once

#include <string>
#include <glm/vec3.hpp>
#include "sim/Train.hpp"

namespace metro::app {

struct StationManifest {
  std::string name;
  std::string line;
  std::string meshDirectory;
  std::string materialDirectory;
  std::string textureDirectory;
  std::string lightmapDirectory;
  glm::vec3 spawnPosition{0.0f, 1.7f, 0.0f};
  float spawnYaw = -90.0f;
  float spawnPitch = 0.0f;
  size_t blockCount = 8;
  float routeLength = 2000.0f;
  size_t passengerCapacity = 320;
  float stopDwellSeconds = 3.0f;
  sim::Train::Parameters trainParameters;

  static bool load(const std::string& path, StationManifest& out);
};

} // namespace metro::app
