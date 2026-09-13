#pragma once
// Vulkan instance + fiziksel/v mantıksal cihaz seçimi + kuyruk + VMA.
//
// volk kullanımı: tüm hedefte VK_NO_PROTOTYPES tanımlıdır; vk* çağrıları
// volk'un yüklediği global fonksiyon işaretçilerine gider. Loader .so'suna
// link yoktur — volk runtime'da dlopen eder.
#include <volk.h>
#include <vk_mem_alloc.h>

#include <cstdint>

struct SDL_Window;

namespace metro::rhi {

class VulkanContext {
public:
  // Hata durumunda false döner (ayrıntı log'lanmıştır).
  bool init(SDL_Window* window);
  void shutdown();

  VkInstance instance() const { return mInstance; }
  VkSurfaceKHR surface() const { return mSurface; }
  VkPhysicalDevice physicalDevice() const { return mPhysicalDevice; }
  VkDevice device() const { return mDevice; }
  VmaAllocator allocator() const { return mAllocator; }
  VkQueue graphicsQueue() const { return mGraphicsQueue; }
  uint32_t graphicsFamily() const { return mGraphicsFamily; }
  const char* deviceName() const { return mDeviceName; }
  bool validationEnabled() const { return mValidation; }

private:
  bool createInstance(SDL_Window* window, uint32_t apiVersion);
  void createSurface(SDL_Window* window);
  bool pickPhysicalDevice();
  bool createLogicalDevice();

  VkInstance mInstance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR mSurface = VK_NULL_HANDLE;
  VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
  VkDevice mDevice = VK_NULL_HANDLE;
  VkQueue mGraphicsQueue = VK_NULL_HANDLE;
  VmaAllocator mAllocator = VK_NULL_HANDLE;
  uint32_t mGraphicsFamily = UINT32_MAX;
  char mDeviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};
  bool mValidation = false;
};

} // namespace metro::rhi
