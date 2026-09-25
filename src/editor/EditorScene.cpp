#include "editor/EditorScene.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace metro::editor {

Scene::Scene() {
  createDefaultScene();
}

void Scene::clear() {
  mRegistry.clear();
  mOrder.clear();
  mSelected = entt::null;
}

entt::entity Scene::create(std::string name, std::string type, entt::entity parent) {
  if (parent != entt::null && get(parent) == nullptr)
    parent = entt::null;

  const entt::entity entity = mRegistry.create();
  auto& node = mRegistry.emplace<SceneEntity>(entity);
  node.id = entity;
  node.parent = parent;
  node.name = std::move(name);
  node.type = std::move(type);
  mOrder.push_back(entity);
  if (mSelected == entt::null)
    mSelected = entity;
  return entity;
}

bool Scene::destroy(entt::entity entity) {
  if (entity == entt::null || !mRegistry.valid(entity))
    return false;

  for (auto child : mOrder) {
    if (auto* node = get(child); node != nullptr && node->parent == entity)
      node->parent = entt::null;
  }

  mRegistry.destroy(entity);
  mOrder.erase(std::remove(mOrder.begin(), mOrder.end(), entity), mOrder.end());
  if (mSelected == entity)
    mSelected = mOrder.empty() ? entt::null : mOrder.front();
  return true;
}

entt::entity Scene::duplicate(entt::entity entity) {
  const auto* source = get(entity);
  if (source == nullptr)
    return entt::null;

  const entt::entity copy = create(source->name + " Copy", source->type, source->parent);
  auto* dst = get(copy);
  if (dst != nullptr) {
    dst->asset = source->asset;
    dst->transform = source->transform;
    dst->transform.position.x += 0.5f;
    dst->transform.position.z += 0.5f;
    dst->visible = source->visible;
    dst->locked = source->locked;
  }
  mSelected = copy;
  return copy;
}

SceneEntity* Scene::get(entt::entity entity) {
  if (!mRegistry.valid(entity))
    return nullptr;
  return mRegistry.try_get<SceneEntity>(entity);
}

const SceneEntity* Scene::get(entt::entity entity) const {
  if (!mRegistry.valid(entity))
    return nullptr;
  return mRegistry.try_get<SceneEntity>(entity);
}

glm::mat4 Scene::localTransform(const SceneEntity& node) const {
  glm::mat4 transform(1.0f);
  transform = glm::translate(transform, node.transform.position);
  transform = glm::rotate(transform, glm::radians(node.transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
  transform = glm::rotate(transform, glm::radians(node.transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
  transform = glm::rotate(transform, glm::radians(node.transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
  transform = glm::scale(transform, node.transform.scale);
  return transform;
}

glm::mat4 Scene::worldTransform(entt::entity entity) const {
  if (get(entity) == nullptr)
    return glm::mat4(1.0f);

  std::vector<entt::entity> chain;
  std::unordered_set<uint32_t> visited;

  entt::entity current = entity;
  while (current != entt::null) {
    const auto* node = get(current);
    if (node == nullptr)
      break;

    const uint32_t id = entt::to_integral(current);
    if (!visited.insert(id).second)
      break; // Hatalı parent döngüsü varsa sonsuza gitme.

    chain.push_back(current);
    current = node->parent;
  }

  glm::mat4 result(1.0f);
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    const auto* node = get(*it);
    if (node != nullptr)
      result *= localTransform(*node);
  }
  return result;
}

glm::vec3 Scene::worldPosition(entt::entity entity) const {
  const glm::vec4 position = worldTransform(entity) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  return glm::vec3(position);
}

bool Scene::translateWorld(entt::entity entity, const glm::vec3& delta) {
  SceneEntity* node = get(entity);
  if (node == nullptr || node->locked)
    return false;

  const glm::mat4 parentWorld =
      node->parent == entt::null ? glm::mat4(1.0f) : worldTransform(node->parent);
  const glm::vec4 localDelta = glm::inverse(parentWorld) * glm::vec4(delta, 0.0f);
  node->transform.position += glm::vec3(localDelta);
  return true;
}

bool Scene::isDescendant(entt::entity entity, entt::entity possibleParent) const {
  if (entity == entt::null || possibleParent == entt::null)
    return false;

  std::unordered_set<uint32_t> visited;
  entt::entity current = possibleParent;
  while (current != entt::null) {
    const auto* node = get(current);
    if (node == nullptr)
      return false;

    if (current == entity)
      return true;

    if (!visited.insert(entt::to_integral(current)).second)
      return true;
    current = node->parent;
  }
  return false;
}

bool Scene::setParent(entt::entity entity, entt::entity parent, bool keepWorldTransform) {
  SceneEntity* node = get(entity);
  if (node == nullptr)
    return false;

  if (parent != entt::null && get(parent) == nullptr)
    return false;
  if (parent == entity || isDescendant(entity, parent))
    return false;

  const glm::mat4 previousWorld = keepWorldTransform ? worldTransform(entity) : glm::mat4(1.0f);
  node->parent = parent;

  if (!keepWorldTransform)
    return true;

  const glm::mat4 parentWorld =
      parent == entt::null ? glm::mat4(1.0f) : worldTransform(parent);
  const glm::mat4 local = glm::inverse(parentWorld) * previousWorld;

  const glm::vec3 translation = glm::vec3(local[3]);
  glm::vec3 scale(
      glm::length(glm::vec3(local[0])),
      glm::length(glm::vec3(local[1])),
      glm::length(glm::vec3(local[2])));
  if (scale.x <= 0.00001f || scale.y <= 0.00001f || scale.z <= 0.00001f)
    return false;

  glm::mat3 rotationMatrix(
      glm::vec3(local[0]) / scale.x,
      glm::vec3(local[1]) / scale.y,
      glm::vec3(local[2]) / scale.z);
  const glm::quat orientation = glm::normalize(glm::quat_cast(rotationMatrix));

  node->transform.position = translation;
  node->transform.scale = scale;
  node->transform.rotation = glm::degrees(glm::eulerAngles(orientation));
  return true;
}

size_t Scene::visibleCount() const {
  size_t count = 0;
  for (auto entity : mOrder) {
    const auto* node = get(entity);
    if (node != nullptr && node->visible)
      ++count;
  }
  return count;
}

void Scene::createDefaultScene() {
  clear();

  const auto world = create("M4 World", "Scene");
  const auto route = create("M4 Route", "Folder", world);

  struct DefaultStop {
    const char* name;
    float position;
  };

  // Durak konumları station manifest ile aynı veri setinden gelir; bunlar
  // editörde sahne yerleşimini başlangıçta görünür kılmak için tutulur.
  constexpr DefaultStop stops[] = {
    {"Kadikoy", 0.0f},
    {"Ayrilik Cesmesi", 1148.0f},
    {"Acibadem", 2435.0f},
    {"Unalan", 3988.25f},
    {"Goztepe", 5541.5f},
    {"Yenisahra", 7094.75f},
    {"Kozyatagi", 8648.0f},
    {"Bostanci", 10201.25f},
    {"Kucukyali", 11754.5f},
    {"Maltepe", 13307.75f},
    {"Huzurevi", 14861.0f},
    {"Gulsuyu", 16414.25f},
    {"Esenkent", 17967.5f},
    {"Hastane-Adliye", 19520.75f},
    {"Soganlik", 21074.0f},
    {"Kartal", 22627.25f},
    {"Yakacik-Adnan Kahveci", 24180.5f},
    {"Pendik", 25733.75f},
    {"Tavsantepe", 27287.0f},
    {"Fevzi Cakmak-Hastane", 28840.25f},
    {"Yayalar-Sehitler", 30393.5f},
    {"Kurtkoy", 31946.75f},
    {"Sabiha Gokcen Havalimani", 33500.0f}
  };

  for (size_t i = 0; i < std::size(stops); ++i) {
    const auto station = create(std::string(stops[i].name) + " Station", "Station", route);
    if (auto* node = get(station)) {
      node->asset = "Station/" + std::to_string(i);
      node->transform.position = {0.0f, 0.0f, -stops[i].position};
    }
  }

  create("Track", "Rail", route);
  create("Catenary", "Infrastructure", route);

  const auto rollingStock = create("Rolling Stock", "Folder", world);
  const auto train = create("M4 CAF Train", "Train", rollingStock);
  if (auto* node = get(train)) {
    node->asset = "Builtin/TrainCar";
    node->transform.position = {0.0f, 0.0f, 0.0f};
  }

  create("Passenger System", "Simulation", world);
  create("Signal System", "Simulation", world);
  mSelected = train;
}

bool Scene::save(const std::string& path) const {
  std::ofstream file(path);
  if (!file)
    return false;

  file << "METRO_SCENE 1\n";
  for (auto entity : mOrder) {
    const auto* node = get(entity);
    if (node == nullptr)
      continue;

    const uint32_t id = entt::to_integral(entity) + 1u;
    const uint32_t parent = node->parent == entt::null
                                ? 0u
                                : entt::to_integral(node->parent) + 1u;
    file << "ENTITY " << id << ' ' << parent << ' '
         << std::quoted(node->name) << ' '
         << std::quoted(node->type) << ' '
         << std::quoted(node->asset) << ' '
         << node->visible << ' ' << node->locked << '\n';
    file << "TRANSFORM "
         << node->transform.position.x << ' '
         << node->transform.position.y << ' '
         << node->transform.position.z << ' '
         << node->transform.rotation.x << ' '
         << node->transform.rotation.y << ' '
         << node->transform.rotation.z << ' '
         << node->transform.scale.x << ' '
         << node->transform.scale.y << ' '
         << node->transform.scale.z << '\n';
  }
  return true;
}

bool Scene::load(const std::string& path) {
  std::ifstream file(path);
  if (!file)
    return false;

  std::string magic;
  int version = 0;
  file >> magic >> version;
  if (magic != "METRO_SCENE" || version != 1)
    return false;

  struct Pending {
    uint32_t oldId = 0;
    uint32_t oldParent = 0;
    std::string name;
    std::string type;
    std::string asset;
    bool visible = true;
    bool locked = false;
    Transform transform{};
  };

  std::vector<Pending> pending;
  std::string tag;
  while (file >> tag) {
    if (tag == "ENTITY") {
      Pending p;
      file >> p.oldId >> p.oldParent
           >> std::quoted(p.name) >> std::quoted(p.type)
           >> std::quoted(p.asset) >> p.visible >> p.locked;
      pending.push_back(std::move(p));
    } else if (tag == "TRANSFORM" && !pending.empty()) {
      auto& p = pending.back();
      file >> p.transform.position.x >> p.transform.position.y >> p.transform.position.z
           >> p.transform.rotation.x >> p.transform.rotation.y >> p.transform.rotation.z
           >> p.transform.scale.x >> p.transform.scale.y >> p.transform.scale.z;
    }
  }

  clear();

  std::unordered_map<uint32_t, entt::entity> remap;
  for (const auto& p : pending)
    remap[p.oldId] = create(p.name, p.type);

  for (const auto& p : pending) {
    const auto entityIt = remap.find(p.oldId);
    if (entityIt == remap.end())
      continue;

    auto* node = get(entityIt->second);
    if (node == nullptr)
      continue;

    node->asset = p.asset;
    node->visible = p.visible;
    node->locked = p.locked;
    node->transform = p.transform;

    if (p.oldParent != 0) {
      const auto parentIt = remap.find(p.oldParent);
      if (parentIt != remap.end())
        setParent(entityIt->second, parentIt->second, false);
    }
  }

  mSelected = mOrder.empty() ? entt::null : mOrder.front();
  return true;
}

} // namespace metro::editor
