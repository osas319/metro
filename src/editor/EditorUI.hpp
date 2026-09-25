#pragma once

#include "editor/EditorScene.hpp"

#include <glm/vec3.hpp>

#include <string>

namespace metro::editor {

class UI {
public:
  void draw(Scene& scene, bool& editorMode, bool& playMode,
            float trainSpeedMps, float trainPosition, float routeLength,
            const char* gpuName, float fps);

  const std::string& scenePath() const { return mScenePath; }

private:
  void drawToolbar(Scene& scene, bool& editorMode, bool& playMode);
  void drawHierarchy(Scene& scene);
  void drawInspector(Scene& scene);
  void drawContentBrowser(Scene& scene);
  void drawViewport(float trainSpeedMps, float trainPosition,
                    float routeLength, const char* gpuName, float fps);
  void drawEntityTree(Scene& scene, entt::entity entity);

  std::string mScenePath = "assets/scenes/m4_editor.scene";
  char mPathBuffer[256] = "assets/scenes/m4_editor.scene";
  char mRenameBuffer[128] = {};
  bool mPathPopup = false;
  entt::entity mLastSelected{entt::null};
  bool mCreatePopup = false;
  std::string mStatus = "Ready";
  float mStatusTimer = 0.0f;
};

} // namespace metro::editor
