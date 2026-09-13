#include "sim/Train.hpp"

#include <algorithm>
#include <cmath>

namespace metro::sim {

void Train::setParameters(Parameters parameters) {
  const Parameters defaults{};
  if (!std::isfinite(parameters.maxSpeed) || parameters.maxSpeed <= 0.0f) {
    parameters.maxSpeed = defaults.maxSpeed;
  }
  if (!std::isfinite(parameters.acceleration) ||
      parameters.acceleration <= 0.0f) {
    parameters.acceleration = defaults.acceleration;
  }
  if (!std::isfinite(parameters.serviceBrake) ||
      parameters.serviceBrake < parameters.acceleration) {
    parameters.serviceBrake =
        std::max(defaults.serviceBrake, parameters.acceleration);
  }
  if (!std::isfinite(parameters.rollingResistance) ||
      parameters.rollingResistance < 0.0f) {
    parameters.rollingResistance = defaults.rollingResistance;
  }
  mParameters = parameters;
}

bool Train::requestDoorsOpen(bool open, bool platformAligned) {
  if (!open) {
    mDoorsOpen = false;
    return true;
  }
  if (mSpeed > 0.05f || !platformAligned) return false;
  mDoorsOpen = true;
  return true;
}

void Train::update(float dt, bool throttle, bool brake, bool signalClear,
                   float routeLength) {
  if (!std::isfinite(dt) || dt <= 0.0f) return;
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
