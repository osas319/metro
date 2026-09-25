#include "app/Application.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <string_view>

#include "core/Log.hpp"
#include "app/StationManifest.hpp"

namespace metro::app {

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
  mPassengers = sim::PassengerSystem(station.passengerCapacity,
                                     station.initialWaitingPassengers);
  mInitialWaitingPassengers = station.initialWaitingPassengers;
  mPhysics = sim::PhysicsWorld(
      {.fixedStep = station.physicsFixedStep,
       .maxSubsteps = station.physicsMaxSubsteps,
       .trainWidth = station.trainWidth,
       .trainHeight = station.trainHeight,
       .trackGauge = station.trackGauge,
       .routeLength = station.routeLength,
       .platformWidth = station.platformWidth,
       .columnSpacing = station.columnSpacing});
  mPhysics.reset(mTrain.position());
  if (station.stops.empty()) {
    mRoute.buildBlockStops(station.blockCount, station.routeLength);
  } else {
    mRoute.setStops(station.stops);
  }
  const bool navGraphReady =
      station.navmeshPath.empty()
          ? mPassengerNav.buildFromStops(mRoute.stops())
          : mPassengerNav.loadFromFile(station.navmeshPath);
  if (!navGraphReady) {
    METRO_ERROR("Yolcu navgraph olusturulamadi");
    return false;
  }
  if (station.navmeshPath.empty()) {
    METRO_INFO("Navmesh asseti yok; durak konumlarindan dogrusal fallback kullaniliyor");
  } else {
    METRO_INFO("Navmesh asseti: %s", station.navmeshPath.c_str());
  }
  if (!station.audioDirectory.empty())
    METRO_INFO("Ses asset dizini: %s", station.audioDirectory.c_str());
  mPassengers.setNavGraph(mPassengerNav);
  mStopDwellSecondsLimit = station.stopDwellSeconds;
  mTrain.setParameters(station.trainParameters);
  mBlockLength = station.routeLength /
                 static_cast<float>(std::max<size_t>(station.blockCount, 1));

  // SDL_VIDEODRIVER=wayland container.sh'te sabitleniyor; burada sadece
  // hangi driver'ın seçildiğini doğrulayıp log'larız (XWayland'e düşmez).
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    METRO_ERROR("SDL_Init: %s", SDL_GetError());
    return false;
  }
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO) || !mAudioBackend.init())
    METRO_WARN("Ses backend'i baslatilamadi: %s", SDL_GetError());
  METRO_INFO("SDL video driver: %s", SDL_GetCurrentVideoDriver());

  mWindow = SDL_CreateWindow(
      "Metro — M4 Simülasyonu", 1280, 720,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (mWindow == nullptr) {
    METRO_ERROR("SDL_CreateWindow: %s", SDL_GetError());
    return false;
  }

  if (!mContext.init(mWindow)) return false;
  if (!mRenderer.init(mContext, mWindow, station.modelPath,
                      station.routeLength, station.platformWidth,
                      station.trackGauge, station.trainWidth,
                      station.trainHeight, station.columnSpacing,
                      station.platformEdgePosition,
                      station.stopPosition, [&station] {
                        std::vector<float> positions;
                        positions.reserve(station.stops.size());
                        for (const sim::Stop& stop : station.stops) {
                          positions.push_back(stop.position);
                        }
                        return positions;
                      }(), station.blockCount)) return false;

  // Editor sahnesini diskteki son çalışma ile geri yükle; yoksa varsayılan ağacı kullan.
  if (std::filesystem::exists(mEditorUI.scenePath())) {
    if (!mEditorScene.load(mEditorUI.scenePath()))
      METRO_WARN("Editor sahnesi yuklenemedi: %s", mEditorUI.scenePath().c_str());
  }
  mEditorMode = true;
  mPlayMode = false;

  METRO_INFO("Pencere acildi; dongu basliyor (kapatmak icin pencereyi kapat)");
  METRO_INFO("Kontroller: Yukari=cekis Asagi=fren Space=acil-fren O/C=kapi F1=kabin F2=takip F3=serbest W/A/S/D");
  return true;
}

void Application::handleEvent(const SDL_Event& e) {
  mRenderer.processEditorEvent(e);
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
      // Free-look: sağ veya orta mouse basılı tutulurken kamera başı döner.
      // Editörde MMB orbit/pan için ImGui tarafından işlenir.
      if (!mEditorMode &&
          (e.button.button == SDL_BUTTON_RIGHT ||
           e.button.button == SDL_BUTTON_MIDDLE) &&
          !mRenderer.editorWantsMouse()) {
        mMouseCaptured = true;
        mCameraViewMode = CameraViewMode::Free;
        SDL_SetWindowRelativeMouseMode(mWindow, true);
      }
      break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (!mEditorMode &&
          (e.button.button == SDL_BUTTON_RIGHT ||
           e.button.button == SDL_BUTTON_MIDDLE)) {
        mMouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(mWindow, false);
      }
      break;
    case SDL_EVENT_KEY_DOWN:
      if (e.key.key == SDLK_F4) {
        mEditorMode = !mEditorMode;
        mPlayMode = false;
        mMouseCaptured = false;
        mCameraViewMode = CameraViewMode::Free;
        SDL_SetWindowRelativeMouseMode(mWindow, false);
      } else if (e.key.key == SDLK_F5) {
        mPlayMode = !mPlayMode;
        if (mPlayMode) {
          mEditorMode = false;
          mMouseCaptured = false;
          SDL_SetWindowRelativeMouseMode(mWindow, false);
        }
      } else if (mEditorMode && (!mRenderer.editorWantsKeyboard() || mEditorUI.viewportHovered()) &&
                 (e.key.key == SDLK_W || e.key.key == SDLK_E || e.key.key == SDLK_R)) {
        if (e.key.key == SDLK_W) mEditorGizmoMode = editor::GizmoMode::Translate;
        if (e.key.key == SDLK_E) mEditorGizmoMode = editor::GizmoMode::Rotate;
        if (e.key.key == SDLK_R) mEditorGizmoMode = editor::GizmoMode::Scale;
      } else if (mEditorMode && (!mRenderer.editorWantsKeyboard() || mEditorUI.viewportHovered()) &&
                 (e.key.key == SDLK_LEFT || e.key.key == SDLK_RIGHT ||
                  e.key.key == SDLK_UP || e.key.key == SDLK_DOWN ||
                  e.key.key == SDLK_PAGEUP || e.key.key == SDLK_PAGEDOWN)) {
        auto* node = mEditorScene.get(mEditorScene.selected());
        if (node != nullptr && !node->locked) {
          const bool largeStep =
              (mKeyboardState != nullptr &&
               (mKeyboardState[SDL_SCANCODE_LSHIFT] ||
                mKeyboardState[SDL_SCANCODE_RSHIFT]));
          const float moveStep = largeStep ? 1.0f : 0.25f;
          const float angleStep = largeStep ? 15.0f : 5.0f;
          const float scaleStep = largeStep ? 0.20f : 0.05f;

          if (mEditorGizmoMode == editor::GizmoMode::Translate) {
            if (e.key.key == SDLK_LEFT) node->transform.position.x -= moveStep;
            if (e.key.key == SDLK_RIGHT) node->transform.position.x += moveStep;
            if (e.key.key == SDLK_UP) node->transform.position.z -= moveStep;
            if (e.key.key == SDLK_DOWN) node->transform.position.z += moveStep;
            if (e.key.key == SDLK_PAGEUP) node->transform.position.y += moveStep;
            if (e.key.key == SDLK_PAGEDOWN) node->transform.position.y -= moveStep;
          } else if (mEditorGizmoMode == editor::GizmoMode::Rotate) {
            if (e.key.key == SDLK_LEFT) node->transform.rotation.y -= angleStep;
            if (e.key.key == SDLK_RIGHT) node->transform.rotation.y += angleStep;
            if (e.key.key == SDLK_UP) node->transform.rotation.x -= angleStep;
            if (e.key.key == SDLK_DOWN) node->transform.rotation.x += angleStep;
            if (e.key.key == SDLK_PAGEUP) node->transform.rotation.z += angleStep;
            if (e.key.key == SDLK_PAGEDOWN) node->transform.rotation.z -= angleStep;
          } else {
            const float delta = scaleStep * (e.key.key == SDLK_LEFT || e.key.key == SDLK_DOWN ? -1.0f : 1.0f);
            if (e.key.key == SDLK_PAGEUP || e.key.key == SDLK_PAGEDOWN)
              node->transform.scale += glm::vec3(
                  e.key.key == SDLK_PAGEUP ? scaleStep : -scaleStep);
            else
              node->transform.scale += glm::vec3(delta);
            node->transform.scale = glm::max(node->transform.scale, glm::vec3(0.05f));
          }
        }
      } else if (mEditorMode && (!mRenderer.editorWantsKeyboard() || mEditorUI.viewportHovered()) && e.key.key == SDLK_F) {
        const auto* node = mEditorScene.get(mEditorScene.selected());
        if (node != nullptr) {
          const glm::vec3 target = mEditorScene.worldPosition(node->id);
          mCamera.position = target + glm::vec3(6.0f, 3.5f, 6.0f);
          const glm::vec3 direction = glm::normalize(target - mCamera.position);
          mCamera.yaw = glm::degrees(std::atan2(direction.z, direction.x));
          mCamera.pitch = glm::degrees(std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
          mMouseCaptured = false;
          SDL_SetWindowRelativeMouseMode(mWindow, false);
        }
      } else if (mEditorMode && (!mRenderer.editorWantsKeyboard() || mEditorUI.viewportHovered()) &&
                 e.key.key == SDLK_L) {
        if (auto* node = mEditorScene.get(mEditorScene.selected()))
          node->locked = !node->locked;
      } else if (!mRenderer.editorWantsKeyboard() && !mEditorMode &&
                 e.key.key == SDLK_F6) {
        mTrain.reset(0.0f);
        mPhysics.reset(0.0f);
        mSignal.resize(mSignal.blockCount());
        mPassengers.reset(mInitialWaitingPassengers);
        mPassengers.setNavGraph(mPassengerNav);
        mStopDwellSeconds = 0.0f;
        mTerminalServiced = false;
        mLastServicedStop = static_cast<size_t>(-1);
        mLastSignalBlock = static_cast<size_t>(-1);
        mAudioEvents.drain();
      } else if (!mRenderer.editorWantsKeyboard() && !mEditorMode &&
                 e.key.key == SDLK_SPACE && !mEmergencyBrakeHeld) {
        mEmergencyBrakeHeld = true;
        mAudioEvents.push({audio::EventType::EmergencyBrake, 0});
      } else if (!mRenderer.editorWantsKeyboard() && !mEditorMode &&
                 e.key.key == SDLK_H && !mHornHeld) {
        mHornHeld = true;
        mAudioEvents.push({audio::EventType::Horn, 0});
      } else if (!mRenderer.editorWantsKeyboard() && e.key.key == SDLK_F1) {
        mCameraViewMode = CameraViewMode::Cab;
        mMouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(mWindow, false);
      } else if (!mRenderer.editorWantsKeyboard() && e.key.key == SDLK_F2) {
        mCameraViewMode = CameraViewMode::Chase;
        mMouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(mWindow, false);
      } else if (!mRenderer.editorWantsKeyboard() && e.key.key == SDLK_F3) {
        mCameraViewMode = CameraViewMode::Free;
        mMouseCaptured = true;
        SDL_SetWindowRelativeMouseMode(mWindow, true);
      } else if (e.key.key == SDLK_ESCAPE && mMouseCaptured) {
        mMouseCaptured = false;
        SDL_SetWindowRelativeMouseMode(mWindow, false);
      }
      break;
    case SDL_EVENT_KEY_UP:
      if (e.key.key == SDLK_H)
        mHornHeld = false;
      if (e.key.key == SDLK_SPACE)
        mEmergencyBrakeHeld = false;
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

  if (mEditorMode && !mPlayMode) {
    if (!mRenderer.editorWantsKeyboard())
      mPlayMode = false;
    // Edit modunda tren fizik simülasyonu durur; kamera kontrolü aşağıda devam eder.
  }
  const bool editorInput = mEditorMode && mRenderer.editorWantsKeyboard();
  const bool simulate = !mEditorMode || mPlayMode;
  const bool throttle = simulate && !editorInput && mKeyboardState[SDL_SCANCODE_UP];
  const bool emergencyBrake = simulate && !editorInput && mKeyboardState[SDL_SCANCODE_SPACE];
  const bool brake = simulate && !editorInput &&
                     (mKeyboardState[SDL_SCANCODE_DOWN] || emergencyBrake);
  bool platformAligned = false;
  for (const sim::Stop& stop : mRoute.stops()) {
    if (std::abs(mTrain.position() - stop.position) <= 0.5f) {
      platformAligned = true;
      break;
    }
  }
  platformAligned = platformAligned && mTrain.speed() < 0.05f;
  if (simulate && !editorInput && mKeyboardState[SDL_SCANCODE_O]) {
    mTrain.requestDoorsOpen(true, platformAligned);
  }
  if (simulate && !editorInput && mKeyboardState[SDL_SCANCODE_C]) {
    mTrain.requestDoorsOpen(false, false);
  }
  if (simulate) {
    const size_t currentBlock =
        std::min(static_cast<size_t>(mTrain.position() / mBlockLength),
                 mSignal.blockCount() - 1);
    for (size_t block = 0; block < mSignal.blockCount(); ++block) {
      mSignal.setOccupied(block, block == currentBlock);
    }
    if (currentBlock != mLastSignalBlock) {
      mAudioEvents.push({audio::EventType::SignalChanged, currentBlock});
      mLastSignalBlock = currentBlock;
    }
    const size_t nextBlock = currentBlock + 1;
    const bool signalClear = nextBlock >= mSignal.blockCount() ||
                             mSignal.canEnter(nextBlock);
    const sim::Stop& upcomingStop = mRoute.nextStop(mTrain.position(), mRouteLength);
    const float stopTarget = upcomingStop.position;
    const float stopDwellLimit = upcomingStop.dwellSeconds >= 0.0f
                                     ? upcomingStop.dwellSeconds
                                     : mStopDwellSecondsLimit;
    mPhysics.step(dt, mTrain, throttle, brake, signalClear, stopTarget);
    const bool atStop = mTrain.position() >= stopTarget - 0.5f &&
                        mTrain.position() <= stopTarget + 0.5f &&
                        mTrain.speed() < 0.05f;
    const bool atTerminal = mTrain.position() >= mRouteLength &&
                            mTrain.speed() < 0.05f;
    if (atStop || atTerminal) {
      size_t servicedStop = static_cast<size_t>(-1);
      for (size_t i = 0; i < mRoute.stopCount(); ++i) {
        if (std::abs(mTrain.position() - mRoute.stops()[i].position) <= 0.5f) {
          servicedStop = i;
          break;
        }
      }
      const bool wasOpen = mTrain.doorsOpen();
      mTrain.requestDoorsOpen(true, true);
      if (!wasOpen && mTrain.doorsOpen()) {
        mAudioEvents.push({atTerminal ? audio::EventType::TerminalServiced
                                      : audio::EventType::StopArrived,
                           servicedStop});
        mAudioEvents.push({audio::EventType::DoorOpened, servicedStop});
      }
      if (servicedStop != static_cast<size_t>(-1) &&
          servicedStop != mLastServicedStop) {
        mPassengers.serviceStop(servicedStop, mRoute.stopCount(),
                                servicedStop + 1 == mRoute.stopCount());
        if (servicedStop + 1 < mRoute.stopCount())
          mPassengers.setBoardingDestination(servicedStop + 1);
        mLastServicedStop = servicedStop;
      }
      if (!atTerminal) {
        mStopDwellSeconds += dt;
        if (mStopDwellSeconds >= stopDwellLimit) {
          mTrain.requestDoorsOpen(false, false);
          mAudioEvents.push({audio::EventType::DoorClosed, servicedStop});
          mAudioEvents.push({audio::EventType::TrainDeparted, servicedStop});
          mStopDwellSeconds = 0.0f;
        }
      }
    } else {
      mStopDwellSeconds = 0.0f;
    }
    if (atTerminal) mTerminalServiced = true;
    mPassengers.update(dt, mTrain.doorsOpen(), mTrain.speed() < 0.05f,
                      !atTerminal);
    consumeAudioEvents();
    mAudioBackend.updateTrainSound(mTrain.speed(), mTrain.acceleration());
  }

  const bool editorCameraMove =
      mEditorMode && mEditorUI.viewportHovered();
  if (!mMouseCaptured && !editorCameraMove)
    return;

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

void Application::consumeAudioEvents() {
  audio::Event event{};
  while (mAudioEvents.tryPop(event)) {
    mAudioBackend.play(event.type);
    METRO_INFO("ses olayi: %s (durak %zu)",
               audio::eventName(event.type), event.stopIndex);
  }
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
    dt = std::min(dt, 0.1f);

    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);

    if (mMinimized) {
      SDL_Delay(50);
      continue;
    }

    if (mResized) {
      mRenderer.onResize();
      mResized = false;
    }

    update(dt);

    const sim::Stop& hudStop =
        mRoute.nextStop(mTrain.position(), mRouteLength);
    const float hudDistance =
        std::max(0.0f, hudStop.position - mTrain.position());
    const size_t hudCurrentBlock =
        mSignal.blockCount() > 0
            ? std::min(static_cast<size_t>(mTrain.position() / mBlockLength),
                       mSignal.blockCount() - 1)
            : 0;
    const size_t hudNextBlock = hudCurrentBlock + 1;
    const sim::SignalAspect hudAspect =
        mSignal.blockCount() > 0 ? mSignal.aspect(hudNextBlock)
                                 : sim::SignalAspect::Proceed;
    const char* hudSignal = hudAspect == sim::SignalAspect::Stop
                                ? "STOP"
                                : (hudAspect == sim::SignalAspect::Caution
                                       ? "CAUTION"
                                       : "PROCEED");
    editor::GameplayHUDData gameplayHUD{};
    gameplayHUD.nextStation = hudStop.name.c_str();
    gameplayHUD.distanceToNextStation = hudDistance;
    gameplayHUD.stationCount = mRoute.stopCount();
    gameplayHUD.nextStationIndex = 0;
    for (size_t i = 0; i < mRoute.stopCount(); ++i) {
      if (mRoute.stops()[i].name == hudStop.name &&
          std::abs(mRoute.stops()[i].position - hudStop.position) < 0.01f) {
        gameplayHUD.nextStationIndex = i;
        break;
      }
    }
    gameplayHUD.signalAspect = hudSignal;
    gameplayHUD.recommendedSpeedMps = mTrain.recommendedSpeed(hudDistance);
    gameplayHUD.accelerationMps2 = mTrain.acceleration();
    gameplayHUD.dwellSeconds = mStopDwellSeconds;
    gameplayHUD.dwellLimitSeconds =
        hudStop.dwellSeconds >= 0.0f ? hudStop.dwellSeconds : mStopDwellSecondsLimit;
    gameplayHUD.doorsOpen = mTrain.doorsOpen();
    gameplayHUD.doorOpenFraction = mTrain.doorOpenFraction();
    gameplayHUD.onboardPassengers = mPassengers.onboard();
    gameplayHUD.waitingPassengers = mPassengers.waiting();
    gameplayHUD.tractionActive = !mEditorMode &&
                                  mKeyboardState[SDL_SCANCODE_UP] &&
                                  !mKeyboardState[SDL_SCANCODE_DOWN] &&
                                  !mKeyboardState[SDL_SCANCODE_SPACE];
    gameplayHUD.serviceBrakeActive = !mEditorMode &&
                                     mKeyboardState[SDL_SCANCODE_DOWN] &&
                                     !mKeyboardState[SDL_SCANCODE_SPACE];
    gameplayHUD.emergencyBrakeActive = !mEditorMode &&
                                       mKeyboardState[SDL_SCANCODE_SPACE];
    gameplayHUD.overspeed = !mEditorMode &&
                            mTrain.speed() > gameplayHUD.recommendedSpeedMps + 1.0f &&
                            gameplayHUD.distanceToNextStation > 25.0f;

    mRenderer.beginEditorFrame();
    mEditorUI.draw(mEditorScene, mEditorMode, mPlayMode, mEditorGizmoMode,
                   mCamera, mTrain.speed(), mTrain.position(), mRouteLength,
                   mContext.deviceName(), mDisplayFps, gameplayHUD);
    mRenderer.finishEditorFrame();

    const float renderTrainPosition = mPhysics.interpolatedPosition(mTrain);
    Camera renderCamera = mCamera;
    if (mEditorMode) {
      renderCamera = mCamera;
      renderCamera.yaw = mCamera.yaw;
      renderCamera.pitch = mCamera.pitch;
    } else if (mCameraViewMode == CameraViewMode::Cab) {
      const float motionSway =
          std::clamp(mTrain.acceleration() * 0.018f, -0.035f, 0.035f);
      const float roadVibration =
          std::sin(static_cast<float>(nowNs) * 0.000006f) *
          (0.004f + std::clamp(mTrain.speed() / 22.2f, 0.0f, 1.0f) * 0.009f);
      renderCamera.position =
          glm::vec3(motionSway, 1.48f + roadVibration,
                    -renderTrainPosition - 39.25f);
      renderCamera.yaw = -90.0f;
      renderCamera.pitch = -3.0f - motionSway * 25.0f;
    } else if (mCameraViewMode == CameraViewMode::Chase) {
      renderCamera.position =
          glm::vec3(4.8f, 4.2f, -renderTrainPosition + 22.0f);
      renderCamera.yaw = -96.0f;
      renderCamera.pitch = -7.0f;
    }

    std::vector<glm::vec2> passengerPositions;
    const size_t waitingVisualCount = std::min<size_t>(mPassengers.waiting(), 32);
    passengerPositions.reserve(mPassengers.walkingAgents().size() +
                               waitingVisualCount);
    for (const auto& passenger : mPassengers.walkingAgents())
      passengerPositions.push_back(passenger.position(mPassengerNav));

    for (size_t i = 0; i < waitingVisualCount; ++i) {
      const size_t row = i / 4;
      const size_t col = i % 4;
      const float x = -3.1f + static_cast<float>(col) * 0.85f;
      const float z = -12.0f - static_cast<float>(row) * 3.4f;
      passengerPositions.emplace_back(x, z);
    }

    std::vector<rhi::Renderer::EditorMarker> editorMarkers;
    std::vector<rhi::Renderer::EditorRenderOverride> editorOverrides;

    if (mEditorMode) {
      editorMarkers.reserve(mEditorScene.visibleCount());

      for (const auto entity : mEditorScene.order()) {
        const auto* node = mEditorScene.get(entity);
        if (node == nullptr ||
            node->type == "Folder" || node->type == "Scene" ||
            node->type == "Simulation")
          continue;

        const bool selected = entity == mEditorScene.selected();

        if (node->type == "Train") {
          rhi::Renderer::EditorRenderOverride override{};
          override.transform = mEditorScene.worldTransform(entity);
          override.visible = node->visible;
          if (!node->asset.empty() && node->asset.rfind("Builtin/", 0) != 0) {
            override.kind = rhi::Renderer::EditorRenderKind::Asset;
            override.index = static_cast<size_t>(-1);
            override.assetPath = node->asset;
          } else {
            override.kind = rhi::Renderer::EditorRenderKind::Train;
          }
          editorOverrides.push_back(std::move(override));
        } else if (node->type == "Station") {
          std::string stationName = node->name;
          constexpr std::string_view suffix = " Station";
          if (stationName.size() > suffix.size() &&
              stationName.ends_with(suffix))
            stationName.erase(stationName.size() - suffix.size());

          for (size_t stationIndex = 0; stationIndex < mRoute.stopCount(); ++stationIndex) {
            if (mRoute.stops()[stationIndex].name != stationName)
              continue;

            rhi::Renderer::EditorRenderOverride override{};
            override.index = stationIndex;
            override.transform = mEditorScene.worldTransform(entity);
            override.visible = node->visible;
            if (!node->asset.empty() && node->asset.rfind("Station/", 0) != 0) {
              override.kind = rhi::Renderer::EditorRenderKind::Asset;
              override.assetPath = node->asset;
            } else {
              override.kind = rhi::Renderer::EditorRenderKind::Station;
            }
            editorOverrides.push_back(std::move(override));
            break;
          }
        }

        if (!node->visible)
          continue;

        rhi::Renderer::EditorMarker marker{};
        marker.position = mEditorScene.worldPosition(entity) +
                          glm::vec3(0.0f, 0.55f, 0.0f);
        marker.scale = selected ? glm::vec3(0.42f) : glm::vec3(0.22f);

        if (node->type == "Train")
          marker.color = selected ? glm::vec4(1.0f, 0.72f, 0.12f, 1.0f)
                                   : glm::vec4(0.16f, 0.52f, 0.95f, 1.0f);
        else if (node->type == "Station")
          marker.color = selected ? glm::vec4(0.96f, 0.82f, 0.20f, 1.0f)
                                   : glm::vec4(0.22f, 0.82f, 0.48f, 1.0f);
        else if (node->type == "Rail")
          marker.color = {0.78f, 0.80f, 0.84f, 1.0f};
        else
          marker.color = {0.72f, 0.38f, 0.92f, 1.0f};

        editorMarkers.push_back(marker);

        if (selected) {
          const glm::vec3 gizmoOrigin =
              mEditorScene.worldPosition(entity) + glm::vec3(0.0f, 0.55f, 0.0f);
          constexpr float axisLength = 1.6f;
          constexpr float axisThickness = 0.055f;

          rhi::Renderer::EditorMarker xAxis{};
          xAxis.position = gizmoOrigin + glm::vec3(axisLength * 0.5f, 0.0f, 0.0f);
          xAxis.scale = glm::vec3(axisLength, axisThickness, axisThickness);
          xAxis.color = {0.92f, 0.20f, 0.18f, 1.0f};
          editorMarkers.push_back(xAxis);

          rhi::Renderer::EditorMarker yAxis{};
          yAxis.position = gizmoOrigin + glm::vec3(0.0f, axisLength * 0.5f, 0.0f);
          yAxis.scale = glm::vec3(axisThickness, axisLength, axisThickness);
          yAxis.color = {0.22f, 0.88f, 0.30f, 1.0f};
          editorMarkers.push_back(yAxis);

          rhi::Renderer::EditorMarker zAxis{};
          zAxis.position = gizmoOrigin + glm::vec3(0.0f, 0.0f, axisLength * 0.5f);
          zAxis.scale = glm::vec3(axisThickness, axisThickness, axisLength);
          zAxis.color = {0.20f, 0.48f, 0.95f, 1.0f};
          editorMarkers.push_back(zAxis);
        }
      }
    }

    mRenderer.drawFrame(renderCamera, renderTrainPosition, mTrain.speed(), mTrain.doorOpenFraction(),
                        mSignal.occupiedBlocks(), passengerPositions, editorMarkers,
                        editorOverrides);
    ++frameCount;
    if (dt > 0.0001f)
      mDisplayFps = 0.9f * mDisplayFps + 0.1f / dt;

    if (nowNs - mLastTitleNs >= Uint64(250000000)) {
      const sim::Stop& titleStop =
          mRoute.nextStop(mTrain.position(), mRouteLength);
      char title[256];
      std::snprintf(title, sizeof(title),
                    "Metro M4 | %5.1f km/h | %s | Kapi: %s",
                    mTrain.speed() * 3.6f, titleStop.name.c_str(),
                    mTrain.doorsOpen() ? "ACIK" : "KAPALI");
      SDL_SetWindowTitle(mWindow, title);
      mLastTitleNs = nowNs;
    }

    if (nowNs - lastStatsNs >= Uint64(2e9)) {
      const double secs = double(nowNs - lastStatsNs) / 1e9;
      METRO_INFO("fps: %.1f", double(frameCount) / secs);
      METRO_INFO("tren: %.1f km/saat, konum %.1f m",
                 mTrain.speed() * 3.6f, mTrain.position());
      METRO_INFO("kapi: %s", mTrain.doorsOpen() ? "acik" : "kapali");
      METRO_INFO("yolcu: %zu trende, %zu bekliyor",
                 mPassengers.onboard(), mPassengers.waiting());
      METRO_INFO("yuruyen yolcu: %zu aktif",
                 mPassengers.activeWalkingAgents());
      const size_t statsBlock =
          std::min(static_cast<size_t>(mTrain.position() / mBlockLength),
                   mSignal.blockCount() - 1);
      const size_t statsNextBlock = statsBlock + 1;
      METRO_INFO("sinyal: blok %zu %s", statsNextBlock,
                 statsNextBlock >= mSignal.blockCount() ||
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

  vkDeviceWaitIdle(mContext.device());
  shutdown();
  return 0;
}

void Application::shutdown() {
  mAudioBackend.shutdown();
  mRenderer.shutdown();
  mContext.shutdown();
  if (mWindow != nullptr) {
    SDL_DestroyWindow(mWindow);
    mWindow = nullptr;
  }
  SDL_Quit();
}

} // namespace metro::app
