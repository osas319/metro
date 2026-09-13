#include "sim/PhysicsWorld.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/RegisterTypes.h>

#include "sim/Train.hpp"

namespace metro::sim {

namespace {

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer object1,
                     JPH::ObjectLayer object2) const override {
    return object1 == 0 ? object2 == 1 : object1 == 1;
  }
};

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
  BroadPhaseLayerInterface() {
    mLayers[0] = JPH::BroadPhaseLayer(0);
    mLayers[1] = JPH::BroadPhaseLayer(1);
  }

  uint GetNumBroadPhaseLayers() const override { return 2; }
  JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
    return mLayers[layer < 2 ? layer : 0];
  }
  const char* GetBroadPhaseLayerName(
      JPH::BroadPhaseLayer layer) const override {
    return layer == JPH::BroadPhaseLayer(1) ? "MOVING" : "NON_MOVING";
  }

private:
  JPH::BroadPhaseLayer mLayers[2];
};

class ObjectVsBroadPhaseLayerFilter final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer objectLayer,
                     JPH::BroadPhaseLayer broadPhaseLayer) const override {
    return objectLayer == 0 ? broadPhaseLayer == JPH::BroadPhaseLayer(1)
                            : true;
  }
};

void EnsureJoltInitialized() {
  static const bool initialized = [] {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    return true;
  }();
  (void)initialized;
}

unsigned JoltWorkerCount() {
  const unsigned hardwareThreads = std::thread::hardware_concurrency();
  return hardwareThreads > 1 ? hardwareThreads - 1 : 1;
}

} // namespace

struct PhysicsWorld::JoltState {
  JoltState(float trainWidth, float trainHeight, float trackGauge,
            float routeLength, float platformWidth)
      : tempAllocator(4 * 1024 * 1024),
        jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                JoltWorkerCount()),
        physicsSystem() {
    broadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterface>();
    objectVsBroadPhaseLayerFilter =
        std::make_unique<ObjectVsBroadPhaseLayerFilter>();
    objectLayerPairFilter = std::make_unique<ObjectLayerPairFilter>();
    physicsSystem.Init(4096, 0, 4096, 4096, *broadPhaseLayerInterface,
                       *objectVsBroadPhaseLayerFilter, *objectLayerPairFilter);

    JPH::BodyInterface& bodies = physicsSystem.GetBodyInterface();
    const JPH::BoxShapeSettings groundShape(JPH::Vec3(500.0f, 0.5f, 500.0f));
    JPH::BodyCreationSettings groundSettings(
        groundShape.Create().Get(), JPH::RVec3(0.0, -1.5, 0.0),
        JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0);
    groundBody = bodies.CreateAndAddBody(groundSettings, JPH::EActivation::DontActivate);

    const JPH::BoxShapeSettings trainShape(
        JPH::Vec3(trainWidth * 0.5f, trainHeight * 0.5f, trackGauge * 0.8f));
    JPH::BodyCreationSettings trainSettings(
        trainShape.Create().Get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
        JPH::EMotionType::Kinematic, 1);
    trainBody = bodies.CreateAndAddBody(trainSettings, JPH::EActivation::Activate);

    const JPH::BoxShapeSettings platformShape(
        JPH::Vec3(0.4f, 0.15f, routeLength * 0.5f));
    for (float x : {-platformWidth - 0.4f, platformWidth + 0.4f}) {
      JPH::BodyCreationSettings platformSettings(
          platformShape.Create().Get(),
          JPH::RVec3(x, -0.2, -routeLength * 0.5),
          JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0);
      platformBodies.push_back(
          bodies.CreateAndAddBody(platformSettings,
                                  JPH::EActivation::DontActivate));
    }
  }

  void syncTrain(float position, float deltaTime) {
    physicsSystem.GetBodyInterface().MoveKinematic(
        trainBody, JPH::RVec3(0.0, 0.0, -position),
        JPH::Quat::sIdentity(), deltaTime);
  }

  JPH::TempAllocatorImpl tempAllocator;
  JPH::JobSystemThreadPool jobSystem;
  JPH::PhysicsSystem physicsSystem;
  std::unique_ptr<BroadPhaseLayerInterface> broadPhaseLayerInterface;
  std::unique_ptr<ObjectVsBroadPhaseLayerFilter>
      objectVsBroadPhaseLayerFilter;
  std::unique_ptr<ObjectLayerPairFilter> objectLayerPairFilter;
  JPH::BodyID groundBody;
  JPH::BodyID trainBody;
  std::vector<JPH::BodyID> platformBodies;
};

PhysicsWorld::PhysicsWorld() : PhysicsWorld(Settings{}) {}

PhysicsWorld::PhysicsWorld(Settings settings) : mSettings(settings) {
  if (!std::isfinite(mSettings.fixedStep) || mSettings.fixedStep <= 0.0f) {
    mSettings.fixedStep = 1.0f / 60.0f;
  }
  if (mSettings.maxSubsteps == 0) mSettings.maxSubsteps = 1;
  EnsureJoltInitialized();
  if (!std::isfinite(mSettings.trainWidth) || mSettings.trainWidth <= 0.0f)
    mSettings.trainWidth = 2.8f;
  if (!std::isfinite(mSettings.trainHeight) || mSettings.trainHeight <= 0.0f)
    mSettings.trainHeight = 3.2f;
  if (!std::isfinite(mSettings.trackGauge) || mSettings.trackGauge <= 0.0f)
    mSettings.trackGauge = 2.4f;
  if (!std::isfinite(mSettings.routeLength) || mSettings.routeLength <= 0.0f)
    mSettings.routeLength = 2000.0f;
  if (!std::isfinite(mSettings.platformWidth) ||
      mSettings.platformWidth <= 0.0f)
    mSettings.platformWidth = 4.0f;
  mJolt = std::make_unique<JoltState>(mSettings.trainWidth,
                                      mSettings.trainHeight,
                                      mSettings.trackGauge, mSettings.routeLength,
                                      mSettings.platformWidth);
}

PhysicsWorld::~PhysicsWorld() = default;
PhysicsWorld::PhysicsWorld(PhysicsWorld&&) noexcept = default;
PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&&) noexcept = default;

void PhysicsWorld::reset(float trainPosition) {
  mAccumulator = 0.0f;
  mPreviousTrainPosition =
      std::isfinite(trainPosition) ? trainPosition : 0.0f;
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
    mPreviousTrainPosition = train.position();
    train.update(mSettings.fixedStep, throttle, brake, signalClear,
                 routeLength);
    mJolt->syncTrain(train.position(), mSettings.fixedStep);
    mJolt->physicsSystem.Update(mSettings.fixedStep, 1, &mJolt->tempAllocator,
                                &mJolt->jobSystem);
    mAccumulator -= mSettings.fixedStep;
  }
}

float PhysicsWorld::interpolationAlpha() const {
  return mAccumulator / mSettings.fixedStep;
}

float PhysicsWorld::interpolatedPosition(const Train& train) const {
  const float alpha = std::clamp(interpolationAlpha(), 0.0f, 1.0f);
  return mPreviousTrainPosition +
         (train.position() - mPreviousTrainPosition) * alpha;
}

} // namespace metro::sim
