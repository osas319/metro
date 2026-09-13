#pragma once

#include <cstddef>
#include <deque>

namespace metro::audio {

enum class EventType {
  DoorOpened,
  DoorClosed,
  StopArrived,
  TerminalServiced,
  TrainDeparted,
  SignalChanged
};

const char* eventName(EventType type);

struct Event {
  EventType type;
  size_t stopIndex = 0;
};

class AudioEventQueue {
public:
  void push(Event event);
  bool tryPop(Event& event);
  size_t drain();
  size_t pending() const { return mEvents.size(); }

private:
  std::deque<Event> mEvents;
};

} // namespace metro::audio
