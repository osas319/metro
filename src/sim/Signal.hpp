#pragma once

#include <cstddef>
#include <vector>

namespace metro::sim {

enum class SignalAspect { Stop, Proceed };

class BlockSignal {
public:
  explicit BlockSignal(size_t blockCount);

  void setOccupied(size_t block, bool occupied);
  SignalAspect aspect(size_t block) const;
  bool canEnter(size_t block) const;

private:
  std::vector<bool> mOccupied;
};

} // namespace metro::sim
