#pragma once

#include "sim/PassengerAgent.hpp"

#include <cstddef>
#include <vector>

namespace metro::sim {

class PassengerSystem {
public:
  explicit PassengerSystem(size_t capacity = 320, size_t waiting = 24)
      : mCapacity(capacity), mWaiting(waiting) {}

  void update(float dt, bool doorsOpen, bool trainStopped,
              bool allowBoarding = true);
  void setNavGraph(const PassengerNavGraph& graph) { mNavGraph = graph; }
  size_t addWalkingAgent(size_t start, size_t goal, float speed = 1.0f);
  void updateWalkingAgents(float dt);
  const std::vector<PassengerAgent>& walkingAgents() const { return mAgents; }
  void clearWalkingAgents() { mAgents.clear(); }
  void serviceStop(size_t stopIndex, size_t stopCount, bool terminal);
  void setBoardingDestination(size_t destinationStop);
  void unloadAtTerminal();
  size_t onboard() const { return mOnboard; }
  size_t waiting() const { return mWaiting; }
  size_t alightedTotal() const { return mAlightedTotal; }
  size_t boardedTotal() const { return mBoardedTotal; }
  size_t alightedAtStops() const { return mAlightedAtStops; }

private:
  size_t mCapacity;
  size_t mOnboard = 0;
  size_t mWaiting = 24;
  size_t mBoardedTotal = 0;
  size_t mAlightedTotal = 0;
  size_t mAlightedAtStops = 0;
  float mTransferTimer = 0.0f;
  size_t mBoardingDestination = 1;
  std::vector<size_t> mDestinationCounts;
  PassengerNavGraph mNavGraph;
  std::vector<PassengerAgent> mAgents;
};

} // namespace metro::sim
