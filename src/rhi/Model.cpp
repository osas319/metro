#include "rhi/Model.hpp"
#include "rhi/Renderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "core/Log.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <utility>

namespace metro::rhi {

namespace {

void createBuffer(VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memUsage, VkBuffer& buffer, VmaAllocation& allocation) {
  VkBufferCreateInfo bufInfo{};
  bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufInfo.size = size;
  bufInfo.usage = usage;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = memUsage;

  if (vmaCreateBuffer(ctx.allocator(), &bufInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("vmaCreateBuffer failed");
  }
}

void copyBuffer(VulkanContext& ctx, VkCommandPool pool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = pool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(ctx.device(), &allocInfo, &cmd);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &beginInfo);

  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = size;
  vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(ctx.graphicsQueue());
  vkFreeCommandBuffers(ctx.device(), pool, 1, &cmd);
}


struct BuiltinPart {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
  float metallic = 0.0f;
  float roughness = 0.7f;
  float materialId = 0.0f;
};

void addFace(BuiltinPart& part, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
  const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
  const uint32_t base = static_cast<uint32_t>(part.vertices.size());
  for (const glm::vec3 p : {a, b, c, d}) {
    Vertex v{};
    v.pos[0] = p.x; v.pos[1] = p.y; v.pos[2] = p.z;
    v.normal[0] = normal.x; v.normal[1] = normal.y; v.normal[2] = normal.z;
    v.texCoord[0] = p.x; v.texCoord[1] = p.z;
    part.vertices.push_back(v);
  }
  part.indices.insert(part.indices.end(), {
      base, base + 1, base + 2, base, base + 2, base + 3
  });
}

void addBox(BuiltinPart& part, glm::vec3 center, glm::vec3 size) {
  const glm::vec3 h = size * 0.5f;
  const float x0 = center.x - h.x, x1 = center.x + h.x;
  const float y0 = center.y - h.y, y1 = center.y + h.y;
  const float z0 = center.z - h.z, z1 = center.z + h.z;

  addFace(part, {x1,y0,z0}, {x1,y1,z0}, {x1,y1,z1}, {x1,y0,z1});
  addFace(part, {x0,y0,z1}, {x0,y1,z1}, {x0,y1,z0}, {x0,y0,z0});
  addFace(part, {x0,y1,z0}, {x0,y1,z1}, {x1,y1,z1}, {x1,y1,z0});
  addFace(part, {x0,y0,z1}, {x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1});
  addFace(part, {x0,y0,z0}, {x0,y1,z0}, {x1,y1,z0}, {x1,y0,z0});
  addFace(part, {x1,y0,z1}, {x1,y1,z1}, {x0,y1,z1}, {x0,y0,z1});
}

void addCylinder(BuiltinPart& part, glm::vec3 center, float radius,
                 float depth, int segments, bool axisX) {
  segments = std::max(8, segments);
  const uint32_t base = static_cast<uint32_t>(part.vertices.size());
  const float half = depth * 0.5f;
  for (int i = 0; i < segments; ++i) {
    const float a = 2.0f * 3.14159265359f *
                    static_cast<float>(i) / static_cast<float>(segments);
    const float c = std::cos(a);
    const float s = std::sin(a);
    for (float side : {-1.0f, 1.0f}) {
      Vertex v{};
      if (axisX) {
        v.pos[0] = center.x + side * half;
        v.pos[1] = center.y + radius * c;
        v.pos[2] = center.z + radius * s;
        v.normal[0] = side;
        v.normal[1] = c;
        v.normal[2] = s;
      } else {
        v.pos[0] = center.x + radius * c;
        v.pos[1] = center.y + radius * s;
        v.pos[2] = center.z + side * half;
        v.normal[0] = c;
        v.normal[1] = s;
        v.normal[2] = side;
      }
      v.texCoord[0] = static_cast<float>(i) / static_cast<float>(segments);
      v.texCoord[1] = side * 0.5f + 0.5f;
      part.vertices.push_back(v);
    }
  }
  for (int i = 0; i < segments; ++i) {
    const uint32_t a = base + static_cast<uint32_t>(i * 2);
    const uint32_t b = base + static_cast<uint32_t>(((i + 1) % segments) * 2);
    part.indices.insert(part.indices.end(), {a, b, a + 1, b, b + 1, a + 1});
  }
}

void addTunnelCeiling(BuiltinPart& part, float innerRadius,
                     float springY, float depth, int segments) {
  segments = std::max(16, segments);
  const uint32_t base = static_cast<uint32_t>(part.vertices.size());
  const float halfDepth = depth * 0.5f;
  constexpr float pi = 3.14159265359f;

  // Gerçek tünel boşluğu için düz tavan yerine yarım daire kesitli iç kabuk.
  // Kesit X-Y düzleminde, Z hattı boyunca ekstrüde edilir.
  for (int i = 0; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float angle = t * pi;
    const float c = std::cos(angle);
    const float sn = std::sin(angle);
    const glm::vec3 inwardNormal{-c, -sn, 0.0f};

    for (float z : {-halfDepth, halfDepth}) {
      Vertex v{};
      v.pos[0] = innerRadius * c;
      v.pos[1] = springY + innerRadius * sn;
      v.pos[2] = z;
      v.normal[0] = inwardNormal.x;
      v.normal[1] = inwardNormal.y;
      v.normal[2] = inwardNormal.z;
      v.texCoord[0] = t * (innerRadius * pi);
      v.texCoord[1] = (z + halfDepth) / std::max(depth, 0.001f);
      part.vertices.push_back(v);
    }
  }

  for (int i = 0; i < segments; ++i) {
    const uint32_t a = base + static_cast<uint32_t>(i * 2);
    const uint32_t b = a + 2;
    const uint32_t aFar = a + 1;
    const uint32_t bFar = b + 1;
    part.indices.insert(part.indices.end(), {a, aFar, bFar, a, bFar, b});
  }
}

void addFrustum(BuiltinPart& part, float z0, float z1,
                float xHalf0, float xHalf1, float yBottom, float yTop) {
  const glm::vec3 p000{-xHalf0, yBottom, z0};
  const glm::vec3 p100{ xHalf0, yBottom, z0};
  const glm::vec3 p110{ xHalf0, yTop, z0};
  const glm::vec3 p010{-xHalf0, yTop, z0};
  const glm::vec3 p001{-xHalf1, yBottom, z1};
  const glm::vec3 p101{ xHalf1, yBottom, z1};
  const glm::vec3 p111{ xHalf1, yTop, z1};
  const glm::vec3 p011{-xHalf1, yTop, z1};

  addFace(part, p000, p001, p011, p010);
  addFace(part, p101, p100, p110, p111);
  addFace(part, p010, p011, p111, p110);
  addFace(part, p001, p000, p100, p101);
  addFace(part, p000, p010, p110, p100);
  addFace(part, p101, p111, p011, p001);
}

std::vector<BuiltinPart> makeTrainCarParts() {
  std::vector<BuiltinPart> parts;

  // M4 CAF araci: yaklasik 22.43 m uzunluk / 3.009 m genislik.
  // Geometri, gercek olcekli 4'lu set yerlesimine gore uzatildi.
  constexpr float halfWidth = 1.505f;
  constexpr float bodyLength = 22.43f;

  BuiltinPart chassis;
  chassis.color = {0.055f, 0.065f, 0.078f, 1.0f};
  chassis.roughness = 0.72f;
  chassis.materialId = 3.0f;
  addBox(chassis, {0.0f, -0.62f, 0.0f}, {2.72f, 0.74f, bodyLength - 0.55f});
  addBox(chassis, {0.0f, -0.20f, -9.45f}, {2.82f, 0.32f, 0.42f});
  addBox(chassis, {0.0f, -0.20f, 9.45f}, {2.82f, 0.32f, 0.42f});
  parts.push_back(std::move(chassis));

  BuiltinPart lower;
  lower.color = {0.035f, 0.22f, 0.44f, 1.0f};
  lower.roughness = 0.40f;
  lower.materialId = 11.0f;
  addBox(lower, {0.0f, 0.00f, 0.0f}, {2.92f, 0.70f, bodyLength - 0.95f});
  addBox(lower, {0.0f, 0.34f, 0.0f}, {2.96f, 0.16f, bodyLength - 1.35f});
  parts.push_back(std::move(lower));

  // Acik renkli yan kabuk + egimli burun.
  BuiltinPart upper;
  upper.color = {0.84f, 0.85f, 0.84f, 1.0f};
  upper.metallic = 0.05f;
  upper.roughness = 0.28f;
  upper.materialId = 11.0f;
  addBox(upper, {0.0f, 0.84f, 0.0f}, {2.98f, 1.12f, 20.15f});
  addFrustum(upper, -11.18f, -9.35f, 1.02f, 1.48f, 0.18f, 1.92f);
  addFrustum(upper, 9.35f, 11.18f, 1.48f, 1.02f, 0.18f, 1.92f);
  parts.push_back(std::move(upper));

  // Mavi M4 guzergh bandi ve yan panel kirilimi.
  BuiltinPart stripe;
  stripe.color = {0.035f, 0.28f, 0.58f, 1.0f};
  stripe.roughness = 0.34f;
  stripe.materialId = 7.0f;
  addBox(stripe, {0.0f, 0.40f, 0.0f}, {3.00f, 0.16f, 18.9f});
  parts.push_back(std::move(stripe));

  BuiltinPart windows;
  windows.color = {0.010f, 0.028f, 0.052f, 1.0f};
  windows.roughness = 0.085f;
  windows.materialId = 4.0f;
  for (float side : {-1.0f, 1.0f}) {
    addBox(windows, {side * (halfWidth - 0.055f), 1.02f, 0.0f},
           {0.045f, 0.57f, 17.7f});
    // Pencere arasi ince dikmeler.
    for (float z : {-7.5f, -3.75f, 0.0f, 3.75f, 7.5f})
      addBox(windows, {side * (halfWidth - 0.065f), 1.02f, z},
             {0.055f, 0.61f, 0.055f});
  }
  for (float z : {-10.95f, 10.95f})
    addBox(windows, {0.0f, 1.18f, z},
           {1.18f, 0.62f, 0.05f});
  parts.push_back(std::move(windows));

  BuiltinPart doors;
  doors.color = {0.60f, 0.63f, 0.66f, 1.0f};
  doors.metallic = 0.18f;
  doors.roughness = 0.24f;
  doors.materialId = 10.0f;
  for (float side : {-1.0f, 1.0f}) {
    for (float z : {-6.7f, 0.0f, 6.7f}) {
      addBox(doors, {side * (halfWidth + 0.012f), 0.70f, z},
             {0.055f, 0.93f, 1.34f});
      addBox(doors, {side * (halfWidth + 0.018f), 0.70f, z - 0.68f},
             {0.065f, 0.035f, 0.035f});
      addBox(doors, {side * (halfWidth + 0.018f), 0.70f, z + 0.68f},
             {0.065f, 0.035f, 0.035f});
    }
  }
  parts.push_back(std::move(doors));

  BuiltinPart endCap;
  endCap.color = {0.045f, 0.25f, 0.52f, 1.0f};
  endCap.metallic = 0.07f;
  endCap.roughness = 0.22f;
  endCap.materialId = 7.0f;
  for (float z : {-11.18f, 11.18f}) {
    addBox(endCap, {0.0f, 0.46f, z}, {2.10f, 0.72f, 0.11f});
    addBox(endCap, {0.0f, 1.05f, z}, {1.48f, 0.46f, 0.07f});
  }
  parts.push_back(std::move(endCap));

  BuiltinPart roof;
  roof.color = {0.34f, 0.37f, 0.40f, 1.0f};
  roof.metallic = 0.35f;
  roof.roughness = 0.30f;
  roof.materialId = 10.0f;
  addBox(roof, {0.0f, 1.67f, 0.0f}, {2.62f, 0.25f, bodyLength - 0.20f});
  addBox(roof, {0.0f, 1.83f, -3.4f}, {0.56f, 0.24f, 4.4f});
  addBox(roof, {0.0f, 1.83f, 3.6f}, {0.40f, 0.20f, 1.80f});
  parts.push_back(std::move(roof));

  BuiltinPart underfloor;
  underfloor.color = {0.075f, 0.085f, 0.10f, 1.0f};
  underfloor.roughness = 0.78f;
  underfloor.materialId = 3.0f;
  for (float z : {-7.2f, -2.4f, 2.4f, 7.2f})
    addBox(underfloor, {0.0f, -0.92f, z}, {1.95f, 0.26f, 1.35f});
  parts.push_back(std::move(underfloor));

  BuiltinPart bogies;
  bogies.color = {0.055f, 0.062f, 0.075f, 1.0f};
  bogies.roughness = 0.80f;
  bogies.materialId = 3.0f;
  for (float z : {-7.05f, 7.05f}) {
    addBox(bogies, {0.0f, -1.06f, z}, {1.95f, 0.32f, 1.65f});
    addBox(bogies, {-0.62f, -1.25f, z}, {0.36f, 0.20f, 1.08f});
    addBox(bogies, {0.62f, -1.25f, z}, {0.36f, 0.20f, 1.08f});
    // Gidon bogie altinda dondurme ekseni boyunca gorunur tekerlek ciftleri.
    for (float wheelZ : {z - 0.48f, z + 0.48f}) {
      addCylinder(bogies, {-0.82f, -1.30f, wheelZ}, 0.34f, 0.16f, 16, true);
      addCylinder(bogies, { 0.82f, -1.30f, wheelZ}, 0.34f, 0.16f, 16, true);
    }
  }
  parts.push_back(std::move(bogies));

  BuiltinPart lights;
  lights.color = {0.92f, 0.95f, 1.0f, 1.0f};
  lights.roughness = 0.12f;
  lights.materialId = 5.0f;
  for (float side : {-0.72f, 0.72f}) {
    addBox(lights, {side, 0.72f, -11.27f}, {0.22f, 0.22f, 0.08f});
    addBox(lights, {side, 0.72f, 11.27f}, {0.22f, 0.22f, 0.08f});
  }
  parts.push_back(std::move(lights));

  BuiltinPart couplers;
  couplers.color = {0.08f, 0.09f, 0.11f, 1.0f};
  couplers.roughness = 0.70f;
  couplers.materialId = 3.0f;
  for (float z : {-11.52f, 11.52f}) {
    addBox(couplers, {0.0f, -0.35f, z}, {0.46f, 0.36f, 0.30f});
    addBox(couplers, {0.0f, 0.12f, z}, {0.24f, 0.28f, 0.44f});
  }
  parts.push_back(std::move(couplers));

  return parts;
}


std::vector<BuiltinPart> makeTunnelModuleParts() {
  std::vector<BuiltinPart> parts;

  BuiltinPart wall;
  wall.color = {0.12f, 0.14f, 0.17f, 1.0f};
  wall.roughness = 0.96f;
  wall.materialId = 12.0f;
  addBox(wall, {-3.36f, -0.28f, 0.0f}, {0.30f, 0.95f, 30.0f});
  addBox(wall, { 3.36f, -0.28f, 0.0f}, {0.30f, 0.95f, 30.0f});
  parts.push_back(std::move(wall));

  BuiltinPart arch;
  arch.color = {0.10f, 0.115f, 0.14f, 1.0f};
  arch.roughness = 0.96f;
  arch.materialId = 12.0f;

  // Daha küçük bore: yarıçap ~3.35 m; ray çevresinde daha sıkı gerçek tünel hissi.
  addTunnelCeiling(arch, 3.35f, -0.18f, 30.0f, 40);
  parts.push_back(std::move(arch));

  // Tünelin yürüyüş/bakım yolları dar ve alçak; geniş platform gibi görünmez.
  BuiltinPart walkway;
  walkway.color = {0.18f, 0.20f, 0.23f, 1.0f};
  walkway.roughness = 0.92f;
  walkway.materialId = 6.0f;
  addBox(walkway, {-2.58f, -1.02f, 0.0f}, {0.42f, 0.30f, 30.0f});
  addBox(walkway, {2.58f, -1.02f, 0.0f}, {0.42f, 0.30f, 30.0f});

  // Sürekli beton zemin + ray yatağı omuzları: tünel tabanında açık boşluk bırakma.
  BuiltinPart floor;
  floor.color = {0.055f, 0.062f, 0.075f, 1.0f};
  floor.roughness = 0.98f;
  floor.materialId = 6.0f;
  addBox(floor, {0.0f, -1.48f, 0.0f}, {7.75f, 0.34f, 30.0f});
  addBox(floor, {-3.55f, -1.28f, 0.0f}, {0.35f, 0.22f, 30.0f});
  addBox(floor, { 3.55f, -1.28f, 0.0f}, {0.35f, 0.22f, 30.0f});
  parts.push_back(std::move(walkway));
  parts.push_back(std::move(floor));

  BuiltinPart cable;
  cable.color = {0.08f, 0.09f, 0.11f, 1.0f};
  cable.roughness = 0.66f;
  for (float side : {-1.0f, 1.0f})
    addBox(cable, {side * 3.75f, 2.45f, 0.0f}, {0.12f, 0.16f, 30.0f});
  parts.push_back(std::move(cable));

  BuiltinPart contact;
  contact.color = {0.55f, 0.57f, 0.59f, 1.0f};
  contact.metallic = 0.55f;
  contact.roughness = 0.24f;
  addBox(contact, {0.0f, 3.38f, 0.0f}, {0.028f, 0.028f, 30.0f});
  parts.push_back(std::move(contact));

  BuiltinPart lights;
  lights.color = {0.72f, 0.80f, 0.92f, 1.0f};
  lights.roughness = 0.18f;
  lights.materialId = 5.0f;
  for (float z = -12.0f; z <= 12.0f; z += 6.0f)
    addBox(lights, {0.0f, 3.68f, z}, {0.22f, 0.045f, 1.80f});
  parts.push_back(std::move(lights));

  return parts;
}

std::vector<BuiltinPart> makeStationModuleParts() {
  std::vector<BuiltinPart> parts;
  constexpr float moduleLength = 30.0f;

  // Yalnızca SOL tarafta yolcu platformu; sağ taraf tamamen ray/tünel alanıdır.
  constexpr float platformSide = -1.0f;
  BuiltinPart platform;
  platform.color = {0.43f, 0.45f, 0.46f, 1.0f};
  platform.roughness = 0.82f;
  platform.materialId = 9.0f;
  addBox(platform, {platformSide * 2.55f, -0.56f, 0.0f},
         {3.40f, 1.00f, moduleLength});
  parts.push_back(std::move(platform));

  // Sol platform kenarı ve sarı güvenlik çizgisi.
  BuiltinPart edge;
  edge.color = {0.68f, 0.70f, 0.70f, 1.0f};
  edge.roughness = 0.64f;
  edge.materialId = 3.0f;
  addBox(edge, {platformSide * 0.94f, -0.02f, 0.0f},
         {0.15f, 0.12f, 29.7f});
  parts.push_back(std::move(edge));

  BuiltinPart safety;
  safety.color = {0.98f, 0.70f, 0.06f, 1.0f};
  safety.roughness = 0.34f;
  safety.materialId = 7.0f;
  addBox(safety, {platformSide * 0.90f, 0.055f, 0.0f},
         {0.045f, 0.025f, 29.0f});
  parts.push_back(std::move(safety));

  // İstasyonda sol arka duvar; sağ tarafta istasyon duvarı yok, tünel devam ediyor.
  BuiltinPart wall;
  wall.color = {0.72f, 0.73f, 0.71f, 1.0f};
  wall.roughness = 0.76f;
  wall.materialId = 12.0f;
  addBox(wall, {platformSide * 4.35f, 1.55f, 0.0f},
         {0.24f, 3.45f, moduleLength});
  parts.push_back(std::move(wall));

  // Sol duvar plakaları/derzleri.
  BuiltinPart wallTiles;
  wallTiles.color = {0.55f, 0.57f, 0.57f, 1.0f};
  wallTiles.roughness = 0.52f;
  wallTiles.materialId = 12.0f;
  for (float z = -14.0f; z <= 14.0f; z += 2.5f)
    addBox(wallTiles, {platformSide * 4.215f, 1.55f, z},
           {0.025f, 3.28f, 0.025f});
  addBox(wallTiles, {platformSide * 4.215f, 2.55f, 0.0f},
         {0.025f, 0.025f, moduleLength});
  parts.push_back(std::move(wallTiles));

  // Sağ ray bölgesinin altında istasyon zemini: platform değil, düz bakım/ray alanı.
  BuiltinPart rightFloor;
  rightFloor.color = {0.065f, 0.073f, 0.085f, 1.0f};
  rightFloor.roughness = 0.97f;
  rightFloor.materialId = 6.0f;
  addBox(rightFloor, {2.10f, -1.48f, 0.0f},
         {4.20f, 0.34f, moduleLength});
  // Ray yatağı çevresinde kirli ballast şeritleri.
  for (float x : {1.10f, 3.20f}) {
    addBox(rightFloor, {x, -1.27f, 0.0f},
           {0.72f, 0.12f, moduleLength});
  }
  parts.push_back(std::move(rightFloor));

  // Sağ tarafta platform yok ama istasyon kabuğu devam eder:
  // sürekli duvar, servis şeridi, kablo kanalı ve dikey derzler.
  constexpr float rightSide = 1.0f;
  BuiltinPart rightWall;
  rightWall.color = {0.16f, 0.18f, 0.21f, 1.0f};
  rightWall.roughness = 0.88f;
  rightWall.materialId = 12.0f;
  addBox(rightWall, {rightSide * 4.20f, 1.35f, 0.0f},
         {0.24f, 3.15f, moduleLength});
  addBox(rightWall, {rightSide * 3.92f, -0.18f, 0.0f},
         {0.28f, 0.24f, moduleLength});
  for (float z = -12.5f; z <= 12.5f; z += 2.5f)
    addBox(rightWall, {rightSide * 4.055f, 1.45f, z},
           {0.035f, 2.90f, 0.025f});
  parts.push_back(std::move(rightWall));

  BuiltinPart rightService;
  rightService.color = {0.075f, 0.085f, 0.10f, 1.0f};
  rightService.roughness = 0.92f;
  rightService.materialId = 6.0f;
  addBox(rightService, {rightSide * 3.45f, -0.72f, 0.0f},
         {1.05f, 0.18f, moduleLength});
  addBox(rightService, {rightSide * 3.55f, 1.95f, 0.0f},
         {0.10f, 0.14f, moduleLength});
  parts.push_back(std::move(rightService));

  // Sağ duvar boyunca seyrek servis kutuları ve kablo kapakları.
  BuiltinPart rightDetails;
  rightDetails.color = {0.22f, 0.25f, 0.29f, 1.0f};
  rightDetails.roughness = 0.72f;
  rightDetails.materialId = 3.0f;
  for (float z = -10.0f; z <= 10.0f; z += 10.0f) {
    addBox(rightDetails, {rightSide * 3.72f, 0.80f, z},
           {0.18f, 1.05f, 0.72f});
    addBox(rightDetails, {rightSide * 3.78f, 2.20f, z},
           {0.08f, 0.45f, 0.62f});
  }
  parts.push_back(std::move(rightDetails));

  // M4 mavi yönlendirme aksı yalnızca yolcu tarafında.
  BuiltinPart wallAccent;
  wallAccent.color = {0.030f, 0.20f, 0.42f, 1.0f};
  wallAccent.roughness = 0.38f;
  wallAccent.materialId = 7.0f;
  addBox(wallAccent, {platformSide * 4.20f, 0.92f, 0.0f},
         {0.035f, 0.22f, moduleLength});
  for (float z = -11.0f; z <= 11.0f; z += 11.0f)
    addBox(wallAccent, {platformSide * 4.16f, 1.72f, z},
           {0.04f, 1.55f, 0.12f});
  parts.push_back(std::move(wallAccent));

  // Tavan: moduler alçı panel + taşıyici traversler.
  BuiltinPart ceiling;
  ceiling.color = {0.80f, 0.81f, 0.79f, 1.0f};
  ceiling.roughness = 0.82f;
  ceiling.materialId = 12.0f;
  addBox(ceiling, {0.0f, 3.92f, 0.0f}, {8.95f, 0.18f, moduleLength});
  for (float z = -12.0f; z <= 12.0f; z += 6.0f)
    addBox(ceiling, {0.0f, 3.82f, z}, {8.55f, 0.08f, 0.09f});
  parts.push_back(std::move(ceiling));

  BuiltinPart pillars;
  pillars.color = {0.48f, 0.50f, 0.50f, 1.0f};
  pillars.metallic = 0.08f;
  pillars.roughness = 0.56f;
  pillars.materialId = 3.0f;
  for (float side : {-1.0f, 1.0f})
    for (float z = -12.0f; z <= 12.0f; z += 6.0f) {
      addBox(pillars, {side * 3.85f, 1.96f, z}, {0.32f, 3.72f, 0.32f});
      addBox(pillars, {side * 3.85f, 3.76f, z}, {0.48f, 0.14f, 0.48f});
    }
  parts.push_back(std::move(pillars));

  // Banklar / servis dolaplari gibi insan olceginde detaylar.
  BuiltinPart furniture;
  furniture.color = {0.09f, 0.12f, 0.15f, 1.0f};
  furniture.roughness = 0.62f;
  furniture.materialId = 3.0f;
  {
    const float side = -1.0f;
    addBox(furniture, {side * 2.65f, 0.12f, -9.0f},
           {1.05f, 0.10f, 1.95f});
    addBox(furniture, {side * 2.65f, 0.50f, -9.0f},
           {1.05f, 0.10f, 1.95f});
    for (float legZ : {-9.72f, -8.28f})
      addBox(furniture, {side * 2.65f, -0.12f, legZ},
             {0.08f, 0.34f, 0.08f});
  }
  parts.push_back(std::move(furniture));

  BuiltinPart tactile;
  tactile.color = {0.70f, 0.70f, 0.66f, 1.0f};
  tactile.roughness = 0.74f;
  tactile.materialId = 9.0f;
  addBox(tactile, {-1.07f, 0.015f, 0.0f}, {0.12f, 0.045f, 29.0f});
  parts.push_back(std::move(tactile));

  // LED armaturlari.
  BuiltinPart lights;
  lights.color = {1.0f, 0.92f, 0.78f, 1.0f};
  lights.roughness = 0.16f;
  lights.materialId = 5.0f;
  for (float side : {-2.45f})
    for (float z = -12.0f; z <= 12.0f; z += 6.0f)
      addBox(lights, {side, 3.76f, z}, {0.22f, 0.045f, 3.8f});
  parts.push_back(std::move(lights));

  // Yonlendirme panolari / reklam panolarinin hacmi.
  BuiltinPart signs;
  signs.color = {0.025f, 0.12f, 0.23f, 1.0f};
  signs.roughness = 0.30f;
  signs.materialId = 5.0f;
  for (float z : {-10.0f, 0.0f, 10.0f}) {
    for (float side : {-1.0f, 1.0f}) {
      addBox(signs, {side * 2.20f, 3.22f, z},
             {1.70f, 0.46f, 0.07f});
      addBox(signs, {side * 2.20f, 2.70f, z},
             {1.42f, 0.06f, 0.05f});
    }
  }
  parts.push_back(std::move(signs));

  return parts;
}

} // namespace

Model::~Model() {
  // Vulkan resources should be explicitly destroyed via destroy()
}

void Model::destroy(VulkanContext& ctx) {
  if (mVertexBuffer) {
    vmaDestroyBuffer(ctx.allocator(), mVertexBuffer, mVertexAlloc);
    mVertexBuffer = VK_NULL_HANDLE;
  }
  if (mIndexBuffer) {
    vmaDestroyBuffer(ctx.allocator(), mIndexBuffer, mIndexAlloc);
    mIndexBuffer = VK_NULL_HANDLE;
  }
  mSubMeshes.clear();
  mVertexCount = 0;
  mIndexCount = 0;
}

bool Model::load(VulkanContext& ctx, VkCommandPool pool, const std::string& path) {
  cgltf_options options = {};
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
    METRO_ERROR("cgltf parse failed: %s", path.c_str());
    return false;
  }

  if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
    METRO_ERROR("cgltf buffer load failed: %s", path.c_str());
    cgltf_free(data);
    return false;
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  for (size_t m = 0; m < data->meshes_count; ++m) {
    const cgltf_mesh& mesh = data->meshes[m];
    glm::mat4 nodeTransform(1.0f);
    for (size_t n = 0; n < data->nodes_count; ++n) {
      if (data->nodes[n].mesh == &mesh) {
        float matrix[16];
        cgltf_node_transform_world(&data->nodes[n], matrix);
        nodeTransform = glm::make_mat4(matrix);
        break;
      }
    }
    const glm::mat3 normalTransform =
        glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
    
    for (size_t p = 0; p < mesh.primitives_count; ++p) {
      const cgltf_primitive& prim = mesh.primitives[p];
      
      uint32_t firstIndex = static_cast<uint32_t>(indices.size());
      uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
      uint32_t indexCount = 0;

      // Índeksleri oku
      if (prim.indices) {
        indexCount = static_cast<uint32_t>(prim.indices->count);
        for (size_t i = 0; i < indexCount; ++i) {
          indices.push_back(static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i)) + vertexOffset);
        }
      }

      // Vertex elemanlarını oku
      size_t vertexCount = 0;
      cgltf_accessor* posAcc = nullptr;
      cgltf_accessor* normAcc = nullptr;
      cgltf_accessor* tcAcc = nullptr;

      for (size_t a = 0; a < prim.attributes_count; ++a) {
        const cgltf_attribute& attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_position) posAcc = attr.data;
        else if (attr.type == cgltf_attribute_type_normal) normAcc = attr.data;
        else if (attr.type == cgltf_attribute_type_texcoord) tcAcc = attr.data;
      }

      if (posAcc) {
        vertexCount = posAcc->count;
        size_t currentSize = vertices.size();
        vertices.resize(currentSize + vertexCount);

        for (size_t i = 0; i < vertexCount; ++i) {
          cgltf_accessor_read_float(posAcc, i, vertices[currentSize + i].pos, 3);
          const glm::vec4 transformedPosition =
              nodeTransform * glm::vec4(vertices[currentSize + i].pos[0],
                                        vertices[currentSize + i].pos[1],
                                        vertices[currentSize + i].pos[2], 1.0f);
          vertices[currentSize + i].pos[0] = transformedPosition.x;
          vertices[currentSize + i].pos[1] = transformedPosition.y;
          vertices[currentSize + i].pos[2] = transformedPosition.z;
          
          if (normAcc) {
            cgltf_accessor_read_float(normAcc, i, vertices[currentSize + i].normal, 3);
            const glm::vec3 transformedNormal =
                glm::normalize(normalTransform *
                               glm::vec3(vertices[currentSize + i].normal[0],
                                         vertices[currentSize + i].normal[1],
                                         vertices[currentSize + i].normal[2]));
            vertices[currentSize + i].normal[0] = transformedNormal.x;
            vertices[currentSize + i].normal[1] = transformedNormal.y;
            vertices[currentSize + i].normal[2] = transformedNormal.z;
          } else {
            vertices[currentSize + i].normal[0] = 0.0f;
            vertices[currentSize + i].normal[1] = 1.0f;
            vertices[currentSize + i].normal[2] = 0.0f;
          }

          if (tcAcc) {
            cgltf_accessor_read_float(tcAcc, i, vertices[currentSize + i].texCoord, 2);
          } else {
            vertices[currentSize + i].texCoord[0] = 0.0f;
            vertices[currentSize + i].texCoord[1] = 0.0f;
          }
        }
      }

      // Malzeme: cgltf spec varsayılanları (metallic/roughness 1.0) dokusuz
      // modellerde ayna-siyah görünür; material yoksa makul nötr değerler.
      SubMesh sub{};
      sub.firstIndex = firstIndex;
      sub.indexCount = indexCount;
      sub.firstVertex = vertexOffset;
      sub.vertexCount = static_cast<uint32_t>(vertexCount);
      if (prim.material != nullptr) {
        const cgltf_pbr_metallic_roughness& pbr =
            prim.material->pbr_metallic_roughness;
        for (int j = 0; j < 4; ++j) sub.baseColor[j] = pbr.base_color_factor[j];
        sub.metallic = pbr.metallic_factor;
        sub.roughness = pbr.roughness_factor;
      }
      mSubMeshes.push_back(sub);
    }
  }

  cgltf_free(data);

  if (vertices.empty()) return false;

  mVertexCount = static_cast<uint32_t>(vertices.size());
  mIndexCount = static_cast<uint32_t>(indices.size());

  // Bufferları oluştur (Staging -> Device Local)
  VkDeviceSize vSize = sizeof(Vertex) * vertices.size();
  VkDeviceSize iSize = sizeof(uint32_t) * indices.size();

  VkBuffer vStaging, iStaging;
  VmaAllocation vAlloc, iAlloc;
  createBuffer(ctx, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, vStaging, vAlloc);
  
  void* mappedData = nullptr;
  vmaMapMemory(ctx.allocator(), vAlloc, &mappedData);
  std::memcpy(mappedData, vertices.data(), vSize);
  vmaUnmapMemory(ctx.allocator(), vAlloc);

  createBuffer(ctx, vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mVertexBuffer, mVertexAlloc);
  copyBuffer(ctx, pool, vStaging, mVertexBuffer, vSize);
  vmaDestroyBuffer(ctx.allocator(), vStaging, vAlloc);

  if (mIndexCount > 0) {
    createBuffer(ctx, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, iStaging, iAlloc);
    vmaMapMemory(ctx.allocator(), iAlloc, &mappedData);
    std::memcpy(mappedData, indices.data(), iSize);
    vmaUnmapMemory(ctx.allocator(), iAlloc);

    createBuffer(ctx, iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, mIndexBuffer, mIndexAlloc);
    copyBuffer(ctx, pool, iStaging, mIndexBuffer, iSize);
    vmaDestroyBuffer(ctx.allocator(), iStaging, iAlloc);
  }

  METRO_INFO("Model yuklendi: %s (V: %u, I: %u)", path.c_str(), mVertexCount, mIndexCount);
  return true;
}


bool Model::loadBuiltin(VulkanContext& ctx, VkCommandPool pool, BuiltinModelType type) {
  destroy(ctx);

  std::vector<BuiltinPart> parts;
  if (type == BuiltinModelType::TrainCar) {
    parts = makeTrainCarParts();
  } else if (type == BuiltinModelType::StationModule) {
    parts = makeStationModuleParts();
  } else {
    parts = makeTunnelModuleParts();
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  mSubMeshes.clear();

  for (const BuiltinPart& part : parts) {
    const uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
    const uint32_t firstIndex = static_cast<uint32_t>(indices.size());
    vertices.insert(vertices.end(), part.vertices.begin(), part.vertices.end());
    for (uint32_t idx : part.indices)
      indices.push_back(idx + vertexOffset);

    SubMesh sub{};
    sub.firstIndex = firstIndex;
    sub.indexCount = static_cast<uint32_t>(part.indices.size());
    sub.firstVertex = vertexOffset;
    sub.vertexCount = static_cast<uint32_t>(part.vertices.size());
    sub.baseColor[0] = part.color.r;
    sub.baseColor[1] = part.color.g;
    sub.baseColor[2] = part.color.b;
    sub.baseColor[3] = part.color.a;
    sub.metallic = part.metallic;
    sub.roughness = part.roughness;
    sub.materialId = part.materialId;
    mSubMeshes.push_back(sub);
  }

  mVertexCount = static_cast<uint32_t>(vertices.size());
  mIndexCount = static_cast<uint32_t>(indices.size());
  if (vertices.empty() || indices.empty()) return false;

  const VkDeviceSize vSize = sizeof(Vertex) * vertices.size();
  const VkDeviceSize iSize = sizeof(uint32_t) * indices.size();

  VkBuffer vStaging{}, iStaging{};
  VmaAllocation vAlloc{}, iAlloc{};
  createBuffer(ctx, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_MEMORY_USAGE_CPU_ONLY, vStaging, vAlloc);
  void* mapped = nullptr;
  vmaMapMemory(ctx.allocator(), vAlloc, &mapped);
  std::memcpy(mapped, vertices.data(), static_cast<size_t>(vSize));
  vmaUnmapMemory(ctx.allocator(), vAlloc);

  createBuffer(ctx, vSize,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, mVertexBuffer, mVertexAlloc);
  copyBuffer(ctx, pool, vStaging, mVertexBuffer, vSize);
  vmaDestroyBuffer(ctx.allocator(), vStaging, vAlloc);

  createBuffer(ctx, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VMA_MEMORY_USAGE_CPU_ONLY, iStaging, iAlloc);
  vmaMapMemory(ctx.allocator(), iAlloc, &mapped);
  std::memcpy(mapped, indices.data(), static_cast<size_t>(iSize));
  vmaUnmapMemory(ctx.allocator(), iAlloc);

  createBuffer(ctx, iSize,
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VMA_MEMORY_USAGE_GPU_ONLY, mIndexBuffer, mIndexAlloc);
  copyBuffer(ctx, pool, iStaging, mIndexBuffer, iSize);
  vmaDestroyBuffer(ctx.allocator(), iStaging, iAlloc);

  METRO_INFO("Builtin model hazir: %s (V: %u, I: %u)",
             type == BuiltinModelType::TrainCar
                 ? "TrainCar"
                 : (type == BuiltinModelType::StationModule ? "StationModule" : "TunnelModule"),
             mVertexCount, mIndexCount);
  return true;
}

void Model::bind(VkCommandBuffer cmd) const {
  if (mVertexCount == 0) return;

  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, &mVertexBuffer, offsets);
  if (mIndexCount > 0) {
    vkCmdBindIndexBuffer(cmd, mIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
  }
}

void Model::drawSubMesh(VkCommandBuffer cmd, size_t index) const {
  const SubMesh& sub = mSubMeshes[index];
  if (sub.indexCount > 0) {
    vkCmdDrawIndexed(cmd, sub.indexCount, 1, sub.firstIndex, 0, 0);
  } else {
    vkCmdDraw(cmd, sub.vertexCount, 1, sub.firstVertex, 0);
  }
}

} // namespace metro::rhi
