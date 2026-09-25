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
  if (!std::isfinite(parameters.maxJerk) || parameters.maxJerk <= 0.0f) {
    parameters.maxJerk = defaults.maxJerk;
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
  if (routeLength > mPosition) {
    const float distance = routeLength - mPosition;
    const float brakingDistance =
        (mSpeed * mSpeed) / (2.0f * mParameters.serviceBrake);
    if (distance <= brakingDistance + 0.05f) {
      throttle = false;
      brake = true;
    }
  }
  float targetAcceleration =
      throttle ? mParameters.acceleration : -mParameters.rollingResistance;
  if (brake || !signalClear)
    targetAcceleration = -mParameters.serviceBrake;

  const float maxAccelerationChange = mParameters.maxJerk * dt;
  mAcceleration += std::clamp(targetAcceleration - mAcceleration,
                              -maxAccelerationChange, maxAccelerationChange);
  mSpeed = std::clamp(mSpeed + mAcceleration * dt, 0.0f,
                      mParameters.maxSpeed);
  if (mSpeed <= 0.0f && mAcceleration < 0.0f)
    mAcceleration = 0.0f;
  const float targetDoorFraction = mDoorsOpen ? 1.0f : 0.0f;
  const float doorRate = 2.5f;
  if (mDoorOpenFraction < targetDoorFraction)
    mDoorOpenFraction = std::min(targetDoorFraction, mDoorOpenFraction + doorRate * dt);
  else if (mDoorOpenFraction > targetDoorFraction)
    mDoorOpenFraction = std::max(targetDoorFraction, mDoorOpenFraction - doorRate * dt);
  const float nextPosition = mPosition + mSpeed * dt;
  const bool settledAtTarget =
      routeLength > mPosition && routeLength - mPosition <= 0.05f &&
      mSpeed <= mParameters.serviceBrake * dt;
  if (routeLength > 0.0f &&
      (nextPosition >= routeLength || settledAtTarget)) {
    mPosition = routeLength;
    mSpeed = 0.0f;
    mAcceleration = 0.0f;
  } else {
    mPosition = nextPosition;
  }
}

} // namespace metro::sim
