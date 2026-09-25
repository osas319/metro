#pragma once

#include <cstddef>
#include <vector>

namespace metro::sim {

enum class SignalAspect { Stop, Caution, Proceed };

class BlockSignal {
public:
  explicit BlockSignal(size_t blockCount);

  void resize(size_t blockCount);
  void setOccupied(size_t block, bool occupied);
  SignalAspect aspect(size_t block) const;
  bool canEnter(size_t block) const;
  const std::vector<bool>& occupiedBlocks() const;
  size_t blockCount() const { return mOccupied.size(); }

private:
  std::vector<bool> mOccupied;
};

} // namespace metro::sim
