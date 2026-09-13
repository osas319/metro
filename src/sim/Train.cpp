#include "sim/Train.hpp"

#include <algorithm>

namespace metro::sim {

void Train::update(float dt, bool throttle, bool brake, bool signalClear,
                   float routeLength) {
  if (mDoorsOpen || !signalClear) throttle = false;
  if (routeLength > 0.0f && mPosition >= routeLength) {
    throttle = false;
    brake = true;
  }
  float accelerationValue =
      throttle ? mParameters.acceleration : -mParameters.rollingResistance;
  if (brake || !signalClear) accelerationValue = -mParameters.serviceBrake;
  mSpeed = std::clamp(mSpeed + accelerationValue * dt, 0.0f,
                      mParameters.maxSpeed);
  const float nextPosition = mPosition + mSpeed * dt;
  if (routeLength > 0.0f && nextPosition >= routeLength) {
    mPosition = routeLength;
    mSpeed = 0.0f;
  } else {
    mPosition = nextPosition;
  }
}

} // namespace metro::sim
