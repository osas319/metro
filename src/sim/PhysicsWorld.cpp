#include "sim/PhysicsWorld.hpp"

#include <algorithm>
#include <cmath>

#include "sim/Train.hpp"

namespace metro::sim {

PhysicsWorld::PhysicsWorld() : PhysicsWorld(Settings{}) {}

PhysicsWorld::PhysicsWorld(Settings settings) : mSettings(settings) {
  if (!std::isfinite(mSettings.fixedStep) || mSettings.fixedStep <= 0.0f) {
    mSettings.fixedStep = 1.0f / 60.0f;
  }
  if (mSettings.maxSubsteps == 0) mSettings.maxSubsteps = 1;
}

void PhysicsWorld::reset() {
  mAccumulator = 0.0f;
}

void PhysicsWorld::step(float frameDelta, Train& train, bool throttle,
                        bool brake, bool signalClear, float routeLength) {
  if (!std::isfinite(frameDelta) || frameDelta <= 0.0f) return;
  mAccumulator = std::min(mAccumulator + frameDelta,
                          mSettings.fixedStep *
                              static_cast<float>(mSettings.maxSubsteps));
  size_t substeps = 0;
  while (mAccumulator >= mSettings.fixedStep &&
         substeps++ < mSettings.maxSubsteps) {
    train.update(mSettings.fixedStep, throttle, brake, signalClear,
                 routeLength);
    mAccumulator -= mSettings.fixedStep;
  }
}

float PhysicsWorld::interpolationAlpha() const {
  return mAccumulator / mSettings.fixedStep;
}

} // namespace metro::sim
