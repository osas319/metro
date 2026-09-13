#include "sim/PassengerSystem.hpp"

#include <algorithm>

namespace metro::sim {

void PassengerSystem::update(float dt, bool doorsOpen, bool trainStopped,
                             bool allowBoarding) {
  if (!doorsOpen || !trainStopped || !allowBoarding) return;

  mTransferTimer += dt;
  constexpr float transferInterval = 0.5f;
  while (mTransferTimer >= transferInterval) {
    mTransferTimer -= transferInterval;
    if (mOnboard < mCapacity && mWaiting > 0) {
      ++mOnboard;
      --mWaiting;
      ++mBoardedTotal;
    }
  }
}

void PassengerSystem::unloadAtTerminal() {
  mAlightedTotal += mOnboard;
  mWaiting += mOnboard;
  mOnboard = 0;
  mTransferTimer = 0.0f;
}

void PassengerSystem::serviceStop(size_t stopIndex, size_t stopCount,
                                  bool terminal) {
  if (terminal) {
    unloadAtTerminal();
    return;
  }
  if (stopCount < 2 || stopIndex == 0 || stopIndex >= stopCount - 1) return;

  const size_t alighting = std::min(mOnboard, mOnboard / 8 + 1);
  mOnboard -= alighting;
  mWaiting += alighting;
  mAlightedTotal += alighting;
  mAlightedAtStops += alighting;
  mTransferTimer = 0.0f;
}

} // namespace metro::sim
