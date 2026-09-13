#pragma once
// glTF model: vertex/index buffer'lar + per-mesh malzeme faktörleri.
// Malzeme sistemi büyüdükçe (doku, normal map) SubMesh zenginleşecek.
#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <string>
#include <vector>

namespace metro::rhi {

class VulkanContext;

struct SubMesh {
  uint32_t firstIndex = 0;
  uint32_t indexCount = 0;
  uint32_t firstVertex = 0;
  uint32_t vertexCount = 0;
  float baseColor[4] = {0.8f, 0.8f, 0.8f, 1.0f}; // pbrMetallicRoughness.baseColorFactor
  float metallic = 0.1f;                          // metallicFactor
  float roughness = 0.7f;                         // roughnessFactor
};

class Model {
public:
  ~Model();

  // glTF/glbb dosyasını yükler; staging → device-local kopyası dahil.
  bool load(VulkanContext& ctx, VkCommandPool pool, const std::string& path);
  void destroy(VulkanContext& ctx);

  // Vertex/index buffer'larını bağla; çizim Renderer'da per-submesh yapılır
  // (malzeme push-constant'ı orada yazılır).
  void bind(VkCommandBuffer cmd) const;
  void drawSubMesh(VkCommandBuffer cmd, size_t index) const;

  size_t subMeshCount() const { return mSubMeshes.size(); }
  const SubMesh& subMesh(size_t index) const { return mSubMeshes[index]; }

private:
  VkBuffer mVertexBuffer = VK_NULL_HANDLE;
  VmaAllocation mVertexAlloc = VK_NULL_HANDLE;

  VkBuffer mIndexBuffer = VK_NULL_HANDLE;
  VmaAllocation mIndexAlloc = VK_NULL_HANDLE;

  uint32_t mVertexCount = 0;
  uint32_t mIndexCount = 0;
  std::vector<SubMesh> mSubMeshes;
};

} // namespace metro::rhi
