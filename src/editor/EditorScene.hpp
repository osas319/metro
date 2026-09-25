#pragma once

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace metro::editor {

struct Transform {
  glm::vec3 position{0.0f};
  glm::vec3 rotation{0.0f};
  glm::vec3 scale{1.0f};
};

struct SceneEntity {
  entt::entity id{entt::null};
  entt::entity parent{entt::null};
  std::string name{"Entity"};
  std::string type{"Empty"};
  std::string asset;
  Transform transform{};
  bool visible = true;
  bool locked = false;
};

class Scene {
public:
  Scene();

  void clear();
  void createDefaultScene();

  entt::entity create(std::string name, std::string type = "Empty",
                      entt::entity parent = entt::null);
  bool destroy(entt::entity entity);
  entt::entity duplicate(entt::entity entity);

  SceneEntity* get(entt::entity entity);
  const SceneEntity* get(entt::entity entity) const;

  // Yerel Transform'dan dünya matrisini hesaplar. Parent zinciri uygulanır.
  glm::mat4 worldTransform(entt::entity entity) const;
  glm::vec3 worldPosition(entt::entity entity) const;

  const std::vector<entt::entity>& order() const { return mOrder; }
  entt::entity selected() const { return mSelected; }
  void select(entt::entity entity) { mSelected = entity; }

  bool save(const std::string& path) const;
  bool load(const std::string& path);

  size_t size() const { return mOrder.size(); }
  size_t visibleCount() const;

private:
  glm::mat4 localTransform(const SceneEntity& node) const;

  entt::registry mRegistry;
  std::vector<entt::entity> mOrder;
  entt::entity mSelected{entt::null};
};

} // namespace metro::editor
