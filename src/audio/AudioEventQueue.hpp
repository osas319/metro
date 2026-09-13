#pragma once

#include <cstddef>
#include <deque>

namespace metro::audio {

enum class EventType { DoorOpened, DoorClosed, StopArrived, TerminalServiced };

struct Event {
  EventType type;
  size_t stopIndex = 0;
};

class AudioEventQueue {
public:
  void push(Event event);
  bool tryPop(Event& event);
  size_t pending() const { return mEvents.size(); }

private:
  std::deque<Event> mEvents;
};

} // namespace metro::audio
