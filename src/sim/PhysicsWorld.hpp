#pragma once

#include <cstddef>

namespace metro::sim {

class Train;

class PhysicsWorld {
public:
  struct Settings {
    float fixedStep = 1.0f / 60.0f;
    size_t maxSubsteps = 4;
  };

  PhysicsWorld();
  explicit PhysicsWorld(Settings settings);

  void reset();
  void step(float frameDelta, Train& train, bool throttle, bool brake,
            bool signalClear, float routeLength);
  float interpolationAlpha() const;
  float interpolatedPosition(const Train& train) const;
  float fixedStep() const { return mSettings.fixedStep; }

private:
  Settings mSettings;
  float mAccumulator = 0.0f;
  float mPreviousTrainPosition = 0.0f;
};

} // namespace metro::sim
