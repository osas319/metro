#include "rhi/Model.hpp"
#include "rhi/Renderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "core/Log.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <cstring>
#include <cmath>

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

  BuiltinPart chassis;
  chassis.color = {0.10f, 0.12f, 0.15f, 1.0f};
  chassis.roughness = 0.72f;
  addBox(chassis, {0.0f, -0.50f, 0.0f}, {2.55f, 0.80f, 17.0f});
  parts.push_back(std::move(chassis));

  BuiltinPart lower;
  lower.color = {0.055f, 0.28f, 0.52f, 1.0f};
  lower.roughness = 0.46f;
  addBox(lower, {0.0f, 0.00f, 0.0f}, {2.68f, 0.72f, 16.8f});
  parts.push_back(std::move(lower));

  BuiltinPart upper;
  upper.color = {0.83f, 0.84f, 0.82f, 1.0f};
  upper.metallic = 0.05f;
  upper.roughness = 0.30f;
  addBox(upper, {0.0f, 0.72f, 0.0f}, {2.76f, 1.16f, 15.9f});
  addFrustum(upper, -9.75f, -7.95f, 1.02f, 1.38f, 0.02f, 1.88f);
  addFrustum(upper, 7.95f, 9.75f, 1.38f, 1.02f, 0.02f, 1.88f);
  parts.push_back(std::move(upper));

  BuiltinPart windows;
  windows.color = {0.012f, 0.035f, 0.060f, 1.0f};
  windows.metallic = 0.0f;
  windows.roughness = 0.10f;
  windows.materialId = 4.0f;
  for (float side : {-1.0f, 1.0f})
    addBox(windows, {side * 1.395f, 0.93f, 0.0f}, {0.045f, 0.52f, 13.8f});
  for (float z : {-9.68f, 9.68f})
    addBox(windows, {0.0f, 1.13f, z}, {1.55f, 0.58f, 0.045f});
  parts.push_back(std::move(windows));

  BuiltinPart endCap;
  endCap.color = {0.075f, 0.34f, 0.62f, 1.0f};
  endCap.metallic = 0.05f;
  endCap.roughness = 0.24f;
  for (float z : {-9.70f, 9.70f})
    addBox(endCap, {0.0f, 0.43f, z}, {1.95f, 0.62f, 0.10f});
  parts.push_back(std::move(endCap));

  BuiltinPart roof;
  roof.color = {0.48f, 0.50f, 0.51f, 1.0f};
  roof.metallic = 0.22f;
  roof.roughness = 0.34f;
  addBox(roof, {0.0f, 1.63f, 0.0f}, {2.48f, 0.25f, 17.2f});
  parts.push_back(std::move(roof));

  BuiltinPart bogies;
  bogies.color = {0.08f, 0.09f, 0.11f, 1.0f};
  bogies.roughness = 0.78f;
  for (float z : {-5.9f, 5.9f})
    addBox(bogies, {0.0f, -0.92f, z}, {1.72f, 0.28f, 1.45f});
  parts.push_back(std::move(bogies));

  BuiltinPart roofEquip;
  roofEquip.color = {0.22f, 0.24f, 0.26f, 1.0f};
  roofEquip.metallic = 0.18f;
  roofEquip.roughness = 0.48f;
  addBox(roofEquip, {0.0f, 1.88f, -1.8f}, {0.42f, 0.26f, 4.4f});
  addBox(roofEquip, {0.0f, 1.88f, 3.6f}, {0.30f, 0.22f, 1.5f});
  parts.push_back(std::move(roofEquip));

  return parts;
}

std::vector<BuiltinPart> makeStationModuleParts() {
  std::vector<BuiltinPart> parts;

  BuiltinPart platform;
  platform.color = {0.48f, 0.49f, 0.48f, 1.0f};
  platform.roughness = 0.88f;
  for (float side : {-1.0f, 1.0f})
    addBox(platform, {side * 2.55f, -0.56f, 0.0f}, {3.40f, 1.00f, 30.0f});
  parts.push_back(std::move(platform));

  BuiltinPart wall;
  wall.color = {0.76f, 0.74f, 0.69f, 1.0f};
  wall.roughness = 0.72f;
  for (float side : {-1.0f, 1.0f})
    addBox(wall, {side * 4.35f, 1.55f, 0.0f}, {0.22f, 3.45f, 30.0f});
  parts.push_back(std::move(wall));

  BuiltinPart wallAccent;
  wallAccent.color = {0.035f, 0.18f, 0.34f, 1.0f};
  wallAccent.roughness = 0.42f;
  for (float side : {-1.0f, 1.0f}) {
    for (float z = -12.0f; z <= 12.0f; z += 6.0f)
      addBox(wallAccent, {side * 4.22f, 1.65f, z}, {0.035f, 2.1f, 1.2f});
  }
  parts.push_back(std::move(wallAccent));

  BuiltinPart ceiling;
  ceiling.color = {0.82f, 0.82f, 0.78f, 1.0f};
  ceiling.roughness = 0.84f;
  addBox(ceiling, {0.0f, 3.92f, 0.0f}, {8.95f, 0.18f, 30.0f});
  parts.push_back(std::move(ceiling));

  BuiltinPart pillars;
  pillars.color = {0.52f, 0.53f, 0.51f, 1.0f};
  pillars.metallic = 0.06f;
  pillars.roughness = 0.60f;
  for (float side : {-1.0f, 1.0f})
    for (float z = -12.0f; z <= 12.0f; z += 6.0f)
      addBox(pillars, {side * 3.85f, 1.96f, z}, {0.30f, 3.72f, 0.30f});
  parts.push_back(std::move(pillars));

  BuiltinPart safety;
  safety.color = {0.96f, 0.70f, 0.05f, 1.0f};
  safety.roughness = 0.42f;
  for (float side : {-1.0f, 1.0f})
    addBox(safety, {side * 0.89f, -0.02f, 0.0f}, {0.055f, 0.025f, 29.0f});
  parts.push_back(std::move(safety));

  BuiltinPart tactile;
  tactile.color = {0.76f, 0.76f, 0.70f, 1.0f};
  tactile.roughness = 0.76f;
  for (float side : {-1.0f, 1.0f})
    addBox(tactile, {side * 1.08f, 0.00f, 0.0f}, {0.08f, 0.04f, 29.0f});
  parts.push_back(std::move(tactile));

  BuiltinPart lights;
  lights.color = {1.0f, 0.91f, 0.73f, 1.0f};
  lights.roughness = 0.18f;
  lights.materialId = 5.0f;
  for (float side : {-1.0f, 1.0f})
    for (float z = -12.0f; z <= 12.0f; z += 6.0f)
      addBox(lights, {side * 2.45f, 3.76f, z}, {0.22f, 0.045f, 3.8f});
  parts.push_back(std::move(lights));

  BuiltinPart signs;
  signs.color = {0.05f, 0.17f, 0.29f, 1.0f};
  signs.roughness = 0.36f;
  for (float z : {-10.0f, 0.0f, 10.0f}) {
    addBox(signs, {-2.1f, 3.55f, z}, {1.65f, 0.36f, 0.06f});
    addBox(signs, {2.1f, 3.55f, z}, {1.65f, 0.36f, 0.06f});
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

  std::vector<BuiltinPart> parts =
      type == BuiltinModelType::TrainCar ? makeTrainCarParts()
                                         : makeStationModuleParts();

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
             type == BuiltinModelType::TrainCar ? "TrainCar" : "StationModule",
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
