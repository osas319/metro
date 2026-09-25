#include "sim/Signal.hpp"

namespace metro::sim {

BlockSignal::BlockSignal(size_t blockCount) : mOccupied(blockCount, false) {}

void BlockSignal::resize(size_t blockCount) {
  mOccupied.assign(blockCount, false);
}

void BlockSignal::setOccupied(size_t block, bool occupied) {
  if (block < mOccupied.size()) mOccupied[block] = occupied;
}

SignalAspect BlockSignal::aspect(size_t block) const {
  if (block >= mOccupied.size()) return SignalAspect::Proceed;
  if (mOccupied[block]) return SignalAspect::Stop;
  if (block + 1 < mOccupied.size() && mOccupied[block + 1])
    return SignalAspect::Caution;
  return SignalAspect::Proceed;
}

bool BlockSignal::canEnter(size_t block) const {
  return aspect(block) != SignalAspect::Stop;
}

const std::vector<bool>& BlockSignal::occupiedBlocks() const {
  return mOccupied;
}

} // namespace metro::sim
