#include "sim/PassengerNavGraph.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace metro::sim {

bool PassengerNavGraph::buildLinear(size_t stopCount, float routeLength) {
  mNodes.clear();
  if (stopCount < 2 || !std::isfinite(routeLength) || routeLength <= 0.0f)
    return false;
  mNodes.resize(stopCount);
  for (size_t i = 0; i < stopCount; ++i) {
    mNodes[i].z = -routeLength * static_cast<float>(i) /
                  static_cast<float>(stopCount - 1);
    if (i > 0) mNodes[i].neighbors.push_back(i - 1);
    if (i + 1 < stopCount) mNodes[i].neighbors.push_back(i + 1);
  }
  return true;
}

bool PassengerNavGraph::buildFromStops(const std::vector<Stop>& stops) {
  mNodes.clear();
  if (stops.size() < 2) return false;
  mNodes.resize(stops.size());
  for (size_t i = 0; i < stops.size(); ++i) {
    if (!std::isfinite(stops[i].position) ||
        (i > 0 && stops[i].position <= stops[i - 1].position)) {
      mNodes.clear();
      return false;
    }
    mNodes[i].z = -stops[i].position;
    if (i > 0) mNodes[i].neighbors.push_back(i - 1);
    if (i + 1 < stops.size()) mNodes[i].neighbors.push_back(i + 1);
  }
  return true;
}

const PassengerNavGraph::Node* PassengerNavGraph::node(size_t index) const {
  return index < mNodes.size() ? &mNodes[index] : nullptr;
}

std::vector<size_t> PassengerNavGraph::shortestPath(size_t start,
                                                    size_t goal) const {
  if (start >= mNodes.size() || goal >= mNodes.size()) return {};
  std::vector<size_t> previous(mNodes.size(), mNodes.size());
  std::queue<size_t> pending;
  pending.push(start);
  previous[start] = start;
  while (!pending.empty()) {
    const size_t current = pending.front();
    pending.pop();
    if (current == goal) break;
    for (const size_t neighbor : mNodes[current].neighbors) {
      if (neighbor < mNodes.size() && previous[neighbor] == mNodes.size()) {
        previous[neighbor] = current;
        pending.push(neighbor);
      }
    }
  }
  if (previous[goal] == mNodes.size()) return {};
  std::vector<size_t> path;
  for (size_t current = goal;; current = previous[current]) {
    path.push_back(current);
    if (current == start) break;
  }
  std::reverse(path.begin(), path.end());
  return path;
}

} // namespace metro::sim
