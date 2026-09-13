#include "app/Application.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <entt/entt.hpp>

#include "core/Log.hpp"
#include "app/StationManifest.hpp"

namespace metro::app {

struct TransformComponent {
    float x, y, z;
};

bool Application::init() {
  const char* manifestPath = std::getenv("METRO_STATION_MANIFEST");
  StationManifest station;
  if (!StationManifest::load(
          manifestPath != nullptr ? manifestPath : "assets/stations/kadikoy/station.json",
          station)) {
    return false;
  }
  mCamera.position = station.spawnPosition;
  mCamera.yaw = station.spawnYaw;
  mCamera.pitch = station.spawnPitch;
  mSignal.resize(station.blockCount);
  mRouteLength = station.routeLength;
  mPassengers = sim::PassengerSystem(station.passengerCapacity);
  mRoute.buildBlockStops(station.blockCount, station.routeLength);
  mStopDwellSecondsLimit = station.stopDwellSeconds;
  mBlockLength = station.routeLength /
                 static_cast<float>(std::max<size_t>(station.blockCount, 1));

  entt::registry registry;
  auto entity = registry.create();
  registry.emplace<TransformComponent>(entity, 1.0f, 2.0f, 3.0f);
  METRO_INFO("EnTT test: Entity olusturuldu (ID: %d), X: %.1f", 
             static_cast<int>(entity), registry.get<TransformComponent>(entity).x);

  // SDL_VIDEODRIVER=wayland container.sh'te sabitleniyor; burada sadece
  // hangi driver'ın seçildiğini doğrulayıp log'luyoruz (XWayland'e düşmez).
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    METRO_ERROR("SDL_Init: %s", SDL_GetError());
    return false;
  }
  METRO_INFO("SDL video driver: %s", SDL_GetCurrentVideoDriver());

  mWindow = SDL_CreateWindow(
      "Metro — M4 Simülasyonu", 1280, 720,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (mWindow == nullptr) {
    METRO_ERROR("SDL_CreateWindow: %s", SDL_GetError());
    return false;
  }

  if (!mContext.init(mWindow)) return false;
  if (!mRenderer.init(mContext, mWindow)) return false;

  METRO_INFO("Pencere acildi; dongu basliyor (kapatmak icin pencereyi kapat)");
  METRO_INFO("Kontroller: Yukari=cekis Asagi=fren O/C=kapi Sol tik=fare ESC=serbest");
  return true;
}

void Application::handleEvent(const SDL_Event& e) {
  switch (e.type) {
    case SDL_EVENT_QUIT:
      mRunning = false;
      break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      mRunning = false;
      break;
    case SDL_EVENT_WINDOW_MINIMIZED:
      mMinimized = true;
      break;
    case SDL_EVENT_WINDOW_RESTORED:
      mMinimized = false;
      break;
    case SDL_EVENT_WINDOW_RESIZED:
      mResized = true;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (e.button.button == SDL_BUTTON_LEFT) {
        mMouseCaptured = !mMouseCaptured;
        SDL_SetWindowRelativeMouseMode(mWindow, mMouseCaptured);
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (e.key.key == SDLK_ESCAPE && mMouseCaptured) {
        mMouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(mWindow, false);
      }
      break;
    case SDL_EVENT_MOUSE_MOTION:
      if (mMouseCaptured) {
        mCamera.yaw += e.motion.xrel * mCamera.mouseSensitivity;
        mCamera.pitch -= e.motion.yrel * mCamera.mouseSensitivity;
        
        if (mCamera.pitch > 89.0f) mCamera.pitch = 89.0f;
        if (mCamera.pitch < -89.0f) mCamera.pitch = -89.0f;
      }
      break;
    default:
      break;
  }
}

void Application::update(float dt) {
  if (mKeyboardState == nullptr) {
      int count = 0;
      mKeyboardState = SDL_GetKeyboardState(&count);
  }

  const bool throttle = mKeyboardState[SDL_SCANCODE_UP];
  const bool brake = mKeyboardState[SDL_SCANCODE_DOWN];
  if (mKeyboardState[SDL_SCANCODE_O] && mTrain.speed() < 0.05f) {
    mTrain.setDoorsOpen(true);
  }
  if (mKeyboardState[SDL_SCANCODE_C]) {
    mTrain.setDoorsOpen(false);
  }
  const size_t currentBlock =
      std::min(static_cast<size_t>(mTrain.position() / mBlockLength),
               mSignal.blockCount() - 1);
  for (size_t block = 0; block < mSignal.blockCount(); ++block) {
    mSignal.setOccupied(block, block == currentBlock);
  }
  const size_t nextBlock = currentBlock + 1;
  const bool signalClear = nextBlock < mSignal.blockCount() &&
                           mSignal.canEnter(nextBlock);
  const sim::Stop& upcomingStop = mRoute.nextStop(mTrain.position(), mRouteLength);
  const float nextStop = upcomingStop.position;
  const bool atStop = mTrain.position() >= nextStop - 0.5f &&
                      mTrain.speed() < 0.05f;
  mTrain.update(dt, throttle, brake, signalClear, nextStop);
  const bool atTerminal = mTrain.position() >= mRouteLength &&
                          mTrain.speed() < 0.05f;
  if (atStop || atTerminal) {
    mTrain.setDoorsOpen(true);
    if (!atTerminal) {
      mStopDwellSeconds += dt;
      if (mStopDwellSeconds >= mStopDwellSecondsLimit) {
        mTrain.setDoorsOpen(false);
        mStopDwellSeconds = 0.0f;
      }
    }
  } else {
    mStopDwellSeconds = 0.0f;
  }
  if (atTerminal && !mTerminalServiced) {
    mPassengers.unloadAtTerminal();
    mTerminalServiced = true;
  }
  mPassengers.update(dt, mTrain.doorsOpen(), mTrain.speed() < 0.05f,
                    !atTerminal);

  if (!mMouseCaptured) return;

  float velocity = mCamera.moveSpeed * dt;
  if (mKeyboardState[SDL_SCANCODE_LSHIFT] ||
      mKeyboardState[SDL_SCANCODE_RSHIFT]) {
    velocity *= 3.0f;
  }
  if (mKeyboardState[SDL_SCANCODE_W]) mCamera.position += mCamera.getFront() * velocity;
  if (mKeyboardState[SDL_SCANCODE_S]) mCamera.position -= mCamera.getFront() * velocity;
  if (mKeyboardState[SDL_SCANCODE_A]) mCamera.position -= mCamera.getRight() * velocity;
  if (mKeyboardState[SDL_SCANCODE_D]) mCamera.position += mCamera.getRight() * velocity;
  if (mKeyboardState[SDL_SCANCODE_Q]) mCamera.position.y -= velocity;
  if (mKeyboardState[SDL_SCANCODE_E]) mCamera.position.y += velocity;
}

int Application::run() {
  if (!init()) {
    shutdown();
    return 1;
  }

  // Test/CI: METRO_AUTO_EXIT=<saniye> → süre sonunda kendini kapat.
  const char* autoExit = std::getenv("METRO_AUTO_EXIT");
  const double autoExitSec = autoExit != nullptr ? std::atof(autoExit) : 0.0;
  const Uint64 startNs = SDL_GetTicksNS();
  Uint64 lastStatsNs = startNs;
  Uint64 lastFrameNs = startNs;
  uint64_t frameCount = 0;

  while (mRunning) {
    const Uint64 nowNs = SDL_GetTicksNS();
    float dt = static_cast<float>(nowNs - lastFrameNs) / 1e9f;
    lastFrameNs = nowNs;

    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);

    if (mMinimized) {
      SDL_Delay(50); // CPU boş yere yakma
      continue;
    }

    if (mResized) {
      mRenderer.onResize();
      mResized = false;
    }
    
    update(dt);

    Camera renderCamera = mCamera;
    if (!mMouseCaptured) {
      renderCamera.position = glm::vec3(0.0f, 2.4f, 8.0f - mTrain.position());
      renderCamera.yaw = -90.0f;
      renderCamera.pitch = -4.0f;
    }
    mRenderer.drawFrame(renderCamera, mTrain.position());
    ++frameCount;

    if (nowNs - lastStatsNs >= Uint64(2e9)) {
      const double secs = double(nowNs - lastStatsNs) / 1e9;
      METRO_INFO("fps: %.1f", double(frameCount) / secs);
      METRO_INFO("tren: %.1f km/saat, konum %.1f m",
                 mTrain.speed() * 3.6f, mTrain.position());
      METRO_INFO("kapi: %s", mTrain.doorsOpen() ? "acik" : "kapali");
      METRO_INFO("yolcu: %zu trende, %zu bekliyor",
                 mPassengers.onboard(), mPassengers.waiting());
      const size_t statsBlock =
          std::min(static_cast<size_t>(mTrain.position() / mBlockLength),
                   mSignal.blockCount() - 1);
      const size_t statsNextBlock = statsBlock + 1;
      METRO_INFO("sinyal: blok %zu %s", statsNextBlock,
                 statsNextBlock < mSignal.blockCount() &&
                         mSignal.canEnter(statsNextBlock)
                     ? "yesil"
                     : "kirmizi");
      const sim::Stop& statsStop =
          mRoute.nextStop(mTrain.position(), mRouteLength);
      METRO_INFO("sonraki durak: %s (%.1f m)",
                 statsStop.name.c_str(), statsStop.position);
      frameCount = 0;
      lastStatsNs = nowNs;
    }
    if (autoExitSec > 0.0 && double(nowNs - startNs) / 1e9 >= autoExitSec) {
      METRO_INFO("METRO_AUTO_EXIT (%.1fs) — cikiliyor", autoExitSec);
      mRunning = false;
    }
  }

  // Swapchain'a bağlı bekleyen işler olmadan yıkım.
  vkDeviceWaitIdle(mContext.device());
  shutdown();
  return 0;
}

void Application::shutdown() {
  mRenderer.shutdown();
  mContext.shutdown();
  if (mWindow != nullptr) {
    SDL_DestroyWindow(mWindow);
    mWindow = nullptr;
  }
  SDL_Quit();
}

} // namespace metro::app
