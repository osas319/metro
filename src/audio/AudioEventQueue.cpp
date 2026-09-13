#include "audio/AudioEventQueue.hpp"

namespace metro::audio {

void AudioEventQueue::push(Event event) {
  constexpr size_t maxPendingEvents = 256;
  if (mEvents.size() >= maxPendingEvents) mEvents.pop_front();
  mEvents.push_back(event);
}

bool AudioEventQueue::tryPop(Event& event) {
  if (mEvents.empty()) return false;
  event = mEvents.front();
  mEvents.pop_front();
  return true;
}

size_t AudioEventQueue::drain() {
  const size_t count = mEvents.size();
  mEvents.clear();
  return count;
}

} // namespace metro::audio
