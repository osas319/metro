#include "sim/PassengerSystem.hpp"

#include <algorithm>

namespace metro::sim {

void PassengerSystem::update(float dt, bool doorsOpen, bool trainStopped) {
  if (!doorsOpen || !trainStopped) return;

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

} // namespace metro::sim
