#include "rhi/Swapchain.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>

#include "core/Log.hpp"
#include "rhi/VulkanContext.hpp"

namespace metro::rhi {

void Swapchain::create(const VulkanContext& ctx, SDL_Window* window) {
  createInternal(ctx, window);
  METRO_INFO("Swapchain: %ux%u, %u image, fmt=%u (B8G8R8A8/R8G8B8A8 SRGB tercih edilir)",
             mExtent.width, mExtent.height, imageCount(), static_cast<unsigned>(mFormat));
}

void Swapchain::recreate(const VulkanContext& ctx, SDL_Window* window) {
  destroy(ctx);
  createInternal(ctx, window);
  METRO_INFO("Swapchain yeniden kuruldu: %ux%u, %u image", mExtent.width, mExtent.height, imageCount());
}

void Swapchain::destroy(const VulkanContext& ctx) {
  for (VkImageView v : mViews) vkDestroyImageView(ctx.device(), v, nullptr);
  mViews.clear();
  mImages.clear();
  if (mSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(ctx.device(), mSwapchain, nullptr);
    mSwapchain = VK_NULL_HANDLE;
  }
}

void Swapchain::createInternal(const VulkanContext& ctx, SDL_Window* window) {
  VkSurfaceKHR surface = ctx.surface();
  VkPhysicalDevice pd = ctx.physicalDevice();

  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps);

  // --- Format: SRGB + nonlinear gamma birincil tercih ---
  uint32_t fmtCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(fmtCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmtCount, formats.data());
  VkSurfaceFormatKHR chosen = formats[0];
  for (const auto& f : formats) {
    const bool srgbFormat = f.format == VK_FORMAT_B8G8R8A8_SRGB ||
                            f.format == VK_FORMAT_R8G8B8A8_SRGB;
    if (srgbFormat && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = f;
      break;
    }
  }
  mFormat = chosen.format;

  // --- Extent ---
  // Wayland WSI'da currentExtent çoğu zaman pencere boyutudur; 0xFFFFFFFF
  // "uygulama belirlesin", 0 ise "henüz configure edilmedi" demektir.
  if (caps.currentExtent.width != 0xFFFFFFFF && caps.currentExtent.width != 0) {
    mExtent = caps.currentExtent;
  } else {
    // SDL3'te piksel cinsinden drawable boyutu bu verir (HiDPI ölçekli).
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    mExtent.width = static_cast<uint32_t>(std::max(1, w));
    mExtent.height = static_cast<uint32_t>(std::max(1, h));
  }
  mExtent.width = std::clamp(mExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
  mExtent.height = std::clamp(mExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

  // --- Image sayısı: min+1, max ile kırp ---
  uint32_t imageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
    imageCount = caps.maxImageCount;
  }

  // --- Present mode: FIFO (v-sync; Wayland'da her zaman destekli) ---
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

  // --- Composite alpha: Wayland'da opaque desteklenmiyorsa ilk desteklenen ---
  VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  const VkCompositeAlphaFlagsKHR supported = caps.supportedCompositeAlpha;
  if ((supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0) {
    const VkCompositeAlphaFlagBitsKHR candidates[] = {
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto c : candidates) {
      if (supported & c) { alpha = c; break; }
    }
  }

  VkSwapchainCreateInfoKHR ci{};
  ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  ci.surface = surface;
  ci.minImageCount = imageCount;
  ci.imageFormat = chosen.format;
  ci.imageColorSpace = chosen.colorSpace;
  ci.imageExtent = mExtent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // tek queue family
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = alpha;
  ci.presentMode = presentMode;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(ctx.device(), &ci, nullptr, &mSwapchain) != VK_SUCCESS) {
    METRO_ERROR("vkCreateSwapchainKHR basarisiz");
    return;
  }

  uint32_t count = 0;
  vkGetSwapchainImagesKHR(ctx.device(), mSwapchain, &count, nullptr);
  mImages.resize(count);
  vkGetSwapchainImagesKHR(ctx.device(), mSwapchain, &count, mImages.data());

  mViews.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = mImages[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = mFormat;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx.device(), &view, nullptr, &mViews[i]) != VK_SUCCESS) {
      METRO_ERROR("vkCreateImageView (image %u) basarisiz", i);
      return;
    }
  }
}

} // namespace metro::rhi
