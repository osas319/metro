#pragma once
// Uygulama: SDL3 penceresi + ana döngü + olay işleme.
// Render detayları Renderer'a, Vulkan kurulumu VulkanContext'e bırakılır.
#include "rhi/Renderer.hpp"
#include "rhi/VulkanContext.hpp"

#include "app/Camera.hpp"
#include "sim/Train.hpp"
#include "sim/Signal.hpp"
#include "sim/PassengerSystem.hpp"
#include "sim/Route.hpp"
#include "sim/PhysicsWorld.hpp"
#include "sim/PassengerNavGraph.hpp"
#include "audio/AudioEventQueue.hpp"
#include <SDL3/SDL.h>

namespace metro::app {

class Application {
public:
  // Başarıda 0; hata kodları 1 (init), 2 (istisna) döner.
  int run();

private:
  bool init();
  void shutdown();
  void handleEvent(const SDL_Event& e);
  void update(float dt);

  SDL_Window* mWindow = nullptr;
  rhi::VulkanContext mContext;
  rhi::Renderer mRenderer;

  bool mRunning = true;
  bool mResized = false;
  bool mMinimized = false;
  
  // Kamera ve Girdi
  Camera mCamera;
  sim::Train mTrain;
  sim::PhysicsWorld mPhysics;
  sim::BlockSignal mSignal{8};
  float mRouteLength = 2000.0f;
  float mBlockLength = 250.0f;
  float mStopDwellSecondsLimit = 3.0f;
  sim::PassengerSystem mPassengers{320};
  sim::Route mRoute;
  sim::PassengerNavGraph mPassengerNav;
  audio::AudioEventQueue mAudioEvents;
  bool mTerminalServiced = false;
  size_t mLastServicedStop = static_cast<size_t>(-1);
  float mStopDwellSeconds = 0.0f;
  bool mMouseCaptured = false;
  const bool* mKeyboardState = nullptr;
};

} // namespace metro::app
