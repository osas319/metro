#pragma once

#include "audio/AudioEventQueue.hpp"

struct SDL_AudioStream;

namespace metro::audio {

class AudioBackend {
public:
  bool init();
  void shutdown();
  void play(EventType type);

private:
  SDL_AudioStream* mStream = nullptr;
};

} // namespace metro::audio
