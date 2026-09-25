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
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/JoltPhysics/Build jolt-build EXCLUDE_FROM_ALL)
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
# Dear ImGui — editor tooling (docking + SDL3 + Vulkan)
# Pinned docking release so editor builds stay reproducible.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9-docking
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

add_library(metro_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(metro_imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  third_party/volk)
target_compile_definitions(metro_imgui PRIVATE
  IMGUI_IMPL_VULKAN_USE_VOLK
  VK_NO_PROTOTYPES)
target_link_libraries(metro_imgui PUBLIC
  volk
  ${METRO_SDL_TARGET})

# ---------------------------------------------------------------------------
# Shader'lar: GLSL -> SPIR-V (glslc veya glslangValidator, hangisi varsa)
# ---------------------------------------------------------------------------
include(cmake/Shaders.cmake)
metro_add_shader_target(
  shaders/pbr.vert
  shaders/pbr.frag
  shaders/tonemap.vert
  shaders/tonemap.frag)

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
  METRO_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
  IMGUI_IMPL_VULKAN_USE_VOLK)

if(METRO_VALIDATION)
  target_compile_definitions(metro PRIVATE METRO_ENABLE_VALIDATION)
endif()

target_link_libraries(metro PRIVATE
  volk
  glm::glm
  Jolt
  ${METRO_SDL_TARGET}
  metro_imgui)

# SPIR-V'ler derlenmeden exe kurulmasın
add_dependencies(metro metro_shaders)

if(BUILD_TESTING)
  add_executable(metro_sim_tests
    tests/sim_tests.cpp
    src/sim/Train.cpp
      src/sim/Signal.cpp
      src/sim/PassengerSystem.cpp
        src/sim/PassengerAgent.cpp
        src/sim/Route.cpp
        src/sim/PhysicsWorld.cpp
          src/sim/PassengerNavGraph.cpp
            src/audio/AudioEventQueue.cpp
            src/app/StationManifest.cpp)
  target_include_directories(metro_sim_tests PRIVATE src)
  target_link_libraries(metro_sim_tests PRIVATE glm::glm Jolt)
  add_test(NAME metro_sim_tests COMMAND metro_sim_tests)
  set_tests_properties(metro_sim_tests PROPERTIES
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
