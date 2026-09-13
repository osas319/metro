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

} // namespace metro::sim
