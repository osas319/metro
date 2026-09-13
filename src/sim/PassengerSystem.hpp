#pragma once

#include <cstddef>

namespace metro::sim {

class PassengerSystem {
public:
  explicit PassengerSystem(size_t capacity = 320) : mCapacity(capacity) {}

  void update(float dt, bool doorsOpen, bool trainStopped);
  void unloadAtTerminal();
  size_t onboard() const { return mOnboard; }
  size_t waiting() const { return mWaiting; }
  size_t alightedTotal() const { return mAlightedTotal; }
  size_t boardedTotal() const { return mBoardedTotal; }

private:
  size_t mCapacity;
  size_t mOnboard = 0;
  size_t mWaiting = 24;
  size_t mBoardedTotal = 0;
  size_t mAlightedTotal = 0;
  float mTransferTimer = 0.0f;
};

} // namespace metro::sim
