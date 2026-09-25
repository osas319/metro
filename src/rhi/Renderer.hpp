#pragma once
// Çerçeve render'ı, iki geçiş: (1) sahne — PBR pipeline HDR offscreen renk
// hedefine (mHdrFormat, lineer) render eder; (2) post-process — tam ekran
// üçgen, HDR hedefi örnekler, ACES filmic tonemap uygular ve swapchain'e
// (LDR, sRGB) yazar. Bloom/SSAO/CSM gelecekte bu iki geçiş arasına veya
// sahne geçişine ek olarak eklenecek. Frame UBO (descriptor set 0, sahne
// geçişi), per-submesh malzeme push-constant'ları, senkronizasyon.
// Sahne sistemi (EnTT entity render) buraya eklenerek Forward+'a evrilir.
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL.h>
#include <imgui.h>

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
  glm::vec4 headlightPos; // xyz: tren farı dünya konumu
  glm::vec4 headlightColor; // rgb: far rengi, a: menzil
};

// Per-submesh push-constant (vertex+fragment; std430 hizası).
struct ModelPush {
  glm::mat4 model;
  glm::vec4 baseColor;
  float metallic;
  float roughness;
  float materialId;
};

struct PostProcessPush {
  float trainSpeedMps = 0.0f;
  float pad0 = 0.0f;
  float pad1 = 0.0f;
  float pad2 = 0.0f;
};

class Renderer {
public:
  static constexpr uint32_t MaxFramesInFlight = 2;

  bool init(VulkanContext& ctx, SDL_Window* window,
            const std::string& manifestModelPath = {},
            float routeLength = 2000.0f, float platformWidth = 4.0f,
            float trackGauge = 2.4f, float trainWidth = 2.8f,
            float trainHeight = 3.2f, float columnSpacing = 24.0f,
            const glm::vec3& platformEdgePosition = glm::vec3(0.0f),
            const glm::vec3& stopPosition = glm::vec3(0.0f),
            const std::vector<float>& stopPositions = {},
            size_t blockCount = 8);
  void shutdown();

  // Bir kareyi uçur (acquire → record → submit → present).
  struct EditorMarker {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{0.25f};
    glm::vec4 color{0.85f, 0.55f, 0.10f, 1.0f};
  };

  enum class EditorRenderKind : uint8_t {
    Train,
    Station,
    Asset
  };

  struct EditorRenderOverride {
    EditorRenderKind kind{EditorRenderKind::Train};
    size_t index{0};
    glm::mat4 transform{1.0f};
    bool visible{true};
    std::string assetPath;
  };

  void drawFrame(const app::Camera& camera, float trainPosition, float trainSpeed,
                 float doorOpenFraction, bool trainBraking, bool editorBackdropBlur,
                 const std::vector<bool>& occupiedBlocks,
                 const std::vector<glm::vec2>& passengerPositions = {},
                 const std::vector<EditorMarker>& editorMarkers = {},
                 const std::vector<EditorRenderOverride>& editorOverrides = {});

  // Pencere boyutu değişti; swapchain'i güvenli anda yeniden kur.
  void onResize();

  // Editor UI (Dear ImGui) yaşam döngüsü.
  bool initEditorUI();
  void shutdownEditorUI();
  void beginEditorFrame();
  void finishEditorFrame();
  void renderEditorUI(VkCommandBuffer cmd);
  ImTextureID editorBlurTexture() const { return mEditorBlurTexture; }
  void processEditorEvent(const SDL_Event& e);
  bool editorWantsMouse() const;
  bool editorWantsKeyboard() const;

private:
  void createRenderPass();       // sahne (PBR) render pass — HDR renk hedefine yazar
  void createDepthResources();
  void createHdrResources();     // offscreen HDR renk hedefi (per swapchain image) + sampler
  void createMaterialTextures(); // gerçek BMP PBR albedo texture array
  void destroyMaterialTextures();
  void createEditorBlurResources();
  void destroyEditorBlurResources();
  void createEditorBlurPipeline();
  void createFrameUniforms();   // UBO + descriptor pool/set'ler (per image)
  void createPipeline();
  void createFramebuffers();
  void createPostRenderPass();   // tonemap render pass — swapchain'e (LDR) yazar
  void createPostResources();    // HDR örnekleme descriptor set layout/pool/set'ler
  void createPostPipeline();     // tam fullscreen üçgen + ACES tonemap pipeline
  void createPostFramebuffers();
  void createCommandObjects();
  void createSyncObjects();
  void destroySwapchainDependent();
  void recreateSwapchain();
  void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                           const app::Camera& camera, float trainPosition, float trainSpeed,
                           float doorOpenFraction, bool trainBraking, bool editorBackdropBlur,
                           const std::vector<bool>& occupiedBlocks,
                           const std::vector<glm::vec2>& passengerPositions,
                           const std::vector<EditorMarker>& editorMarkers,
                           const std::vector<EditorRenderOverride>& editorOverrides);
  VkShaderModule loadShader(const char* filename);
  VkFormat pickDepthFormat() const;
  Model* getEditorModel(const std::string& path) const;

  enum class SceneModel : uint8_t {
    Box,
    TrainCar,
    StationModule,
    TunnelModule,
    External
  };

  struct SceneInstance {
    glm::mat4 transform{1.0f};
    glm::vec4 color{1.0f};
    float roughness = 0.7f;
    float materialId = 0.0f;
    SceneModel model = SceneModel::Box;
    bool useModelMaterial = false;
    std::string externalAsset;
  };

  // Kadıköy sahnesini veri tabanlı placeholder geometriyle kurar.
  // İleride gerçek GLB asset'leri aynı instance listesinin yerini alabilir.
  std::vector<SceneInstance> buildKadikoyScene(
      float trainPosition, float trainSpeed, float doorOpenFraction, bool trainBraking,
      const std::vector<bool>& occupiedBlocks,
      const std::vector<glm::vec2>& passengerPositions,
      const std::vector<EditorRenderOverride>& editorOverrides) const;

  VulkanContext* mCtx = nullptr;
  SDL_Window* mWindow = nullptr;

  Swapchain mSwapchain;

  // --- Sahne geçişi (PBR): HDR offscreen hedefe render eder ---
  VkRenderPass mRenderPass = VK_NULL_HANDLE;
  VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
  VkPipeline mPipeline = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> mFramebuffers;

  // HDR renk hedefi (lineer, sıkıştırılmamış); swapchain image sayısı kadar,
  // her biri kendi UBO'su gibi kendi imageIndex'iyle eşleşir.
  static constexpr VkFormat kHdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  std::vector<VkImage> mHdrImages;
  std::vector<VmaAllocation> mHdrAllocs;
  std::vector<VkImageView> mHdrViews;
  VkSampler mHdrSampler = VK_NULL_HANDLE; // pipeline ömründe; extent'ten bağımsız

  // 6 katmanlı gerçek malzeme texture array'i: concrete/tile/ballast/rail/train/glass.
  static constexpr uint32_t kMaterialTextureLayers = 6;
  static constexpr uint32_t kMaterialTextureSize = 128;
  VkImage mMaterialTextureImage = VK_NULL_HANDLE;
  VmaAllocation mMaterialTextureAlloc = VK_NULL_HANDLE;
  VkImageView mMaterialTextureView = VK_NULL_HANDLE;
  VkSampler mMaterialTextureSampler = VK_NULL_HANDLE;

  // Editör camı için düşük çözünürlüklü bulanık sahne hedefi. Bu görüntü
  // yalnızca ImGui panellerinin arkasında örneklenir; ana viewport bulanmaz.
  static constexpr VkFormat kEditorBlurFormat = VK_FORMAT_R8G8B8A8_UNORM;
  VkImage mEditorBlurImage = VK_NULL_HANDLE;
  VmaAllocation mEditorBlurAlloc = VK_NULL_HANDLE;
  VkImageView mEditorBlurView = VK_NULL_HANDLE;
  VkSampler mEditorBlurSampler = VK_NULL_HANDLE;
  VkRenderPass mEditorBlurRenderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout mEditorBlurDescSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool mEditorBlurDescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> mEditorBlurDescriptorSets;
  VkPipelineLayout mEditorBlurPipelineLayout = VK_NULL_HANDLE;
  VkPipeline mEditorBlurPipeline = VK_NULL_HANDLE;
  ImTextureID mEditorBlurTexture = 0;

  // --- Post-process geçişi: HDR'ı örnekleyip ACES tonemap ile swapchain'e (LDR) yazar ---
  VkRenderPass mPostRenderPass = VK_NULL_HANDLE;
  VkDescriptorSetLayout mPostDescSetLayout = VK_NULL_HANDLE; // pipeline ömründe
  VkDescriptorPool mPostDescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> mPostDescriptorSets; // per swapchain image (HDR görünümü)
  VkPipelineLayout mPostPipelineLayout = VK_NULL_HANDLE;
  VkPipeline mPostPipeline = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> mPostFramebuffers; // swapchain hedefli

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
  Model mTrainCarModel;
  Model mStationModuleModel;
  Model mTunnelModuleModel;
  mutable std::unordered_map<std::string, std::unique_ptr<Model>> mEditorModels;
  mutable std::unordered_set<std::string> mFailedEditorAssets;
  float mRouteLength = 2000.0f;
  float mPlatformWidth = 4.0f;
  float mTrackGauge = 2.4f;
  float mTrainWidth = 2.8f;
  float mTrainHeight = 3.2f;
  float mColumnSpacing = 24.0f;
  glm::vec3 mPlatformEdgePosition{0.0f};
  glm::vec3 mStopPosition{0.0f};
  std::vector<float> mStopPositions;
  size_t mBlockCount = 8;

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
  bool mEditorUIInitialized = false;
};

} // namespace metro::rhi
