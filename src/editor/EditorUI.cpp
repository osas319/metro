#include "editor/EditorUI.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <vector>

namespace metro::editor {

namespace {

ImVec4 entityTypeColor(const char* type) {
  if (std::string(type) == "Train") return {0.18f, 0.55f, 0.95f, 1.0f};
  if (std::string(type) == "Station") return {0.35f, 0.82f, 0.48f, 1.0f};
  if (std::string(type) == "Rail") return {0.75f, 0.75f, 0.78f, 1.0f};
  if (std::string(type) == "Folder") return {0.92f, 0.70f, 0.25f, 1.0f};
  if (std::string(type) == "Simulation") return {0.78f, 0.40f, 0.92f, 1.0f};
  return {0.65f, 0.67f, 0.72f, 1.0f};
}

bool hasChildren(const Scene& scene, entt::entity entity) {
  for (auto id : scene.order()) {
    const auto* node = scene.get(id);
    if (node != nullptr && node->parent == entity)
      return true;
  }
  return false;
}

bool isBelow(const Scene& scene, entt::entity entity, entt::entity possibleAncestor) {
  if (entity == entt::null || possibleAncestor == entt::null)
    return false;

  entt::entity current = entity;
  for (size_t guard = 0; guard < scene.size() + 1; ++guard) {
    const auto* node = scene.get(current);
    if (node == nullptr)
      return false;
    if (node->parent == possibleAncestor)
      return true;
    if (node->parent == entt::null)
      return false;
    current = node->parent;
  }
  return true;
}

entt::entity creationParent(const Scene& scene) {
  const auto* selected = scene.get(scene.selected());
  if (selected != nullptr &&
      (selected->type == "Scene" || selected->type == "Folder"))
    return selected->id;

  if (selected != nullptr)
    return selected->parent;

  return entt::null;
}

std::string parentLabel(const Scene& scene, entt::entity parent) {
  if (parent == entt::null)
    return "None";
  const auto* node = scene.get(parent);
  return node != nullptr ? node->name : "None";
}


struct ProjectedPoint {
  bool visible = false;
  ImVec2 screen{};
};

ProjectedPoint projectWorldPoint(const app::Camera& camera,
                                 const ImVec2& windowPos,
                                 const ImVec2& windowSize,
                                 const glm::vec3& world) {
  if (windowSize.x <= 1.0f || windowSize.y <= 1.0f)
    return {};

  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 projection =
      camera.getProjectionMatrix(windowSize.x / windowSize.y);
  const glm::vec4 clip = projection * view * glm::vec4(world, 1.0f);
  if (!std::isfinite(clip.w) || clip.w <= 0.001f)
    return {};

  const glm::vec3 ndc = glm::vec3(clip) / clip.w;
  if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y))
    return {};

  ProjectedPoint result;
  result.visible = true;
  result.screen = {
      windowPos.x + (ndc.x * 0.5f + 0.5f) * windowSize.x,
      windowPos.y + (ndc.y * 0.5f + 0.5f) * windowSize.y};
  return result;
}

float distanceToSegment(const ImVec2& point, const ImVec2& a, const ImVec2& b,
                        float* outT = nullptr) {
  const ImVec2 ab{b.x - a.x, b.y - a.y};
  const float ab2 = ab.x * ab.x + ab.y * ab.y;
  if (ab2 <= 0.001f) {
    if (outT) *outT = 0.0f;
    const ImVec2 d{point.x - a.x, point.y - a.y};
    return std::sqrt(d.x * d.x + d.y * d.y);
  }

  const ImVec2 ap{point.x - a.x, point.y - a.y};
  const float t = std::clamp((ap.x * ab.x + ap.y * ab.y) / ab2, 0.0f, 1.0f);
  if (outT) *outT = t;
  const ImVec2 closest{a.x + ab.x * t, a.y + ab.y * t};
  const ImVec2 d{point.x - closest.x, point.y - closest.y};
  return std::sqrt(d.x * d.x + d.y * d.y);
}

} // namespace

void UI::draw(Scene& scene, bool& editorMode, bool& playMode, GizmoMode& gizmoMode,
              app::Camera& camera, float trainSpeedMps, float trainPosition,
              float routeLength, const char* gpuName, float fps,
              const GameplayHUDData& gameplay) {
  if (!editorMode) {
    mViewportHovered = false;
    drawGameplayHUD(trainSpeedMps, trainPosition, routeLength, gameplay);
    return;
  }

  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                               ImGuiDockNodeFlags_PassthruCentralNode);

  ImGuiIO& io = ImGui::GetIO();
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    mStatus = scene.save(mScenePath) ? "Scene saved (Ctrl+S)" : "Save failed";
  }

  drawToolbar(scene, editorMode, playMode, gizmoMode);
  drawHierarchy(scene);
  drawInspector(scene);
  drawContentBrowser(scene);
  drawViewport(scene, camera, gizmoMode, trainSpeedMps, trainPosition,
               routeLength, gpuName, fps);
}

void UI::drawToolbar(Scene& scene, bool& editorMode, bool& playMode, GizmoMode& gizmoMode) {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize({viewport->WorkSize.x, 54.0f});

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings;

  if (!ImGui::Begin("##MetroToolbar", nullptr, flags)) {
    ImGui::End();
    return;
  }

  if (ImGui::Button(playMode ? "||  PAUSE" : ">  PLAY", {92.0f, 32.0f}))
    playMode = !playMode;

  ImGui::SameLine();
  if (ImGui::Button("STOP", {72.0f, 32.0f}))
    playMode = false;

  ImGui::SameLine();
  const bool moveMode = gizmoMode == GizmoMode::Translate;
  const bool rotateMode = gizmoMode == GizmoMode::Rotate;
  const bool scaleMode = gizmoMode == GizmoMode::Scale;
  if (moveMode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  if (ImGui::Button("W  MOVE", {82.0f, 32.0f})) gizmoMode = GizmoMode::Translate;
  if (moveMode) ImGui::PopStyleColor();

  ImGui::SameLine();
  if (rotateMode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  if (ImGui::Button("E  ROTATE", {90.0f, 32.0f})) gizmoMode = GizmoMode::Rotate;
  if (rotateMode) ImGui::PopStyleColor();

  ImGui::SameLine();
  if (scaleMode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  if (ImGui::Button("R  SCALE", {82.0f, 32.0f})) gizmoMode = GizmoMode::Scale;
  if (scaleMode) ImGui::PopStyleColor();

  ImGui::SameLine();
  if (ImGui::Button(" +  ENTITY", {92.0f, 32.0f})) {
    scene.create("New Entity", "Empty", creationParent(scene));
    mStatus = "Created entity";
  }

  ImGui::SameLine();
  if (ImGui::Button("DUPLICATE", {96.0f, 32.0f})) {
    scene.duplicate(scene.selected());
    mStatus = "Entity duplicated";
  }

  ImGui::SameLine();
  if (ImGui::Button("DELETE", {76.0f, 32.0f})) {
    scene.destroy(scene.selected());
    mStatus = "Entity deleted";
  }

  ImGui::SameLine();
  if (ImGui::Button("SAVE", {72.0f, 32.0f})) {
    mStatus = scene.save(mScenePath) ? "Scene saved" : "Save failed";
  }

  ImGui::SameLine();
  if (ImGui::Button("LOAD", {72.0f, 32.0f})) {
    mStatus = scene.load(mScenePath) ? "Scene loaded" : "Load failed";
    mLastSelected = entt::null;
  }

  ImGui::SameLine();
  ImGui::TextDisabled("|");

  ImGui::SameLine();
  ImGui::TextUnformatted("Metro Editor");
  ImGui::SameLine();
  ImGui::TextDisabled("Scene");
  ImGui::SameLine();
  ImGui::TextUnformatted(mScenePath.c_str());

  ImGui::SameLine();
  ImGui::TextDisabled("| %s", mStatus.c_str());

  ImGui::SameLine();
  const float right = ImGui::GetContentRegionAvail().x;
  if (right > 150.0f) ImGui::SameLine(ImGui::GetCursorPosX() + right - 150.0f);
  if (ImGui::Button("GAME VIEW", {110.0f, 32.0f}))
    editorMode = false;

  ImGui::End();
}

void UI::drawHierarchy(Scene& scene) {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos({viewport->WorkPos.x, viewport->WorkPos.y + 54.0f},
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({285.0f, viewport->WorkSize.y * 0.62f},
                           ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Scene Hierarchy"))
  {
    ImGui::End();
    return;
  }

  ImGui::TextDisabled("%zu entities", scene.size());
  ImGui::SameLine();
  if (ImGui::SmallButton("+"))
    ImGui::OpenPopup("CreateEntityPopup");

  if (ImGui::BeginPopup("CreateEntityPopup")) {
    const entt::entity parent = creationParent(scene);
    if (ImGui::MenuItem("Empty")) { scene.create("Empty", "Empty", parent); mStatus = "Created Empty"; }
    if (ImGui::MenuItem("Folder")) { scene.create("New Folder", "Folder", parent); mStatus = "Created Folder"; }
    if (ImGui::MenuItem("Station")) { scene.create("New Station", "Station", parent); mStatus = "Created Station"; }
    if (ImGui::MenuItem("Train")) { scene.create("New Train", "Train", parent); mStatus = "Created Train"; }
    if (ImGui::MenuItem("Rail")) { scene.create("New Rail", "Rail", parent); mStatus = "Created Rail"; }
    ImGui::EndPopup();
  }

  ImGui::Separator();

  for (auto entity : scene.order()) {
    const auto* node = scene.get(entity);
    if (node == nullptr || node->parent != entt::null)
      continue;
    drawEntityTree(scene, entity);
  }

  ImGui::End();
}

void UI::drawEntityTree(Scene& scene, entt::entity entity) {
  auto* node = scene.get(entity);
  if (node == nullptr)
    return;

  const bool children = hasChildren(scene, entity);
  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_SpanAvailWidth |
      ImGuiTreeNodeFlags_OpenOnArrow |
      ImGuiTreeNodeFlags_OpenOnDoubleClick;

  if (!children)
    flags |= ImGuiTreeNodeFlags_Leaf;
  if (scene.selected() == entity)
    flags |= ImGuiTreeNodeFlags_Selected;

  const ImVec4 color = entityTypeColor(node->type.c_str());
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  const bool open = ImGui::TreeNodeEx(
      reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(entity) + 1u)),
      flags, "%s", node->name.c_str());
  ImGui::PopStyleColor();

  if (ImGui::IsItemClicked())
    scene.select(entity);

  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("METRO_ASSET")) {
      if (payload->Data != nullptr && payload->DataSize > 0) {
        node->asset.assign(static_cast<const char*>(payload->Data));
        mStatus = "Asset assigned";
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (open) {
    for (auto child : scene.order()) {
      const auto* childNode = scene.get(child);
      if (childNode != nullptr && childNode->parent == entity)
        drawEntityTree(scene, child);
    }
    ImGui::TreePop();
  }
}

void UI::drawInspector(Scene& scene) {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float width = 340.0f;
  ImGui::SetNextWindowPos(
      {viewport->WorkPos.x + viewport->WorkSize.x - width, viewport->WorkPos.y + 54.0f},
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({width, viewport->WorkSize.y * 0.62f},
                           ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Inspector")) {
    ImGui::End();
    return;
  }

  SceneEntity* node = scene.get(scene.selected());
  if (node == nullptr) {
    ImGui::TextDisabled("No entity selected");
    ImGui::End();
    return;
  }

  if (mLastSelected != scene.selected()) {
    std::snprintf(mRenameBuffer, sizeof(mRenameBuffer), "%s", node->name.c_str());
    mLastSelected = scene.selected();
  }
  if (ImGui::InputText("Name", mRenameBuffer, sizeof(mRenameBuffer),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    node->name = mRenameBuffer;
    mStatus = "Renamed entity";
  }

  ImGui::TextDisabled("Type: %s", node->type.c_str());
  if (!node->asset.empty())
    ImGui::TextDisabled("Asset: %s", node->asset.c_str());

  ImGui::Separator();
  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginDisabled(node->locked);
    ImGui::DragFloat3("Position", &node->transform.position.x, 0.05f);
    ImGui::DragFloat3("Rotation", &node->transform.rotation.x, 0.5f);
    ImGui::DragFloat3("Scale", &node->transform.scale.x, 0.01f, 0.01f, 100.0f);
    if (ImGui::Button("Reset Transform")) {
      node->transform = {};
      node->transform.scale = glm::vec3(1.0f);
      mStatus = "Transform reset";
    }
    ImGui::EndDisabled();

    const glm::vec3 world = scene.worldPosition(node->id);
    ImGui::TextDisabled("World: %.2f, %.2f, %.2f", world.x, world.y, world.z);
  }

  if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Visible", &node->visible);
    ImGui::Checkbox("Locked", &node->locked);
  }

  if (ImGui::CollapsingHeader("Entity", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("ID: %u", entt::to_integral(node->id));

    const std::string currentParent = parentLabel(scene, node->parent);
    if (ImGui::BeginCombo("Parent", currentParent.c_str())) {
      const bool noneSelected = node->parent == entt::null;
      if (ImGui::Selectable("None", noneSelected)) {
        if (scene.setParent(node->id, entt::null))
          mStatus = "Parent cleared";
      }
      if (noneSelected)
        ImGui::SetItemDefaultFocus();

      for (const auto entity : scene.order()) {
        const auto* candidate = scene.get(entity);
        if (candidate == nullptr || candidate->id == node->id ||
            isBelow(scene, candidate->id, node->id))
          continue;

        const bool selectedParent = candidate->id == node->parent;
        if (ImGui::Selectable(candidate->name.c_str(), selectedParent)) {
          if (scene.setParent(node->id, candidate->id))
            mStatus = "Parent changed";
        }
        if (selectedParent)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    if (ImGui::Button("Duplicate")) {
      scene.duplicate(node->id);
      mStatus = "Entity duplicated";
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
      scene.destroy(node->id);
      mStatus = "Entity deleted";
    }
  }

  ImGui::End();
}

void UI::drawContentBrowser(Scene& scene) {
  (void)scene;
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float height = 205.0f;
  ImGui::SetNextWindowPos(
      {viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height});
  ImGui::SetNextWindowSize({viewport->WorkSize.x, height});

  if (!ImGui::Begin("Content Browser", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::End();
    return;
  }

  if (!std::filesystem::exists(mContentPath)) {
    mContentPath = "assets";
  }

  if (ImGui::Button("<-")) {
    if (mContentPath != "assets" && mContentPath.has_parent_path())
      mContentPath = mContentPath.parent_path();
  }
  ImGui::SameLine();
  if (ImGui::Button("HOME"))
    mContentPath = "assets";

  ImGui::SameLine();
  ImGui::TextDisabled("%s", mContentPath.generic_string().c_str());
  ImGui::Separator();

  std::vector<std::filesystem::directory_entry> entries;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(mContentPath, ec)) {
    if (ec)
      break;
    entries.push_back(entry);
  }
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
              std::error_code ea, eb;
              const bool ad = a.is_directory(ea);
              const bool bd = b.is_directory(eb);
              if (ad != bd) return ad > bd;
              return a.path().filename().string() < b.path().filename().string();
            });

  int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / 150.0f));
  if (columns > 8) columns = 8;

  int visibleIndex = 0;
  for (const auto& entry : entries) {
    std::error_code entryEc;
    const bool isDirectory = entry.is_directory(entryEc);
    if (entryEc)
      continue;

    const std::string label = entry.path().filename().string();
    if (!isDirectory) {
      const std::string ext = entry.path().extension().string();
      if (!ext.empty() &&
          ext != ".glb" && ext != ".gltf" && ext != ".scene" &&
          ext != ".json" && ext != ".nav") {
        continue;
      }
    }

    if ((visibleIndex % columns) != 0)
      ImGui::SameLine();
    const std::string buttonLabel =
        isDirectory ? "[DIR] " + label : label;
    if (ImGui::Selectable(buttonLabel.c_str(), false,
                          ImGuiSelectableFlags_AllowDoubleClick,
                          {140.0f, 28.0f})) {
      if (isDirectory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        mContentPath /= label;
      } else if (!isDirectory) {
        mStatus = "Asset selected: " + entry.path().generic_string();
      }
    }

    if (!isDirectory && ImGui::BeginDragDropSource()) {
      const std::string assetPath = entry.path().generic_string();
      ImGui::SetDragDropPayload("METRO_ASSET", assetPath.c_str(),
                                assetPath.size() + 1);
      ImGui::TextUnformatted(label.c_str());
      ImGui::EndDragDropSource();
    }
    ++visibleIndex;
  }

  if (visibleIndex == 0)
    ImGui::TextDisabled("No supported assets in this folder");

  ImGui::End();
}

void UI::drawViewport(Scene& scene, app::Camera& camera, GizmoMode& gizmoMode,
                      float trainSpeedMps, float trainPosition,
                      float routeLength, const char* gpuName, float fps) {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float left = 285.0f;
  const float right = 340.0f;
  const float top = 54.0f;
  const float bottom = 205.0f;

  ImGui::SetNextWindowPos({viewport->WorkPos.x + left, viewport->WorkPos.y + top});
  ImGui::SetNextWindowSize(
      {std::max(100.0f, viewport->WorkSize.x - left - right),
       std::max(100.0f, viewport->WorkSize.y - top - bottom)});

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

  if (!ImGui::Begin("Scene Viewport", nullptr, flags)) {
    ImGui::End();
    return;
  }

  const ImVec2 windowPos = ImGui::GetWindowPos();
  const ImVec2 windowSize = ImGui::GetWindowSize();
  ImGuiIO& io = ImGui::GetIO();

  ImGui::SetCursorPos({0.0f, 0.0f});
  const ImVec2 inputSize = ImGui::GetContentRegionAvail();
  ImGui::InvisibleButton("##ViewportInput", inputSize,
                         ImGuiButtonFlags_MouseButtonLeft |
                         ImGuiButtonFlags_MouseButtonMiddle |
                         ImGuiButtonFlags_MouseButtonRight);
  const bool hovered = ImGui::IsItemHovered();
  mViewportHovered = hovered;

  if (hovered && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    ImGui::SetWindowFocus();
    const ImVec2 mouse = io.MousePos;

    const entt::entity selected = scene.selected();
    const SceneEntity* selectedNode = scene.get(selected);
    int hitAxis = -1;

    if (selectedNode != nullptr && selectedNode->visible &&
        !selectedNode->locked) {
      const glm::vec3 originWorld = scene.worldPosition(selected);
      const ProjectedPoint origin =
          projectWorldPoint(camera, windowPos, windowSize, originWorld);
      constexpr float axisLength = 2.0f;
      const glm::vec3 axisWorld[] = {
          {axisLength, 0.0f, 0.0f},
          {0.0f, axisLength, 0.0f},
          {0.0f, 0.0f, axisLength}};
      if (origin.visible) {
        float bestDistance = 15.0f;
        for (int axis = 0; axis < 3; ++axis) {
          const ProjectedPoint endpoint =
              projectWorldPoint(camera, windowPos, windowSize,
                                originWorld + axisWorld[axis]);
          if (!endpoint.visible)
            continue;
          const float distance =
              distanceToSegment(mouse, origin.screen, endpoint.screen);
          if (distance < bestDistance) {
            bestDistance = distance;
            hitAxis = axis;
          }
        }
      }
    }

    if (hitAxis >= 0) {
      mGizmoEntity = selected;
      mGizmoAxis = hitAxis;
    } else {
      float nearest = 18.0f;
      entt::entity hitEntity = entt::null;
      for (const auto entity : scene.order()) {
        const SceneEntity* node = scene.get(entity);
        if (node == nullptr || !node->visible ||
            node->type == "Folder" || node->type == "Scene" ||
            node->type == "Simulation")
          continue;
        const ProjectedPoint point =
            projectWorldPoint(camera, windowPos, windowSize,
                              scene.worldPosition(entity));
        if (!point.visible)
          continue;
        const ImVec2 d{mouse.x - point.screen.x,
                       mouse.y - point.screen.y};
        const float distance = std::sqrt(d.x * d.x + d.y * d.y);
        if (distance < nearest) {
          nearest = distance;
          hitEntity = entity;
        }
      }
      if (hitEntity != entt::null)
        scene.select(hitEntity);
    }
  }

  if (mGizmoEntity != entt::null && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    SceneEntity* node = scene.get(mGizmoEntity);
    if (node == nullptr || node->locked) {
      mGizmoEntity = entt::null;
      mGizmoAxis = -1;
    } else if (std::abs(io.MouseDelta.x) > 0.0f ||
               std::abs(io.MouseDelta.y) > 0.0f) {
      const glm::vec3 originWorld = scene.worldPosition(mGizmoEntity);
      const glm::vec3 axes[] = {
          {1.0f, 0.0f, 0.0f},
          {0.0f, 1.0f, 0.0f},
          {0.0f, 0.0f, 1.0f}};
      const glm::vec3 axis = axes[std::clamp(mGizmoAxis, 0, 2)];

      if (gizmoMode == GizmoMode::Translate) {
        const ProjectedPoint origin =
            projectWorldPoint(camera, windowPos, windowSize, originWorld);
        const ProjectedPoint endpoint =
            projectWorldPoint(camera, windowPos, windowSize,
                              originWorld + axis * 2.0f);
        if (origin.visible && endpoint.visible) {
          const ImVec2 screenAxis{endpoint.screen.x - origin.screen.x,
                                  endpoint.screen.y - origin.screen.y};
          const float pixelsPerUnit =
              std::sqrt(screenAxis.x * screenAxis.x +
                        screenAxis.y * screenAxis.y) / 2.0f;
          if (pixelsPerUnit > 0.25f) {
            const ImVec2 delta = io.MouseDelta;
            const float projectedPixels =
                delta.x * screenAxis.x + delta.y * screenAxis.y;
            const float axisScreenLengthSq =
                screenAxis.x * screenAxis.x + screenAxis.y * screenAxis.y;
            const float signedUnits =
                axisScreenLengthSq > 0.001f
                    ? projectedPixels / axisScreenLengthSq * 2.0f
                    : 0.0f;
            scene.translateWorld(mGizmoEntity, axis * signedUnits);
          }
        }
      } else if (gizmoMode == GizmoMode::Rotate) {
        const float sensitivity = 0.45f;
        if (mGizmoAxis == 0)
          node->transform.rotation.x += io.MouseDelta.y * sensitivity;
        else if (mGizmoAxis == 1)
          node->transform.rotation.y += io.MouseDelta.x * sensitivity;
        else
          node->transform.rotation.z += io.MouseDelta.y * sensitivity;
      } else {
        const float amount = (-io.MouseDelta.y + io.MouseDelta.x) * 0.004f;
        const float factor = std::max(0.01f, 1.0f + amount);
        if (mGizmoAxis == 0)
          node->transform.scale.x = std::max(0.05f, node->transform.scale.x * factor);
        else if (mGizmoAxis == 1)
          node->transform.scale.y = std::max(0.05f, node->transform.scale.y * factor);
        else
          node->transform.scale.z = std::max(0.05f, node->transform.scale.z * factor);
      }
    }
  }

  if (mGizmoEntity != entt::null &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    mGizmoEntity = entt::null;
    mGizmoAxis = -1;
  }

  if (hovered && io.MouseWheel != 0.0f && mGizmoEntity == entt::null) {
    camera.position +=
        camera.getFront() * (io.MouseWheel * 2.0f);
  }

  if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    if (io.KeyShift) {
      camera.position -= camera.getRight() * io.MouseDelta.x * 0.02f;
      camera.position += camera.getUp() * io.MouseDelta.y * 0.02f;
    } else {
      camera.yaw += io.MouseDelta.x * 0.25f;
      camera.pitch -= io.MouseDelta.y * 0.25f;
      camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
    }
  }

  if (scene.selected() != entt::null) {
    const auto* selectedNode = scene.get(scene.selected());
    if (selectedNode != nullptr) {
      const glm::vec3 originWorld = scene.worldPosition(selectedNode->id);
      const glm::vec3 axes[] = {
          {1.0f, 0.0f, 0.0f},
          {0.0f, 1.0f, 0.0f},
          {0.0f, 0.0f, 1.0f}};
      const ImU32 colors[] = {
          IM_COL32(235, 55, 50, 255),
          IM_COL32(65, 225, 95, 255),
          IM_COL32(65, 120, 245, 255)};
      const char* labels[] = {"X", "Y", "Z"};
      const float drawLength = 2.0f;
      const auto origin = projectWorldPoint(
          camera, windowPos, windowSize, originWorld);
      if (origin.visible) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddCircleFilled(origin.screen, 5.0f,
                                  IM_COL32(255, 225, 80, 255));
        for (int axis = 0; axis < 3; ++axis) {
          const auto endpoint = projectWorldPoint(
              camera, windowPos, windowSize, originWorld + axes[axis] * drawLength);
          if (!endpoint.visible)
            continue;
          drawList->AddLine(origin.screen, endpoint.screen, colors[axis],
                             axis == mGizmoAxis ? 5.0f : 3.0f);
          drawList->AddCircleFilled(endpoint.screen, 6.0f, colors[axis]);
          drawList->AddText(ImVec2(endpoint.screen.x + 7.0f,
                                   endpoint.screen.y - 7.0f),
                            colors[axis], labels[axis]);
        }
      }
    }
  }

  ImGui::SetCursorPos({16.0f, 12.0f});
  ImGui::BeginGroup();
  ImGui::Text("Scene: M4 World");
  ImGui::Text("Camera | FPS %.1f | %.1f km/h | %.0f / %.0f m",
              fps, trainSpeedMps * 3.6f, trainPosition, routeLength);
  ImGui::TextDisabled("MMB orbit | Shift+MMB pan | Wheel zoom | Click entity");
  ImGui::EndGroup();

  ImGui::SetCursorPos({16.0f, ImGui::GetWindowHeight() - 46.0f});
  ImGui::BeginGroup();
  ImGui::Text("GPU: %s", gpuName != nullptr ? gpuName : "Unknown");
  ImGui::TextDisabled("W Move | E Rotate | R Scale | Drag gizmo | Arrow keys");
  ImGui::EndGroup();

  ImGui::End();
}

void UI::drawGameplayHUD(float trainSpeedMps, float trainPosition,
                         float routeLength, const GameplayHUDData& gameplay) {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoSavedSettings;

  if (!ImGui::Begin("##GameplayHUD", nullptr, flags)) {
    ImGui::End();
    return;
  }

  const ImVec2 size = ImGui::GetWindowSize();
  const float speedKmh = trainSpeedMps * 3.6f;
  const float recommendedKmh = gameplay.recommendedSpeedMps * 3.6f;
  const float progress =
      routeLength > 0.0f ? std::clamp(trainPosition / routeLength, 0.0f, 1.0f)
                         : 0.0f;
  const float doorPercent =
      std::clamp(gameplay.doorOpenFraction, 0.0f, 1.0f) * 100.0f;

  ImGui::SetCursorPos({24.0f, 24.0f});
  ImGui::BeginGroup();
  ImGui::TextColored({0.95f, 0.95f, 1.0f, 1.0f}, "M4  /  DRIVER");
  ImGui::Text("NEXT  %s", gameplay.nextStation);
  if (gameplay.overspeed)
    ImGui::TextColored({1.0f, 0.18f, 0.16f, 1.0f}, "OVERSPEED  %.0f km/h", speedKmh);
  ImGui::TextDisabled("%.0f m  |  advisory %.0f km/h",
                      gameplay.distanceToNextStation, recommendedKmh);

  if (gameplay.stationCount > 0) {
    const float mapWidth = 300.0f;
    const float step = gameplay.stationCount > 1
                           ? mapWidth / static_cast<float>(gameplay.stationCount - 1)
                           : 0.0f;
    ImDrawList* list = ImGui::GetWindowDrawList();
    const ImVec2 mapOrigin = ImGui::GetCursorScreenPos();
    const float lineY = mapOrigin.y + 18.0f;
    list->AddLine(mapOrigin, ImVec2(mapOrigin.x + mapWidth, lineY),
                  IM_COL32(130, 135, 145, 180), 2.0f);
    for (size_t i = 0; i < gameplay.stationCount; ++i) {
      const float x = mapOrigin.x + step * static_cast<float>(i);
      const bool active = i == gameplay.nextStationIndex;
      const float radius = active ? 5.0f : 3.0f;
      list->AddCircleFilled(ImVec2(x, lineY), radius,
                            active ? IM_COL32(255, 210, 70, 255)
                                   : IM_COL32(180, 185, 195, 220));
    }
    ImGui::Dummy({mapWidth, 30.0f});
  }
  ImGui::EndGroup();

  ImGui::SetCursorPos({size.x - 250.0f, 24.0f});
  ImGui::BeginGroup();
  ImGui::Text("SIGNAL");
  ImGui::TextColored(
      std::string(gameplay.signalAspect) == "STOP"
          ? ImVec4(0.95f, 0.2f, 0.2f, 1.0f)
          : (std::string(gameplay.signalAspect) == "CAUTION"
                 ? ImVec4(1.0f, 0.78f, 0.18f, 1.0f)
                 : ImVec4(0.25f, 0.95f, 0.4f, 1.0f)),
      "%s", gameplay.signalAspect);
  ImGui::Text("DOOR  %s  %3.0f%%",
              gameplay.doorsOpen ? "OPEN" : "CLOSED", doorPercent);
  ImGui::EndGroup();

  ImGui::SetCursorPos({24.0f, size.y - 142.0f});
  ImGui::BeginGroup();
  if (gameplay.emergencyBrakeActive)
    ImGui::TextColored({1.0f, 0.18f, 0.16f, 1.0f}, "EMERGENCY BRAKE");
  else if (gameplay.serviceBrakeActive)
    ImGui::TextColored({1.0f, 0.72f, 0.18f, 1.0f}, "SERVICE BRAKE");
  else if (gameplay.tractionActive)
    ImGui::TextColored({0.25f, 0.85f, 0.45f, 1.0f}, "TRACTION");
  else
    ImGui::TextDisabled("COAST");
  ImGui::Text("ACCEL  %+0.2f m/s²", gameplay.accelerationMps2);
  ImGui::Text("PASSENGERS  %zu / %zu", gameplay.onboardPassengers,
              gameplay.onboardPassengers + gameplay.waitingPassengers);
  if (gameplay.dwellLimitSeconds > 0.0f) {
    const float dwell =
        std::clamp(gameplay.dwellSeconds / gameplay.dwellLimitSeconds, 0.0f, 1.0f);
    ImGui::ProgressBar(dwell, {220.0f, 12.0f}, "DWELL");
  }
  ImGui::TextDisabled("UP throttle | DOWN brake | SPACE emergency | O/C doors | H horn | F6 restart");
  ImGui::EndGroup();

  ImGui::SetCursorPos({size.x - 290.0f, size.y - 174.0f});
  ImGui::BeginGroup();
  ImGui::TextDisabled("SPEED");
  ImGui::Text("%.0f", speedKmh);
  ImGui::SameLine();
  ImGui::TextDisabled("km/h");
  char progressLabel[32];
  std::snprintf(progressLabel, sizeof(progressLabel), "M4  %3.0f%%", progress * 100.0f);
  ImGui::ProgressBar(progress, {260.0f, 16.0f}, progressLabel);
  ImGui::EndGroup();

  ImGui::SetCursorPos({size.x * 0.5f - 150.0f, 18.0f});
  ImGui::BeginGroup();
  ImGui::Text("KADIKÖY  →  SABİHA GÖKÇEN");
  ImGui::TextDisabled("F1 CAB   F2 CHASE   F3 FREE   F4 EDITOR   F5 PLAY");
  ImGui::EndGroup();

  ImGui::End();
}


} // namespace metro::editor
