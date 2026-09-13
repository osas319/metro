#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace metro::sim {

struct Stop {
  std::string name;
  float position = 0.0f;
  float dwellSeconds = -1.0f;
};

class Route {
public:
  void buildBlockStops(size_t blockCount, float routeLength);
  void setStops(std::vector<Stop> stops);
  float nextStopPosition(float currentPosition, float routeLength) const;
  const Stop& nextStop(float currentPosition, float routeLength) const;
  const std::vector<Stop>& stops() const { return mStops; }
  size_t stopCount() const { return mStops.size(); }

private:
  std::vector<Stop> mStops;
};

} // namespace metro::sim
