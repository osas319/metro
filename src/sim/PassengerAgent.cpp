#include "sim/PassengerAgent.hpp"

#include <cmath>
#include <limits>
#include <utility>

namespace metro::sim {

PassengerAgent::PassengerAgent(std::vector<size_t> path, float speed)
    : mPath(std::move(path)),
      mSpeed(std::isfinite(speed) && speed > 0.0f ? speed : 0.0f) {
  if (!mPath.empty()) {
    mNode = mPath.front();
    mState = mPath.size() > 1 ? State::Walking : State::Arrived;
    if (mState == State::Arrived) mProgress = 1.0f;
  }
}

void PassengerAgent::update(float dt, const PassengerNavGraph& graph) {
  if (mState == State::Arrived || !std::isfinite(dt) || dt <= 0.0f ||
      mSpeed <= 0.0f)
    return;

  float distanceRemaining = mSpeed * dt;
  while (distanceRemaining > 0.0f && mState == State::Walking) {
    if (mPathIndex + 1 >= mPath.size()) {
      mNode = mPath.back();
      mProgress = 1.0f;
      mState = State::Arrived;
      break;
    }

    const auto* from = graph.node(mPath[mPathIndex]);
    const auto* to = graph.node(mPath[mPathIndex + 1]);
    if (from == nullptr || to == nullptr) {
      mState = State::Arrived;
      mProgress = 1.0f;
      break;
    }
    const float dx = to->x - from->x;
    const float dz = to->z - from->z;
    const float edgeLength = std::sqrt(dx * dx + dz * dz);
    if (!std::isfinite(edgeLength) ||
        edgeLength <= std::numeric_limits<float>::epsilon()) {
      ++mPathIndex;
      mNode = mPath[mPathIndex];
      mProgress = 0.0f;
      continue;
    }

    const float edgeRemaining = (1.0f - mProgress) * edgeLength;
    if (distanceRemaining < edgeRemaining) {
      mProgress += distanceRemaining / edgeLength;
      distanceRemaining = 0.0f;
    } else {
      distanceRemaining -= edgeRemaining;
      ++mPathIndex;
      mNode = mPath[mPathIndex];
      mProgress = 0.0f;
      if (mPathIndex + 1 >= mPath.size()) {
        mProgress = 1.0f;
        mState = State::Arrived;
      }
    }
  }
}

} // namespace metro::sim
