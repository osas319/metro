#include "sim/Route.hpp"

#include <algorithm>

namespace metro::sim {

void Route::buildBlockStops(size_t blockCount, float routeLength) {
  mStops.clear();
  if (blockCount == 0 || routeLength <= 0.0f) return;

  mStops.reserve(blockCount + 1);
  mStops.push_back({"Kadikoy", 0.0f});
  const float spacing = routeLength / static_cast<float>(blockCount);
  for (size_t i = 1; i < blockCount; ++i) {
    mStops.push_back({"M4 duragi " + std::to_string(i), spacing * i});
  }
  mStops.push_back({"Sabiha Gokcen", routeLength});
}

float Route::nextStopPosition(float currentPosition, float routeLength) const {
  for (const Stop& stop : mStops) {
    if (stop.position > currentPosition + 0.5f) {
      return stop.position;
    }
  }
  return routeLength;
}

} // namespace metro::sim
