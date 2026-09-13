#include "audio/AudioBackend.hpp"

#include <SDL3/SDL_audio.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace metro::audio {

bool AudioBackend::init() {
  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_F32;
  spec.channels = 1;
  spec.freq = 48000;
  mStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                      &spec, nullptr, nullptr);
  if (mStream == nullptr) return false;
  return SDL_ResumeAudioStreamDevice(mStream);
}

void AudioBackend::shutdown() {
  if (mStream == nullptr) return;
  SDL_DestroyAudioStream(mStream);
  mStream = nullptr;
}

void AudioBackend::play(EventType type) {
  if (mStream == nullptr) return;
  constexpr int sampleRate = 48000;
  constexpr int sampleCount = sampleRate / 20;
  constexpr float pi = 3.14159265358979323846f;
  const float base =
      type == EventType::SignalChanged ? 660.0f
                                       : type == EventType::DoorOpened ? 440.0f
                                                                       : 330.0f;
  std::vector<float> samples(sampleCount);
  for (int i = 0; i < sampleCount; ++i) {
    const float envelope =
        std::min(1.0f, static_cast<float>(i) / 240.0f) *
        std::min(1.0f, static_cast<float>(sampleCount - i) / 480.0f);
    samples[i] = 0.08f * envelope *
                 std::sin(2.0f * pi * base *
                          static_cast<float>(i) / sampleRate);
  }
  SDL_PutAudioStreamData(mStream, samples.data(),
                         static_cast<int>(samples.size() * sizeof(float)));
}

} // namespace metro::audio
