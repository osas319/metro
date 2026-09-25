#pragma once

#include "editor/EditorScene.hpp"
#include "app/Camera.hpp"

#include <imgui.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <filesystem>
#include <string>

namespace metro::editor {

struct GameplayHUDData {
  const char* nextStation = "Terminal";
  float distanceToNextStation = 0.0f;
  const char* signalAspect = "PROCEED";
  float recommendedSpeedMps = 0.0f;
  float accelerationMps2 = 0.0f;
  float dwellSeconds = 0.0f;
  float dwellLimitSeconds = 0.0f;
  bool doorsOpen = false;
  float doorOpenFraction = 0.0f;
  size_t onboardPassengers = 0;
  size_t waitingPassengers = 0;
  size_t nextStationIndex = 0;
  size_t stationCount = 0;
  bool tractionActive = false;
  bool serviceBrakeActive = false;
  bool emergencyBrakeActive = false;
  bool overspeed = false;
};

class UI {
public:
  void draw(Scene& scene, bool& editorMode, bool& playMode, GizmoMode& gizmoMode,
            app::Camera& camera, float trainSpeedMps, float trainPosition,
            float routeLength, const char* gpuName, float fps,
            ImTextureID editorBlurTexture,
            const GameplayHUDData& gameplay);

  const std::string& scenePath() const { return mScenePath; }
  bool viewportHovered() const { return mViewportHovered; }
  bool viewportContains(float x, float y) const {
    return x >= mViewportMin.x && y >= mViewportMin.y &&
           x < mViewportMax.x && y < mViewportMax.y;
  }

private:
  void drawToolbar(Scene& scene, bool& editorMode, bool& playMode, GizmoMode& gizmoMode,
                   ImTextureID editorBlurTexture);
  void drawHierarchy(Scene& scene, ImTextureID editorBlurTexture);
  void drawInspector(Scene& scene, ImTextureID editorBlurTexture);
  void drawContentBrowser(Scene& scene, ImTextureID editorBlurTexture);
  void drawViewport(Scene& scene, app::Camera& camera, GizmoMode& gizmoMode,
                    ImTextureID editorBlurTexture,
                    float trainSpeedMps, float trainPosition,
                    float routeLength, const char* gpuName, float fps);
  void drawGameplayHUD(float trainSpeedMps, float trainPosition,
                       float routeLength, const GameplayHUDData& gameplay);
  void drawEntityTree(Scene& scene, entt::entity entity);
  void drawGlassBackground(ImTextureID editorBlurTexture, float tintAlpha = 0.56f);

  std::string mScenePath = "assets/scenes/m4_editor.scene";
  char mPathBuffer[256] = "assets/scenes/m4_editor.scene";
  char mRenameBuffer[128] = {};
  bool mPathPopup = false;
  entt::entity mLastSelected{entt::null};
  bool mCreatePopup = false;
  std::string mStatus = "Ready";
  float mStatusTimer = 0.0f;
  std::filesystem::path mContentPath = "assets";
  entt::entity mGizmoEntity{entt::null};
  int mGizmoAxis = -1;
  bool mViewportHovered = false;
  glm::vec2 mViewportMin{0.0f};
  glm::vec2 mViewportMax{0.0f};
};

} // namespace metro::editor
