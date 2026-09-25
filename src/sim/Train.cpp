#include "sim/Train.hpp"

#include <algorithm>
#include <cmath>

namespace metro::sim {

void Train::reset(float position) {
  mPosition = std::isfinite(position) ? std::max(0.0f, position) : 0.0f;
  mSpeed = 0.0f;
  mAcceleration = 0.0f;
  mDoorsOpen = false;
  mDoorOpenFraction = 0.0f;
  mTractionCommand = 0.0f;
  mBrakeCommand = 0.0f;
}

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
      parameters.serviceBrake <= 0.0f) {
    parameters.serviceBrake = defaults.serviceBrake;
  }
  if (!std::isfinite(parameters.emergencyBrake) ||
      parameters.emergencyBrake < parameters.serviceBrake) {
    parameters.emergencyBrake =
        std::max(defaults.emergencyBrake, parameters.serviceBrake);
  }
  if (!std::isfinite(parameters.rollingResistance) ||
      parameters.rollingResistance < 0.0f) {
    parameters.rollingResistance = defaults.rollingResistance;
  }
  if (!std::isfinite(parameters.maxJerk) || parameters.maxJerk <= 0.0f) {
    parameters.maxJerk = defaults.maxJerk;
  }
  if (!std::isfinite(parameters.tractionResponse) ||
      parameters.tractionResponse <= 0.0f) {
    parameters.tractionResponse = defaults.tractionResponse;
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

float Train::brakingDistance() const {
  if (!std::isfinite(mSpeed) || mSpeed <= 0.0f ||
      !std::isfinite(mParameters.serviceBrake) ||
      mParameters.serviceBrake <= 0.0f) {
    return 0.0f;
  }
  return (mSpeed * mSpeed) / (2.0f * mParameters.serviceBrake);
}

float Train::recommendedSpeed(float distanceMeters) const {
  if (!std::isfinite(distanceMeters) || distanceMeters <= 0.0f ||
      !std::isfinite(mParameters.serviceBrake) ||
      mParameters.serviceBrake <= 0.0f) {
    return 0.0f;
  }
  return std::min(mParameters.maxSpeed,
                  std::sqrt(2.0f * mParameters.serviceBrake * distanceMeters));
}

void Train::update(float dt, bool throttle, bool brake, bool signalClear,
                   float routeLength, bool emergencyBrake) {
  if (!std::isfinite(dt) || dt <= 0.0f) return;
  if (mDoorsOpen || mDoorOpenFraction > 0.01f || !signalClear)
    throttle = false;
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
  const float speedRatio =
      mParameters.maxSpeed > 0.0f
          ? std::clamp(mSpeed / mParameters.maxSpeed, 0.0f, 1.0f)
          : 0.0f;

  // Kumanda kolu ani sekilde degismez: motor ve fren komutlari yumusak
  // bicimde baslar/kesilir. Jerk limiti bunun uzerine ikinci bir konfor katmani ekler.
  const float commandAlpha = std::clamp(mParameters.tractionResponse * dt, 0.0f, 1.0f);
  const float targetTraction = throttle ? 1.0f : 0.0f;
  const float targetBrake = (brake || !signalClear || emergencyBrake) ? 1.0f : 0.0f;
  mTractionCommand += (targetTraction - mTractionCommand) * commandAlpha;
  mBrakeCommand += (targetBrake - mBrakeCommand) * commandAlpha;

  // Hafif aerodinamik direnc: dusuk hizda yuvarlanma, yuksek hizda hava direnci.
  const float resistance =
      mParameters.rollingResistance + 0.00035f * mSpeed * mSpeed;

  // CERCEVE traction curve: dusuk/orta hizda yuksek cekis, Vmax'a yaklasirken
  // daha yumusak guc azalmasi. Maksimum ivme korunurken fizik daha doğal hissettirir.
  const float tractionFactor =
      std::clamp(1.0f - 0.30f * std::pow(speedRatio, 1.7f), 0.52f, 1.0f);
  const float tractionAcceleration =
      mParameters.acceleration * tractionFactor * mTractionCommand - resistance;

  float targetAcceleration = tractionAcceleration;
  if (mBrakeCommand > 0.001f) {
    const float serviceDecel =
        mParameters.serviceBrake * (0.82f + 0.18f * speedRatio);
    const float brakeDecel = emergencyBrake
                                 ? mParameters.emergencyBrake * (0.96f + 0.04f * speedRatio)
                                 : serviceDecel;
    targetAcceleration = -brakeDecel * mBrakeCommand - resistance * 0.20f;
  } else if (mTractionCommand <= 0.001f) {
    targetAcceleration = -resistance;
  }

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
