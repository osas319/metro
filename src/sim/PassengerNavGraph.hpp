#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include "sim/Route.hpp"

namespace metro::sim {

class PassengerNavGraph {
public:
  struct Node {
    float x = 0.0f;
    float z = 0.0f;
    std::vector<size_t> neighbors;
  };

  bool buildLinear(size_t stopCount, float routeLength);
  bool buildFromStops(const std::vector<Stop>& stops);
  bool loadFromFile(const std::string& path);
  bool valid() const { return !mNodes.empty(); }
  size_t nodeCount() const { return mNodes.size(); }
  const Node* node(size_t index) const;
  std::vector<size_t> shortestPath(size_t start, size_t goal) const;

private:
  std::vector<Node> mNodes;
};

} // namespace metro::sim
