#include "audio/AudioBackend.hpp"

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <array>
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

  struct Tone {
    float frequency;
    float seconds;
    float gain;
  };

  std::array<Tone, 3> tones{};
  size_t toneCount = 0;
  switch (type) {
    case EventType::DoorOpened:
      tones = {{{440.0f, 0.10f, 0.07f},
               {660.0f, 0.14f, 0.07f},
               {880.0f, 0.06f, 0.04f}}};
      toneCount = 3;
      break;
    case EventType::DoorClosed:
      tones = {{{660.0f, 0.08f, 0.06f},
               {440.0f, 0.14f, 0.06f}}};
      toneCount = 2;
      break;
    case EventType::StopArrived:
      tones = {{{523.25f, 0.12f, 0.06f},
               {659.25f, 0.16f, 0.07f},
               {783.99f, 0.20f, 0.07f}}};
      toneCount = 3;
      break;
    case EventType::TerminalServiced:
      tones = {{{392.0f, 0.12f, 0.06f},
               {523.25f, 0.12f, 0.06f},
               {659.25f, 0.18f, 0.07f}}};
      toneCount = 3;
      break;
    case EventType::TrainDeparted:
      tones = {{{329.63f, 0.10f, 0.05f},
               {440.0f, 0.12f, 0.06f},
               {554.37f, 0.12f, 0.06f}}};
      toneCount = 3;
      break;
    case EventType::SignalChanged:
      tones = {{{740.0f, 0.09f, 0.07f},
               {988.0f, 0.09f, 0.06f}}};
      toneCount = 2;
      break;
  }

  constexpr int sampleRate = 48000;
  constexpr float twoPi = 6.28318530717958647692f;
  int totalSamples = 0;
  for (size_t i = 0; i < toneCount; ++i)
    totalSamples += static_cast<int>(tones[i].seconds * sampleRate);

  std::vector<float> samples(static_cast<size_t>(totalSamples), 0.0f);
  int cursor = 0;
  for (size_t t = 0; t < toneCount; ++t) {
    const int count = static_cast<int>(tones[t].seconds * sampleRate);
    for (int i = 0; i < count; ++i) {
      const float local = static_cast<float>(i) / sampleRate;
      const float attack =
          std::min(1.0f, static_cast<float>(i) / (0.012f * sampleRate));
      const float release = std::min(
          1.0f, static_cast<float>(count - i) / (0.035f * sampleRate));
      samples[static_cast<size_t>(cursor + i)] =
          tones[t].gain * attack * release *
          std::sin(twoPi * tones[t].frequency * local);
    }
    cursor += count;
  }

  SDL_PutAudioStreamData(
      mStream, samples.data(),
      static_cast<int>(samples.size() * sizeof(float)));
}

} // namespace metro::audio
