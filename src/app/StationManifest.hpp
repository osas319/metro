#pragma once

#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include "sim/Route.hpp"
#include "sim/Train.hpp"

namespace metro::app {

struct StationManifest {
  std::string name;
  std::string line;
  std::string meshDirectory;
  std::string materialDirectory;
  std::string textureDirectory;
  std::string lightmapDirectory;
  std::string modelPath;
  glm::vec3 spawnPosition{0.0f, 1.7f, 0.0f};
  float spawnYaw = -90.0f;
  float spawnPitch = 0.0f;
  size_t blockCount = 8;
  float routeLength = 2000.0f;
  float platformWidth = 4.0f;
  float trackGauge = 2.4f;
  float trainWidth = 2.8f;
  float trainHeight = 3.2f;
  glm::vec3 platformEdgePosition{0.0f};
  glm::vec3 stopPosition{0.0f};
  size_t passengerCapacity = 320;
  size_t initialWaitingPassengers = 24;
  float stopDwellSeconds = 3.0f;
  float physicsFixedStep = 1.0f / 60.0f;
  size_t physicsMaxSubsteps = 4;
  sim::Train::Parameters trainParameters;
  std::vector<sim::Stop> stops;

  static bool load(const std::string& path, StationManifest& out);
};

} // namespace metro::app
