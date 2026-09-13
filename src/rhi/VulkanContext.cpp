#include "rhi/VulkanContext.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <vector>

#include "core/Log.hpp"

namespace metro::rhi {

// Validation layer mesajlarını seviyeye göre yönlendir.
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*types*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/) {
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    METRO_ERROR("[VVL] %s", data->pMessage);
  } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    METRO_WARN("[VVL] %s", data->pMessage);
  } else {
    METRO_INFO("[VVL] %s", data->pMessage);
  }
  return VK_FALSE; // çağrıyı iptal etme, sadece raporla
}

bool VulkanContext::init(SDL_Window* window) {
  if (volkInitialize() != VK_SUCCESS) {
    METRO_ERROR("volkInitialize basarisiz — Vulkan loader bulunamadi");
    return false;
  }

  const uint32_t supportedApi = volkGetInstanceVersion();
  // Hedef 1.3; eski sürücülerde desteklenen sürüme düş ve uyar.
  if (supportedApi < VK_API_VERSION_1_3) {
    METRO_WARN("Vulkan %u.%u destekleniyor (1.3 alti) — beklenmeyen davranis olabilir",
               VK_VERSION_MAJOR(supportedApi), VK_VERSION_MINOR(supportedApi));
  }
  const uint32_t requestedApi =
      supportedApi >= VK_API_VERSION_1_3 ? VK_API_VERSION_1_3 : supportedApi;

  if (!createInstance(window, requestedApi)) return false;
  volkLoadInstance(mInstance); // instance seviyesi vk* işaretçileri

  if (mValidation) {
    VkDebugUtilsMessengerCreateInfoEXT dbg{};
    dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg.pfnUserCallback = debugCallback;
    if (vkCreateDebugUtilsMessengerEXT(mInstance, &dbg, nullptr, &mDebugMessenger) == VK_SUCCESS) {
      METRO_INFO("Debug utils messenger aktif");
    } else {
      METRO_WARN("Debug utils messenger kurulamadi");
    }
  }

  createSurface(window);
  if (mSurface == VK_NULL_HANDLE) return false;
  if (!pickPhysicalDevice()) return false;
  if (!createLogicalDevice()) return false;

  // VMA, VK_NO_PROTOTYPES (volk) ortamında fonksiyon işaretçilerini
  // kendisi çözemez; volkLoadDevice sonrası volk'un global tablosundan besle.
  // vmaCreateAllocator yapıyı kopyalar, lokal değişken yeterli.
  VmaVulkanFunctions vmaFuncs{};
  vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vmaFuncs.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo vma{};
  vma.physicalDevice = mPhysicalDevice;
  vma.device = mDevice;
  vma.instance = mInstance;
  vma.vulkanApiVersion = requestedApi;
  vma.pVulkanFunctions = &vmaFuncs;
  if (vmaCreateAllocator(&vma, &mAllocator) != VK_SUCCESS) {
    METRO_ERROR("vmaCreateAllocator basarisiz");
    return false;
  }

  METRO_INFO("GPU: %s | queue family %u (grafik+present)", mDeviceName, mGraphicsFamily);
  return true;
}

bool VulkanContext::createInstance(SDL_Window* window, uint32_t apiVersion) {
  // SDL, Wayland surface için gerekli uzantıları bildirir
  // (VK_KHR_surface + VK_KHR_wayland_surface). SDL 3.x'te bu liste
  // pencereden bağımsız statik bir dizidir.
  uint32_t extCount = 0;
  const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
  if (sdlExts == nullptr) {
    METRO_ERROR("SDL_Vulkan_GetInstanceExtensions: %s", SDL_GetError());
    return false;
  }
  std::vector<const char*> extensions(sdlExts, sdlExts + extCount);

  const char* validationLayer = "VK_LAYER_KHRONOS_validation";
#ifdef METRO_ENABLE_VALIDATION
  // Layer kuruluysa iste; kurulu değilse sessizce devam et.
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> layers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
  for (const auto& l : layers) {
    if (std::strcmp(l.layerName, validationLayer) == 0) {
      mValidation = true;
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      METRO_INFO("Validation layer bulundu, etkinlestiriliyor");
      break;
    }
  }
  if (!mValidation) {
    METRO_WARN("METRO_VALIDATION acik ama VK_LAYER_KHRONOS_validation kurulu degil");
  }
#endif

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "Metro M4";
  app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app.pEngineName = "MetroEngine";
  app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app.apiVersion = apiVersion;

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &app;
  ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  ci.ppEnabledExtensionNames = extensions.data();
  if (mValidation) {
    ci.enabledLayerCount = 1;
    ci.ppEnabledLayerNames = &validationLayer;
  }

  if (vkCreateInstance(&ci, nullptr, &mInstance) != VK_SUCCESS) {
    METRO_ERROR("vkCreateInstance basarisiz");
    return false;
  }
  METRO_INFO("VkInstance olusturuldu (%u uzanti, validation=%s)",
             ci.enabledExtensionCount, mValidation ? "acik" : "kapali");
  return true;
}

void VulkanContext::createSurface(SDL_Window* window) {
  // SDL3: ayırıcı ile (null) oluştur; Wayland'da xdg_surface'a bağlanır.
  if (!SDL_Vulkan_CreateSurface(window, mInstance, nullptr, &mSurface)) {
    METRO_ERROR("SDL_Vulkan_CreateSurface: %s", SDL_GetError());
    mSurface = VK_NULL_HANDLE;
  }
}

bool VulkanContext::pickPhysicalDevice() {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
  if (count == 0) {
    METRO_ERROR("Vulkan cihazi yok (container'da /dev/dri erisimi var mi?)");
    return false;
  }
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

  int bestScore = -1;
  for (VkPhysicalDevice dev : devices) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);
    if (props.apiVersion < VK_API_VERSION_1_2) continue;

    // Grafik + present destekleyen queue family ara (tek family yeter).
    std::vector<VkQueueFamilyProperties> fams;
    uint32_t famCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &famCount, nullptr);
    fams.resize(famCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &famCount, fams.data());
    uint32_t chosen = UINT32_MAX;
    for (uint32_t i = 0; i < famCount; ++i) {
      VkBool32 present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mSurface, &present);
      if ((fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && present == VK_TRUE) {
        chosen = i;
        break;
      }
    }
    if (chosen == UINT32_MAX) continue;

    // Swapchain uzantısı ve sunulabilir yüzey kapasitesi şart.
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());
    bool hasSwapchain = false;
    for (const auto& e : exts) {
      if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
        hasSwapchain = true;
        break;
      }
    }
    if (!hasSwapchain) continue;
    uint32_t fmtCount = 0, pmCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, mSurface, &fmtCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, mSurface, &pmCount, nullptr);
    if (fmtCount == 0 || pmCount == 0) continue;

    // Discrete > integrated > diğerleri; 8K doku desteği küçük bonus.
    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 4;
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 2;
    if (props.limits.maxImageDimension2D > 8192) score += 1;

    if (score > bestScore) {
      bestScore = score;
      mPhysicalDevice = dev;
      mGraphicsFamily = chosen;
      std::strncpy(mDeviceName, props.deviceName, sizeof(mDeviceName) - 1);
    }
  }

  if (mPhysicalDevice == VK_NULL_HANDLE) {
    METRO_ERROR("Uygun Vulkan cihazi bulunamadi");
    return false;
  }
  return true;
}

bool VulkanContext::createLogicalDevice() {
  float priority = 1.0f;
  VkDeviceQueueCreateInfo queue{};
  queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue.queueFamilyIndex = mGraphicsFamily;
  queue.queueCount = 1;
  queue.pQueuePriorities = &priority;

  const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkPhysicalDeviceFeatures features{};
  features.samplerAnisotropy = VK_TRUE; // PBR doku filtresi
  features.fillModeNonSolid = VK_TRUE;  // wireframe debug görünümü

  VkDeviceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  ci.queueCreateInfoCount = 1;
  ci.pQueueCreateInfos = &queue;
  ci.enabledExtensionCount = 1;
  ci.ppEnabledExtensionNames = deviceExtensions;
  ci.pEnabledFeatures = &features;

  if (vkCreateDevice(mPhysicalDevice, &ci, nullptr, &mDevice) != VK_SUCCESS) {
    METRO_ERROR("vkCreateDevice basarisiz");
    return false;
  }
  volkLoadDevice(mDevice); // cihaz seviyesi vk* işaretçileri (vkCreateSwapchainKHR vb.)
  vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mGraphicsQueue);
  return true;
}

void VulkanContext::shutdown() {
  if (mAllocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(mAllocator);
    mAllocator = VK_NULL_HANDLE;
  }
  if (mDevice != VK_NULL_HANDLE) {
    vkDestroyDevice(mDevice, nullptr);
    mDevice = VK_NULL_HANDLE;
  }
  if (mDebugMessenger != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
    mDebugMessenger = VK_NULL_HANDLE;
  }
  if (mSurface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    mSurface = VK_NULL_HANDLE;
  }
  if (mInstance != VK_NULL_HANDLE) {
    vkDestroyInstance(mInstance, nullptr);
    mInstance = VK_NULL_HANDLE;
  }
}

} // namespace metro::rhi
