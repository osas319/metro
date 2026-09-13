#pragma once
// Uygulama: SDL3 penceresi + ana döngü + olay işleme.
// Render detayları Renderer'a, Vulkan kurulumu VulkanContext'e bırakılır.
#include "rhi/Renderer.hpp"
#include "rhi/VulkanContext.hpp"

#include "app/Camera.hpp"
#include <SDL3/SDL.h>

namespace metro::app {

class Application {
public:
  // Başarıda 0; hata kodları 1 (init), 2 (istisna) döner.
  int run();

private:
  bool init();
  void shutdown();
  void handleEvent(const SDL_Event& e);
  void update(float dt);

  SDL_Window* mWindow = nullptr;
  rhi::VulkanContext mContext;
  rhi::Renderer mRenderer;

  bool mRunning = true;
  bool mResized = false;
  bool mMinimized = false;
  
  // Kamera ve Girdi
  Camera mCamera;
  bool mMouseCaptured = false;
  const bool* mKeyboardState = nullptr;
};

} // namespace metro::app
