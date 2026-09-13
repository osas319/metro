#pragma once
// Çerçeve render'ı: render pass (renk+derinlik), PBR pipeline, frame UBO
// (descriptor set 0), per-submesh malzeme push-constant'ları, senkronizasyon.
// Sahne sistemi (EnTT entity render) buraya eklenerek Forward+'a evrilir.
#include <volk.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <string>

#include "rhi/Swapchain.hpp"
#include "app/Camera.hpp"
#include "rhi/Model.hpp"

struct SDL_Window;

namespace metro::rhi {

class VulkanContext;

struct Vertex {
  float pos[3];
  float normal[3];
  float texCoord[2];
};
static_assert(sizeof(Vertex) == 32, "Vertex yerlesimi shader ile birebir olmali");

// Per-frame uniform'lar (std140; shader'daki FrameUniforms ile birebir).
struct FrameUniforms {
  glm::mat4 viewProj;
  glm::vec4 lightDir;   // xyz: ışığın gidiş yönü
  glm::vec4 cameraPos;  // xyz: göz konumu
};

// Per-submesh push-constant (vertex+fragment; std430 hizası).
struct ModelPush {
  glm::mat4 model;
  glm::vec4 baseColor;
  float metallic;
  float roughness;
};

class Renderer {
public:
  static constexpr uint32_t MaxFramesInFlight = 2;

  bool init(VulkanContext& ctx, SDL_Window* window,
            const std::string& manifestModelPath = {},
            float routeLength = 2000.0f, float platformWidth = 4.0f);
  void shutdown();

  // Bir kareyi uçur (acquire → record → submit → present).
  void drawFrame(const app::Camera& camera, float trainPosition);

  // Pencere boyutu değişti; swapchain'i güvenli anda yeniden kur.
  void onResize();

private:
  void createRenderPass();
  void createDepthResources();
  void createFrameUniforms();   // UBO + descriptor pool/set'ler (per image)
  void createPipeline();
  void createFramebuffers();
  void createCommandObjects();
  void createSyncObjects();
  void destroySwapchainDependent();
  void recreateSwapchain();
  void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                           const app::Camera& camera, float trainPosition);
  VkShaderModule loadShader(const char* filename);
  VkFormat pickDepthFormat() const;

  VulkanContext* mCtx = nullptr;
  SDL_Window* mWindow = nullptr;

  Swapchain mSwapchain;
  VkRenderPass mRenderPass = VK_NULL_HANDLE;
  VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
  VkPipeline mPipeline = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> mFramebuffers;

  // Derinlik buffer'ı (extent swapchain'e bağlı)
  VkImage mDepthImage = VK_NULL_HANDLE;
  VmaAllocation mDepthAlloc = VK_NULL_HANDLE;
  VkImageView mDepthView = VK_NULL_HANDLE;
  VkFormat mDepthFormat = VK_FORMAT_UNDEFINED;

  // Frame UBO + descriptor'lar (swapchain image sayısı kadar)
  VkDescriptorSetLayout mDescSetLayout = VK_NULL_HANDLE; // pipeline ömründe
  VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
  std::vector<VkBuffer> mFrameUboBuffers;
  std::vector<VmaAllocation> mFrameUboAllocs;
  std::vector<void*> mFrameUboMapped;
  std::vector<VkDescriptorSet> mDescriptorSets;

  Model mModel;
  float mRouteLength = 2000.0f;
  float mPlatformWidth = 4.0f;

  VkCommandPool mCommandPool = VK_NULL_HANDLE;
  // Tüm per-frame senkronizasyon nesneleri swapchain image sayısına göre
  // boyutlanır. Böylece MaxFramesInFlight (2) ile swapchain image sayısı (4)
  // arasındaki uyumsuzluk kaynaklı semafor yarış durumu tamamen önlenir.
  std::vector<VkCommandBuffer> mCommands;
  std::vector<VkSemaphore> mImageAvailable;
  std::vector<VkSemaphore> mRenderFinished;
  std::vector<VkFence> mInFlight;
  uint32_t mFrame = 0;
  VkFormat mSwapFormat = VK_FORMAT_UNDEFINED; // render pass formatı değişim kontrolü
};

} // namespace metro::rhi
