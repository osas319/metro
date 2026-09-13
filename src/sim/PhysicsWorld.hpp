#pragma once

#include <cstddef>
#include <memory>

namespace metro::sim {

class Train;

class PhysicsWorld {
public:
  struct Settings {
    float fixedStep = 1.0f / 60.0f;
    size_t maxSubsteps = 4;
    float trainWidth = 2.8f;
    float trainHeight = 3.2f;
    float trackGauge = 2.4f;
    float routeLength = 2000.0f;
    float platformWidth = 4.0f;
    float columnSpacing = 24.0f;
  };

  PhysicsWorld();
  explicit PhysicsWorld(Settings settings);
  ~PhysicsWorld();
  PhysicsWorld(PhysicsWorld&&) noexcept;
  PhysicsWorld& operator=(PhysicsWorld&&) noexcept;
  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;

  void reset(float trainPosition = 0.0f);
  void step(float frameDelta, Train& train, bool throttle, bool brake,
            bool signalClear, float routeLength);
  float interpolationAlpha() const;
  float interpolatedPosition(const Train& train) const;
  float fixedStep() const { return mSettings.fixedStep; }

private:
  Settings mSettings;
  float mAccumulator = 0.0f;
  float mPreviousTrainPosition = 0.0f;
  struct JoltState;
  std::unique_ptr<JoltState> mJolt;
};

} // namespace metro::sim
