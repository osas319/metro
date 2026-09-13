cmake_minimum_required(VERSION 3.28)

project(Metro
  VERSION 0.1.0
  DESCRIPTION "İstanbul M4 metro simülasyonu — özel motor (Vulkan + Wayland)"
  LANGUAGES C CXX)

include(CTest)

# ---------------------------------------------------------------------------
# Dil ve derleyici politikaları
# C++20: derleyici desteği (GCC 13+) her yerde olgun; C++23 gereksinimimiz
# henüz yok (std::expected gibi şeyler gerekirse yükseltilir).
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(METRO_VALIDATION "Vulkan validation layer iste (geliştirme)" ON)

# ---------------------------------------------------------------------------
# third_party (git submodule; sürümler .gitmodules ile pinlenir)
# ---------------------------------------------------------------------------
add_subdirectory(third_party/volk)   # hedef: volk (Vulkan loader'ı runtime'da yükler)
add_subdirectory(third_party/glm)    # hedef: glm::glm
# VMA header-only: include yolu aşağıda; tek implementation TU'si
# src/rhi/VmaImpl.cpp içinde (VMA_IMPLEMENTATION).

# ---------------------------------------------------------------------------
# SDL3 — önce CMake config, yoksa pkg-config (Fedora SDL3-devel ikisini de
# taşıyabilir; hangisi bulunursa).
# ---------------------------------------------------------------------------
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  set(METRO_SDL_TARGET SDL3::SDL3)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
  set(METRO_SDL_TARGET PkgConfig::SDL3)
endif()

# Sadece vulkan header'larının varlığını doğrula; loader'ı volk runtime'da
# dlopen eder, link etmeyiz.
find_package(Vulkan REQUIRED)

# ---------------------------------------------------------------------------
# Shader'lar: GLSL -> SPIR-V (glslc veya glslangValidator, hangisi varsa)
# ---------------------------------------------------------------------------
include(cmake/Shaders.cmake)
metro_add_shader_target(
  shaders/pbr.vert
  shaders/pbr.frag)

# ---------------------------------------------------------------------------
# Uygulama
# ---------------------------------------------------------------------------
file(GLOB_RECURSE METRO_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(metro ${METRO_SOURCES})

target_include_directories(metro PRIVATE
  src
  third_party
  third_party/entt/src
  third_party/VulkanMemoryAllocator/include)

target_compile_definitions(metro PRIVATE
  # volk: loader prototype'lerini gizle, tüm vk* çağrıları volk'un global
  # fonksiyon işaretçilerinden gitsin (VMA da bunları kullanır).
  VK_NO_PROTOTYPES
  VMA_VULKAN_VERSION=1003000
  METRO_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders")

if(METRO_VALIDATION)
  target_compile_definitions(metro PRIVATE METRO_ENABLE_VALIDATION)
endif()

target_link_libraries(metro PRIVATE
  volk
  glm::glm
  ${METRO_SDL_TARGET})

# SPIR-V'ler derlenmeden exe kurulmasın
add_dependencies(metro metro_shaders)

if(BUILD_TESTING)
  add_executable(metro_sim_tests
    tests/sim_tests.cpp
    src/sim/Train.cpp
      src/sim/Signal.cpp
      src/sim/PassengerSystem.cpp
      src/sim/Route.cpp)
  target_include_directories(metro_sim_tests PRIVATE src)
  add_test(NAME metro_sim_tests COMMAND metro_sim_tests)
endif()
