#include "sim/Train.hpp"

#include <algorithm>

namespace metro::sim {

void Train::update(float dt, bool throttle, bool brake) {
  constexpr float maxSpeed = 22.2f;
  constexpr float acceleration = 1.2f;
  constexpr float serviceBrake = 2.4f;
  constexpr float rollingResistance = 0.08f;

  if (mDoorsOpen) throttle = false;
  float accelerationValue = throttle ? acceleration : -rollingResistance;
  if (brake) accelerationValue = -serviceBrake;
  mSpeed = std::clamp(mSpeed + accelerationValue * dt, 0.0f, maxSpeed);
  mPosition += mSpeed * dt;
}

} // namespace metro::sim
