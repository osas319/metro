#pragma once

#include "editor/EditorScene.hpp"
#include "app/Camera.hpp"

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
};

class UI {
public:
  void draw(Scene& scene, bool& editorMode, bool& playMode, GizmoMode& gizmoMode,
            app::Camera& camera, float trainSpeedMps, float trainPosition,
            float routeLength, const char* gpuName, float fps,
            const GameplayHUDData& gameplay);

  const std::string& scenePath() const { return mScenePath; }

private:
  void drawToolbar(Scene& scene, bool& editorMode, bool& playMode, GizmoMode& gizmoMode);
  void drawHierarchy(Scene& scene);
  void drawInspector(Scene& scene);
  void drawContentBrowser(Scene& scene);
  void drawViewport(Scene& scene, app::Camera& camera, GizmoMode& gizmoMode,
                    float trainSpeedMps, float trainPosition,
                    float routeLength, const char* gpuName, float fps);
  void drawGameplayHUD(float trainSpeedMps, float trainPosition,
                       float routeLength, const GameplayHUDData& gameplay);
  void drawEntityTree(Scene& scene, entt::entity entity);

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
};

} // namespace metro::editor
