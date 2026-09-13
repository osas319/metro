#include "rhi/Model.hpp"
#include "rhi/Renderer.hpp"
#include "rhi/VulkanContext.hpp"
#include "core/Log.hpp"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#include <stdexcept>
#include <cstring>

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
          
          if (normAcc) {
            cgltf_accessor_read_float(normAcc, i, vertices[currentSize + i].normal, 3);
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
  if (mIndexCount > 0) {
    vkCmdDrawIndexed(cmd, sub.indexCount, 1, sub.firstIndex, 0, 0);
  } else {
    vkCmdDraw(cmd, mVertexCount, 1, 0, 0);
  }
}

} // namespace metro::rhi
