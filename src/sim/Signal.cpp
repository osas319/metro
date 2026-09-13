#include "sim/Signal.hpp"

namespace metro::sim {

BlockSignal::BlockSignal(size_t blockCount) : mOccupied(blockCount, false) {}

void BlockSignal::setOccupied(size_t block, bool occupied) {
  if (block < mOccupied.size()) mOccupied[block] = occupied;
}

SignalAspect BlockSignal::aspect(size_t block) const {
  return block < mOccupied.size() && mOccupied[block]
             ? SignalAspect::Stop
             : SignalAspect::Proceed;
}

bool BlockSignal::canEnter(size_t block) const {
  return aspect(block) == SignalAspect::Proceed;
}

} // namespace metro::sim
