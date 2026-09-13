#pragma once

#include "sim/PassengerNavGraph.hpp"

#include <glm/vec2.hpp>
#include <cstddef>
#include <vector>

namespace metro::sim {

class PassengerAgent {
public:
  enum class State { Walking, Arrived };

  PassengerAgent(std::vector<size_t> path = {}, float speed = 1.0f);

  void update(float dt, const PassengerNavGraph& graph);

  size_t node() const { return mNode; }
  const std::vector<size_t>& path() const { return mPath; }
  float progress() const { return mProgress; }
  float speed() const { return mSpeed; }
  State state() const { return mState; }
  glm::vec2 position(const PassengerNavGraph& graph) const;

private:
  std::vector<size_t> mPath;
  size_t mPathIndex = 0;
  size_t mNode = 0;
  float mProgress = 0.0f;
  float mSpeed = 1.0f;
  State mState = State::Arrived;
};

} // namespace metro::sim
