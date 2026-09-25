#pragma once

#include "audio/AudioEventQueue.hpp"

struct SDL_AudioStream;

namespace metro::audio {

class AudioBackend {
public:
  bool init();
  void shutdown();
  void play(EventType type);
  void updateTrainSound(float speedMps, float accelerationMps2);

private:
  SDL_AudioStream* mStream = nullptr;
  float mMotorPhase = 0.0f;
};

} // namespace metro::audio
