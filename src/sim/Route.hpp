#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace metro::sim {

struct Stop {
  std::string name;
  float position = 0.0f;
};

class Route {
public:
  void buildBlockStops(size_t blockCount, float routeLength);
  const std::vector<Stop>& stops() const { return mStops; }
  size_t stopCount() const { return mStops.size(); }

private:
  std::vector<Stop> mStops;
};

} // namespace metro::sim
