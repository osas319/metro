#include "audio/AudioEventQueue.hpp"

namespace metro::audio {

const char* eventName(EventType type) {
  switch (type) {
  case EventType::DoorOpened: return "door_opened";
  case EventType::DoorClosed: return "door_closed";
  case EventType::StopArrived: return "stop_arrived";
  case EventType::TerminalServiced: return "terminal_serviced";
  case EventType::TrainDeparted: return "train_departed";
  case EventType::SignalChanged: return "signal_changed";
  case EventType::Horn: return "horn";
  case EventType::EmergencyBrake: return "emergency_brake";
  }
  return "unknown";
}

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
