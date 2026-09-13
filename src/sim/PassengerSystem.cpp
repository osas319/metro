#include "sim/PassengerSystem.hpp"

#include <algorithm>
#include <limits>

namespace metro::sim {

void PassengerSystem::update(float dt, bool doorsOpen, bool trainStopped,
                             bool allowBoarding) {
  updateWalkingAgents(dt);
  if (!doorsOpen || !trainStopped || !allowBoarding) return;

  mTransferTimer += dt;
  constexpr float transferInterval = 0.5f;
  while (mTransferTimer >= transferInterval) {
    mTransferTimer -= transferInterval;
    if (mOnboard < mCapacity && mWaiting > 0) {
      ++mOnboard;
      --mWaiting;
      ++mBoardedTotal;
      if (mDestinationCounts.size() <= mBoardingDestination)
        mDestinationCounts.resize(mBoardingDestination + 1);
      ++mDestinationCounts[mBoardingDestination];
    }
  }
}

size_t PassengerSystem::addWalkingAgent(size_t start, size_t goal,
                                        float speed) {
  const auto path = mNavGraph.shortestPath(start, goal);
  if (path.empty()) return std::numeric_limits<size_t>::max();
  mAgents.emplace_back(path, speed);
  return mAgents.size() - 1;
}

void PassengerSystem::updateWalkingAgents(float dt) {
  for (auto& agent : mAgents) agent.update(dt, mNavGraph);
}

void PassengerSystem::unloadAtTerminal() {
  mAlightedTotal += mOnboard;
  mWaiting += mOnboard;
  mOnboard = 0;
  mTransferTimer = 0.0f;
  mDestinationCounts.clear();
}

void PassengerSystem::serviceStop(size_t stopIndex, size_t stopCount,
                                  bool terminal) {
  if (terminal) {
    unloadAtTerminal();
    return;
  }
  if (stopCount < 2 || stopIndex == 0 || stopIndex >= stopCount - 1) return;

  const size_t alighting =
      stopIndex < mDestinationCounts.size() ? mDestinationCounts[stopIndex] : 0;
  if (alighting == 0) return;
  mDestinationCounts[stopIndex] = 0;
  mOnboard -= std::min(mOnboard, alighting);
  mWaiting += alighting;
  mAlightedTotal += alighting;
  mAlightedAtStops += alighting;
  mTransferTimer = 0.0f;
}

void PassengerSystem::setBoardingDestination(size_t destinationStop) {
  mBoardingDestination = destinationStop;
}

} // namespace metro::sim
