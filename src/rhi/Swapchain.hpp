#pragma once
// Swapchain: yüzey formatı, extent, image/view yönetimi ve yeniden kurulum.
#include <volk.h>

#include <cstdint>
#include <vector>

struct SDL_Window;

namespace metro::rhi {

class VulkanContext;

class Swapchain {
public:
  void create(const VulkanContext& ctx, SDL_Window* window);
  // waitIdle çağrılmış varsayımıyla: eskisini yıkıp yenisini kurar.
  void recreate(const VulkanContext& ctx, SDL_Window* window);
  void destroy(const VulkanContext& ctx);

  VkSwapchainKHR handle() const { return mSwapchain; }
  VkFormat format() const { return mFormat; }
  VkExtent2D extent() const { return mExtent; }
  const std::vector<VkImage>& images() const { return mImages; }
  const std::vector<VkImageView>& views() const { return mViews; }
  uint32_t imageCount() const { return static_cast<uint32_t>(mImages.size()); }

private:
  void createInternal(const VulkanContext& ctx, SDL_Window* window);

  VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
  VkFormat mFormat = VK_FORMAT_UNDEFINED;
  VkExtent2D mExtent{};
  std::vector<VkImage> mImages;
  std::vector<VkImageView> mViews;
};

} // namespace metro::rhi
