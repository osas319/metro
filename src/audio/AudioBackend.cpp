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


void AudioBackend::updateTrainSound(float speedMps, float accelerationMps2) {
  if (mStream == nullptr) return;

  speedMps = std::max(0.0f, speedMps);
  if (!std::isfinite(speedMps) || !std::isfinite(accelerationMps2))
    return;

  constexpr int sampleRate = 48000;
  constexpr int chunkSamples = sampleRate / 50; // 20 ms
  constexpr int targetQueuedBytes = sampleRate / 5 * static_cast<int>(sizeof(float));

  const int queued = SDL_GetAudioStreamQueued(mStream);
  if (queued < 0 || queued >= targetQueuedBytes) return;

  const float speedFraction = std::clamp(speedMps / 22.2f, 0.0f, 1.0f);
  const float traction = std::clamp(std::max(accelerationMps2, 0.0f) / 1.2f, 0.0f, 1.0f);
  const float braking = std::clamp(std::max(-accelerationMps2, 0.0f) / 2.4f, 0.0f, 1.0f);
  const float gain =
      0.006f + speedFraction * 0.030f + traction * 0.035f + braking * 0.010f;
  const float frequency =
      72.0f + speedFraction * 155.0f + traction * 95.0f;

  constexpr float twoPi = 6.28318530717958647692f;
  std::vector<float> samples(chunkSamples);
  for (int i = 0; i < chunkSamples; ++i) {
    const float sampleTime = static_cast<float>(i) / sampleRate;
    const float phase = mMotorPhase + sampleTime * frequency;
    const float fundamental = std::sin(twoPi * phase);
    const float second = std::sin(twoPi * phase * 2.01f) * 0.32f;
    const float tractionWhine =
        std::sin(twoPi * phase * 3.97f) * (0.08f + traction * 0.22f);
    const float brakeTone =
        std::sin(twoPi * phase * 0.53f) * braking * 0.12f;
    const float value =
        (fundamental + second + tractionWhine + brakeTone) * gain;
    const float fadeIn = std::min(1.0f, static_cast<float>(i) / 180.0f);
    samples[static_cast<size_t>(i)] = value * fadeIn;
  }

  mMotorPhase += static_cast<float>(chunkSamples) * frequency / sampleRate;
  mMotorPhase = std::fmod(mMotorPhase, 1.0f);
  SDL_PutAudioStreamData(
      mStream, samples.data(),
      static_cast<int>(samples.size() * sizeof(float)));
}

} // namespace metro::audio
