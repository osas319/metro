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

} // namespace

void UI::draw(Scene& scene, bool& editorMode, bool& playMode,
              float trainSpeedMps, float trainPosition, float routeLength,
              const char* gpuName, float fps) {
  if (!editorMode)
    return;

  ImGuiIO& io = ImGui::GetIO();
  (void)io;

  ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(),
                               ImGuiDockNodeFlags_PassthruCentralNode);

  drawToolbar(scene, editorMode, playMode);
  drawHierarchy(scene);
  drawInspector(scene);
  drawContentBrowser(scene);
  drawViewport(trainSpeedMps, trainPosition, routeLength, gpuName, fps);
}

void UI::drawToolbar(Scene& scene, bool& editorMode, bool& playMode) {
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
  if (ImGui::Button(" +  ENTITY", {92.0f, 32.0f})) {
    scene.create("New Entity", "Empty", creationParent(scene));
    mStatus = "Created entity";
  }

  ImGui::SameLine();
  if (ImGui::Button("DUPLICATE", {96.0f, 32.0f}))
    scene.duplicate(scene.selected());

  ImGui::SameLine();
  if (ImGui::Button("DELETE", {76.0f, 32.0f}))
    scene.destroy(scene.selected());

  ImGui::SameLine();
  if (ImGui::Button("SAVE", {72.0f, 32.0f}))
    scene.save(mScenePath);

  ImGui::SameLine();
  if (ImGui::Button("LOAD", {72.0f, 32.0f}))
    scene.load(mScenePath);

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

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
  ImGui::SetNextWindowSize(
      {viewport->WorkSize.x, height});

  if (!ImGui::Begin("Content Browser", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::End();
    return;
  }

  ImGui::TextUnformatted("Assets");
  ImGui::SameLine();
  ImGui::TextDisabled("> stations > kadikoy");

  ImGui::Separator();

  const std::filesystem::path root("assets");
  if (!std::filesystem::exists(root)) {
    ImGui::TextDisabled("assets/ not found");
    ImGui::End();
    return;
  }

  int shown = 0;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (ec || shown >= 14)
      break;
    const std::string label = entry.path().filename().string();
    if (entry.is_directory(ec))
      ImGui::Selectable(("[DIR] " + label).c_str());
    else
      ImGui::Selectable(label.c_str());
    ++shown;
  }

  ImGui::End();
}

void UI::drawViewport(float trainSpeedMps, float trainPosition,
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
      ImGuiWindowFlags_NoInputs |
      ImGuiWindowFlags_NoSavedSettings;

  if (!ImGui::Begin("Scene Viewport", nullptr, flags)) {
    ImGui::End();
    return;
  }

  ImGui::SetCursorPos({16.0f, 12.0f});
  ImGui::BeginGroup();
  ImGui::Text("Scene: M4 World");
  ImGui::Text("Camera | FPS %.1f | %.1f km/h | %.0f / %.0f m",
              fps, trainSpeedMps * 3.6f, trainPosition, routeLength);
  ImGui::TextDisabled("WASD camera | Mouse orbit | F1 Cab  F2 Chase  F3 Free");
  ImGui::EndGroup();

  ImGui::SetCursorPos({16.0f, ImGui::GetWindowHeight() - 46.0f});
  ImGui::BeginGroup();
  ImGui::Text("GPU: %s", gpuName != nullptr ? gpuName : "Unknown");
  ImGui::TextDisabled("Hierarchy transforms active | Select an entity to edit");
  ImGui::EndGroup();

  ImGui::End();
}

} // namespace metro::editor
