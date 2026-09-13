#include "sim/Route.hpp"

#include <algorithm>
#include <utility>

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

void Route::setStops(std::vector<Stop> stops) {
  if (stops.size() < 2) return;
  for (size_t i = 1; i < stops.size(); ++i) {
    if (stops[i].name.empty() || stops[i].position <= stops[i - 1].position) {
      return;
    }
  }
  mStops = std::move(stops);
}

float Route::nextStopPosition(float currentPosition, float routeLength) const {
  return nextStop(currentPosition, routeLength).position;
}

const Stop& Route::nextStop(float currentPosition, float routeLength) const {
  for (const Stop& stop : mStops) {
    if (stop.position > currentPosition + 0.5f) {
      return stop;
    }
  }
  static const Stop terminal{"Terminal", 0.0f};
  if (!mStops.empty() && routeLength >= mStops.back().position) {
    return mStops.back();
  }
  return terminal;
}

} // namespace metro::sim
