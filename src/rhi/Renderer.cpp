#include "rhi/Renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#include "core/Log.hpp"
#include "rhi/VulkanContext.hpp"

namespace metro::rhi {

// SPIR-V dizini: ortam değişkeniyle ezilebilir; varsayılan CMake tanımı.
static std::string shaderDir() {
  if (const char* env = std::getenv("METRO_SHADER_DIR")) return env;
  return METRO_SHADER_DIR;
}

VkShaderModule Renderer::loadShader(const char* filename) {
  std::ifstream file(shaderDir() + "/" + filename, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::runtime_error(std::string("Shader okunamiyor: ") + shaderDir() + "/" + filename +
                             " (once 'tools/container.sh build' calistirilmali)");
  }
  const std::streamsize size = file.tellg();
  if (size == 0 || size % 4 != 0) {
    throw std::runtime_error(std::string("Shader bozuk (SPIR-V 4 bayt hizali olmali): ") + filename);
  }
  std::vector<char> bytes(static_cast<size_t>(size));
  file.seekg(0);
  file.read(bytes.data(), size);

  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = static_cast<size_t>(size);
  ci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(mCtx->device(), &ci, nullptr, &module) != VK_SUCCESS) {
    throw std::runtime_error(std::string("vkCreateShaderModule: ") + filename);
  }
  return module;
}

VkFormat Renderer::pickDepthFormat() const {
  const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                 VK_FORMAT_D24_UNORM_S8_UINT};
  for (VkFormat f : candidates) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(mCtx->physicalDevice(), f, &props);
    if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
      return f;
    }
  }
  return VK_FORMAT_D16_UNORM;
}

bool Renderer::init(VulkanContext& ctx, SDL_Window* window) {
  mCtx = &ctx;
  mWindow = window;

  try {
    mSwapchain.create(ctx, window);
    createRenderPass();
    createDepthResources();
    createFrameUniforms();
    createPipeline();
    createFramebuffers();
    createCommandObjects();
    const char* modelPath = std::getenv("METRO_MODEL_PATH");
    const char* selectedModel = modelPath != nullptr ? modelPath : "assets/box.glb";
    if (!mModel.load(ctx, mCommandPool, selectedModel)) {
      throw std::runtime_error(std::string("Model yuklenemedi: ") + selectedModel);
    }
    createSyncObjects();
  } catch (const std::exception& e) {
    METRO_ERROR("Renderer init: %s", e.what());
    return false;
  }
  METRO_INFO("Renderer hazir (PBR pipeline, model: %s, %u frame-in-flight)",
             std::getenv("METRO_MODEL_PATH") != nullptr ? std::getenv("METRO_MODEL_PATH")
                                                        : "assets/box.glb",
             MaxFramesInFlight);
  return true;
}

void Renderer::createRenderPass() {
  mSwapFormat = mSwapchain.format();
  mDepthFormat = pickDepthFormat();

  VkAttachmentDescription color{};
  color.format = mSwapchain.format();
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depth{};
  depth.format = mDepthFormat;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription attachments[] = {color, depth};

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  // Sunum motorunun okuması ile renk eki yazmasını çakıştırma.
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 2;
  ci.pAttachments = attachments;
  ci.subpassCount = 1;
  ci.pSubpasses = &subpass;
  ci.dependencyCount = 1;
  ci.pDependencies = &dep;

  if (vkCreateRenderPass(mCtx->device(), &ci, nullptr, &mRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateRenderPass");
  }
}

void Renderer::createDepthResources() {
  VkImageCreateInfo img{};
  img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  img.imageType = VK_IMAGE_TYPE_2D;
  img.format = mDepthFormat;
  img.extent.width = mSwapchain.extent().width;
  img.extent.height = mSwapchain.extent().height;
  img.extent.depth = 1;
  img.mipLevels = 1;
  img.arrayLayers = 1;
  img.samples = VK_SAMPLE_COUNT_1_BIT;
  img.tiling = VK_IMAGE_TILING_OPTIMAL;
  img.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO;

  if (vmaCreateImage(mCtx->allocator(), &img, &alloc, &mDepthImage, &mDepthAlloc, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("vmaCreateImage (depth)");
  }

  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = mDepthImage;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = mDepthFormat;
  view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  // Kombine depth+stencil formatlarında stencil bölümü de işaretle.
  if (mDepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || mDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
    view.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  view.subresourceRange.levelCount = 1;
  view.subresourceRange.layerCount = 1;
  if (vkCreateImageView(mCtx->device(), &view, nullptr, &mDepthView) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateImageView (depth)");
  }
}

void Renderer::createFrameUniforms() {
  const uint32_t count = mSwapchain.imageCount();

  // Set layout pipeline ömründe yaşar; yalnız ilk kurulumda yaratılır.
  if (mDescSetLayout == VK_NULL_HANDLE) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(mCtx->device(), &layoutCi, nullptr, &mDescSetLayout) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateDescriptorSetLayout");
    }
  }

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = count;

  VkDescriptorPoolCreateInfo poolCi{};
  poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolCi.maxSets = count;
  poolCi.poolSizeCount = 1;
  poolCi.pPoolSizes = &poolSize;
  if (vkCreateDescriptorPool(mCtx->device(), &poolCi, nullptr, &mDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateDescriptorPool");
  }

  // Her swapchain image'i kendi UBO'sunu kullanır: acquire dönene kadar o
  // image'in önceki karesi bitmiş olur, mapped yazım güvenlidir.
  mFrameUboBuffers.resize(count);
  mFrameUboAllocs.resize(count);
  mFrameUboMapped.resize(count);
  mDescriptorSets.resize(count);

  for (uint32_t i = 0; i < count; ++i) {
    VkBufferCreateInfo buf{};
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.size = sizeof(FrameUniforms);
    buf.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo alloc{};
    alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;
    alloc.usage = VMA_MEMORY_USAGE_AUTO;

    VmaAllocationInfo out{};
    if (vmaCreateBuffer(mCtx->allocator(), &buf, &alloc, &mFrameUboBuffers[i],
                        &mFrameUboAllocs[i], &out) != VK_SUCCESS) {
      throw std::runtime_error("vmaCreateBuffer (frame UBO)");
    }
    mFrameUboMapped[i] = out.pMappedData;

    VkDescriptorSetAllocateInfo setAi{};
    setAi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAi.descriptorPool = mDescriptorPool;
    setAi.descriptorSetCount = 1;
    setAi.pSetLayouts = &mDescSetLayout;
    if (vkAllocateDescriptorSets(mCtx->device(), &setAi, &mDescriptorSets[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkAllocateDescriptorSets");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mFrameUboBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(FrameUniforms);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = mDescriptorSets[i];
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(mCtx->device(), 1, &write, 0, nullptr);
  }
}

void Renderer::createPipeline() {
  // Viewport/scissor dinamik: pencere boyutu değişince pipeline yeniden
  // derlenmez, sadece record sırasında güncellenir.
  VkShaderModule vert = loadShader("pbr.vert.spv");
  VkShaderModule frag = loadShader("pbr.frag.spv");

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";

  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = sizeof(Vertex);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attrs[3]{};
  attrs[0].location = 0; // inPosition
  attrs[0].binding = 0;
  attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrs[0].offset = offsetof(Vertex, pos);
  attrs[1].location = 1; // inNormal
  attrs[1].binding = 0;
  attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrs[1].offset = offsetof(Vertex, normal);
  attrs[2].location = 2; // inTexCoord
  attrs[2].binding = 0;
  attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
  attrs[2].offset = offsetof(Vertex, texCoord);

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &binding;
  vertexInput.vertexAttributeDescriptionCount = 3;
  vertexInput.pVertexAttributeDescriptions = attrs;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_BACK_BIT; // artık gerçek 3D sahne + depth var
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_TRUE;
  depth.depthWriteEnable = VK_TRUE;
  depth.depthCompareOp = VK_COMPARE_OP_LESS;

  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blendAttachment;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamicStates;

  // Model matrisi + malzeme faktörleri: 88 bayt (vertex+fragment).
  VkPushConstantRange pcRange{};
  pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pcRange.offset = 0;
  pcRange.size = sizeof(ModelPush);

  VkPipelineLayoutCreateInfo layoutCi{};
  layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutCi.setLayoutCount = 1;
  layoutCi.pSetLayouts = &mDescSetLayout;
  layoutCi.pushConstantRangeCount = 1;
  layoutCi.pPushConstantRanges = &pcRange;

  if (vkCreatePipelineLayout(mCtx->device(), &layoutCi, nullptr, &mPipelineLayout) != VK_SUCCESS) {
    vkDestroyShaderModule(mCtx->device(), vert, nullptr);
    vkDestroyShaderModule(mCtx->device(), frag, nullptr);
    throw std::runtime_error("vkCreatePipelineLayout");
  }

  VkGraphicsPipelineCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci.stageCount = 2;
  ci.pStages = stages;
  ci.pVertexInputState = &vertexInput;
  ci.pInputAssemblyState = &inputAssembly;
  ci.pViewportState = &viewportState;
  ci.pRasterizationState = &raster;
  ci.pMultisampleState = &multisample;
  ci.pDepthStencilState = &depth;
  ci.pColorBlendState = &blend;
  ci.pDynamicState = &dynamic;
  ci.layout = mPipelineLayout;
  ci.renderPass = mRenderPass;
  ci.subpass = 0;

  const VkResult r = vkCreateGraphicsPipelines(mCtx->device(), VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline);

  vkDestroyShaderModule(mCtx->device(), vert, nullptr); // pipeline modülü kopyaladı
  vkDestroyShaderModule(mCtx->device(), frag, nullptr);
  if (r != VK_SUCCESS) throw std::runtime_error("vkCreateGraphicsPipelines");
}

void Renderer::createFramebuffers() {
  mFramebuffers.resize(mSwapchain.views().size());
  for (size_t i = 0; i < mSwapchain.views().size(); ++i) {
    VkImageView attachments[] = {mSwapchain.views()[i], mDepthView};

    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = mRenderPass;
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.width = mSwapchain.extent().width;
    ci.height = mSwapchain.extent().height;
    ci.layers = 1;
    if (vkCreateFramebuffer(mCtx->device(), &ci, nullptr, &mFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateFramebuffer");
    }
  }
}

void Renderer::createCommandObjects() {
  VkCommandPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = mCtx->graphicsFamily();
  if (vkCreateCommandPool(mCtx->device(), &pool, nullptr, &mCommandPool) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateCommandPool");
  }

  const uint32_t count = mSwapchain.imageCount();
  mCommands.resize(count);
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = mCommandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = count;
  if (vkAllocateCommandBuffers(mCtx->device(), &alloc, mCommands.data()) != VK_SUCCESS) {
    throw std::runtime_error("vkAllocateCommandBuffers");
  }
}

void Renderer::createSyncObjects() {
  VkSemaphoreCreateInfo sem{};
  sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT; // ilk karede bekleme anında dönsün

  // Tüm senkronizasyon nesneleri swapchain image sayısı kadar:
  // N image → N acquire semaforu, N render-bitim semaforu, N fence.
  // Böylece hiçbir nesne henüz present'ten dönmeden yeniden kullanılmaz.
  const uint32_t count = mSwapchain.imageCount();
  mImageAvailable.resize(count);
  mRenderFinished.resize(count);
  mInFlight.resize(count);

  for (uint32_t i = 0; i < count; ++i) {
    if (vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mImageAvailable[i]) != VK_SUCCESS ||
        vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mRenderFinished[i]) != VK_SUCCESS ||
        vkCreateFence(mCtx->device(), &fence, nullptr, &mInFlight[i]) != VK_SUCCESS) {
      throw std::runtime_error("senkronizasyon nesnesi olusturulamadi");
    }
  }
  mFrame = 0;
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const app::Camera& camera) {
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) throw std::runtime_error("vkBeginCommandBuffer");

  // Frame uniform'ları yaz (acquire dönmüş olmalı → UBO[imageIndex] boşta).
  const float aspect = static_cast<float>(mSwapchain.extent().width) /
                       static_cast<float>(mSwapchain.extent().height);
  FrameUniforms fu{};
  fu.viewProj = camera.getProjectionMatrix(aspect) * camera.getViewMatrix();
  fu.lightDir = glm::vec4(glm::normalize(glm::vec3(0.45f, -1.0f, -0.25f)), 0.0f);
  fu.cameraPos = glm::vec4(camera.position, 1.0f);
  std::memcpy(mFrameUboMapped[imageIndex], &fu, sizeof(fu));

  VkClearValue clears[2]{};
  clears[0].color = {{0.05f, 0.07f, 0.12f, 1.0f}}; // koyu gece mavisi — tünel havası
  clears[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = mRenderPass;
  rp.framebuffer = mFramebuffers[imageIndex];
  rp.renderArea.extent = mSwapchain.extent();
  rp.clearValueCount = 2;
  rp.pClearValues = clears;
  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = static_cast<float>(mSwapchain.extent().width);
  viewport.height = static_cast<float>(mSwapchain.extent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = mSwapchain.extent();
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1,
                          &mDescriptorSets[imageIndex], 0, nullptr);

  // Geçici istasyon: gerçek asset'ler gelene kadar aynı glTF mesh'i zemin,
  // peron ve tavan placeholder'larına dönüştürülür.
  struct SceneInstance {
    glm::mat4 transform;
    glm::vec4 color;
    float roughness;
  };
  const SceneInstance instances[] = {
      {glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.5f, 0.0f)),
                  glm::vec3(10.0f, 0.15f, 40.0f)),
       glm::vec4(0.18f, 0.20f, 0.23f, 1.0f), 0.9f},
      {glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-4.0f, -0.2f, 0.0f)),
                  glm::vec3(0.8f, 0.3f, 40.0f)),
       glm::vec4(0.25f, 0.28f, 0.32f, 1.0f), 0.75f},
      {glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, -0.2f, 0.0f)),
                  glm::vec3(0.8f, 0.3f, 40.0f)),
       glm::vec4(0.25f, 0.28f, 0.32f, 1.0f), 0.75f},
      {glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, 0.0f)),
                  glm::vec3(10.0f, 0.15f, 40.0f)),
       glm::vec4(0.12f, 0.14f, 0.18f, 1.0f), 0.95f},
      {glm::mat4(1.0f), glm::vec4(0.72f, 0.10f, 0.06f, 1.0f), 0.55f},
  };
  mModel.bind(cmd);
  for (const SceneInstance& instance : instances) {
    for (size_t i = 0; i < mModel.subMeshCount(); ++i) {
      const SubMesh& sub = mModel.subMesh(i);
      ModelPush push{};
      push.model = instance.transform;
      push.baseColor = instance.color;
      push.metallic = sub.metallic;
      push.roughness = instance.roughness;
      vkCmdPushConstants(cmd, mPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(ModelPush), &push);
      mModel.drawSubMesh(cmd, i);
    }
  }

  vkCmdEndRenderPass(cmd);
  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) throw std::runtime_error("vkEndCommandBuffer");
}

void Renderer::drawFrame(const app::Camera& camera) {
  const VkDevice dev = mCtx->device();
  const uint32_t syncCount = static_cast<uint32_t>(mInFlight.size());

  // 1) Bu karenin fence'i: önceki submit bitmiş mi?
  vkWaitForFences(dev, 1, &mInFlight[mFrame], VK_TRUE, UINT64_MAX);

  uint32_t imageIndex = 0;
  const VkResult acquired =
      vkAcquireNextImageKHR(dev, mSwapchain.handle(), UINT64_MAX, mImageAvailable[mFrame],
                            VK_NULL_HANDLE, &imageIndex);
  if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return;
  }
  if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
    METRO_ERROR("vkAcquireNextImageKHR: %d", static_cast<int>(acquired));
    return;
  }

  vkResetFences(dev, 1, &mInFlight[mFrame]);

  // 2) Komut tamponunu bu image için yeniden yaz.
  vkResetCommandBuffer(mCommands[mFrame], 0);
  recordCommandBuffer(mCommands[mFrame], imageIndex, camera);

  // 3) Submit: renk çıktısı aşamasına kadar bekle.
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &mImageAvailable[mFrame];
  submit.pWaitDstStageMask = &waitStage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &mCommands[mFrame];
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &mRenderFinished[mFrame];

  if (vkQueueSubmit(mCtx->graphicsQueue(), 1, &submit, mInFlight[mFrame]) != VK_SUCCESS) {
    throw std::runtime_error("vkQueueSubmit");
  }

  // 4) Present. handle() değer döndürdüğü için yerel değişkene al (adres
  // geçiciye bağlanmasın).
  VkSwapchainKHR swapchainHandle = mSwapchain.handle();
  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &mRenderFinished[mFrame];
  present.swapchainCount = 1;
  present.pSwapchains = &swapchainHandle;
  present.pImageIndices = &imageIndex;

  const VkResult presented = vkQueuePresentKHR(mCtx->graphicsQueue(), &present);
  if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain();
  } else if (presented != VK_SUCCESS) {
    METRO_ERROR("vkQueuePresentKHR: %d", static_cast<int>(presented));
  }

  mFrame = (mFrame + 1) % syncCount;
}

void Renderer::onResize() {
  recreateSwapchain();
}

void Renderer::destroySwapchainDependent() {
  for (VkFramebuffer fb : mFramebuffers) vkDestroyFramebuffer(mCtx->device(), fb, nullptr);
  mFramebuffers.clear();

  // Depth buffer extent'e bağlı → yık, recreate'te yeniden kurulur.
  if (mDepthView) vkDestroyImageView(mCtx->device(), mDepthView, nullptr);
  mDepthView = VK_NULL_HANDLE;
  if (mDepthImage) vmaDestroyImage(mCtx->allocator(), mDepthImage, mDepthAlloc);
  mDepthImage = VK_NULL_HANDLE;
  mDepthAlloc = VK_NULL_HANDLE;

  // Frame UBO + descriptor'lar image sayısına bağlı.
  for (size_t i = 0; i < mFrameUboBuffers.size(); ++i) {
    vmaDestroyBuffer(mCtx->allocator(), mFrameUboBuffers[i], mFrameUboAllocs[i]);
  }
  mFrameUboBuffers.clear();
  mFrameUboAllocs.clear();
  mFrameUboMapped.clear();
  mDescriptorSets.clear();
  if (mDescriptorPool) vkDestroyDescriptorPool(mCtx->device(), mDescriptorPool, nullptr);
  mDescriptorPool = VK_NULL_HANDLE;

  // Tüm senkronizasyon nesneleri ve command buffer'ları yık.
  for (auto& s : mImageAvailable) if (s) vkDestroySemaphore(mCtx->device(), s, nullptr);
  for (auto& s : mRenderFinished) if (s) vkDestroySemaphore(mCtx->device(), s, nullptr);
  for (auto& f : mInFlight) if (f) vkDestroyFence(mCtx->device(), f, nullptr);
  mImageAvailable.clear();
  mRenderFinished.clear();
  mInFlight.clear();

  if (!mCommands.empty()) {
    vkFreeCommandBuffers(mCtx->device(), mCommandPool, static_cast<uint32_t>(mCommands.size()), mCommands.data());
    mCommands.clear();
  }
}

void Renderer::recreateSwapchain() {
  vkDeviceWaitIdle(mCtx->device());
  destroySwapchainDependent();

  mSwapchain.recreate(*mCtx, mWindow);

  const uint32_t count = mSwapchain.imageCount();
  mImageAvailable.resize(count);
  mRenderFinished.resize(count);
  mInFlight.resize(count);

  VkSemaphoreCreateInfo sem{};
  sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < count; ++i) {
    if (vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mImageAvailable[i]) != VK_SUCCESS ||
        vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mRenderFinished[i]) != VK_SUCCESS ||
        vkCreateFence(mCtx->device(), &fence, nullptr, &mInFlight[i]) != VK_SUCCESS) {
      throw std::runtime_error("senkronizasyon nesnesi olusturulamadi (recreate)");
    }
  }

  mCommands.resize(count);
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = mCommandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = count;
  if (vkAllocateCommandBuffers(mCtx->device(), &alloc, mCommands.data()) != VK_SUCCESS) {
    throw std::runtime_error("vkAllocateCommandBuffers (recreate)");
  }

  mFrame = 0;

  // Derinlik + frame UBO'ları yeni image sayısı/extent'e göre yeniden kur.
  createDepthResources();
  createFrameUniforms();

  // Format değişmediyse pipeline kalmaya devam eder (viewport dinamik).
  if (mSwapchain.format() != mSwapFormat) {
    METRO_WARN("Swapchain formati degisti (%u -> %u); render pass + pipeline yenileniyor",
               static_cast<unsigned>(mSwapFormat), static_cast<unsigned>(mSwapchain.format()));
    vkDestroyPipeline(mCtx->device(), mPipeline, nullptr);
    mPipeline = VK_NULL_HANDLE;
    vkDestroyRenderPass(mCtx->device(), mRenderPass, nullptr);
    mRenderPass = VK_NULL_HANDLE;
    createRenderPass();
    createPipeline();
  }
  createFramebuffers();
}

void Renderer::shutdown() {
  if (mCtx == nullptr || mCtx->device() == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(mCtx->device());

  destroySwapchainDependent();

  if (mCommandPool) vkDestroyCommandPool(mCtx->device(), mCommandPool, nullptr);
  mCommandPool = VK_NULL_HANDLE;

  mModel.destroy(*mCtx);

  if (mPipeline) vkDestroyPipeline(mCtx->device(), mPipeline, nullptr);
  mPipeline = VK_NULL_HANDLE;
  if (mPipelineLayout) vkDestroyPipelineLayout(mCtx->device(), mPipelineLayout, nullptr);
  mPipelineLayout = VK_NULL_HANDLE;
  if (mDescSetLayout) vkDestroyDescriptorSetLayout(mCtx->device(), mDescSetLayout, nullptr);
  mDescSetLayout = VK_NULL_HANDLE;
  if (mRenderPass) vkDestroyRenderPass(mCtx->device(), mRenderPass, nullptr);
  mRenderPass = VK_NULL_HANDLE;

  mSwapchain.destroy(*mCtx);
}

} // namespace metro::rhi
