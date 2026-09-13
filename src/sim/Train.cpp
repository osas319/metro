#include "sim/Train.hpp"

#include <algorithm>

namespace metro::sim {

void Train::update(float dt, bool throttle, bool brake, bool signalClear,
                   float routeLength) {
  constexpr float maxSpeed = 22.2f;
  constexpr float acceleration = 1.2f;
  constexpr float serviceBrake = 2.4f;
  constexpr float rollingResistance = 0.08f;

  if (mDoorsOpen || !signalClear) throttle = false;
  if (routeLength > 0.0f && mPosition >= routeLength) {
    throttle = false;
    brake = true;
  }
  float accelerationValue = throttle ? acceleration : -rollingResistance;
  if (brake || !signalClear) accelerationValue = -serviceBrake;
  mSpeed = std::clamp(mSpeed + accelerationValue * dt, 0.0f, maxSpeed);
  mPosition += mSpeed * dt;
  if (routeLength > 0.0f && mPosition >= routeLength) {
    mPosition = routeLength;
    mSpeed = 0.0f;
  }
}

} // namespace metro::sim
