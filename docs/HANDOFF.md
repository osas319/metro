# METRO PROJESİ — HANDOFF DÖKÜMANI (devralan AI için)

Bu doküman, projenin şu anki tam durumunu özetler. Son güncelleme: 2026-09-13
(modelleme organizasyonu oturumu — bkz. §11).

## 1. PROJE TANIMI
- İstanbul M4 metro hattı (Kadıköy–Sabiha Gökçen) simülasyonu, SIFIRDAN oyun motoru ile.
- Hazır motor YOK. Vulkan 1.3 + native Wayland (XWayland fallback istenmiyor). C++20.
- Kullanıcı kararı: HEDEFTÜM istasyonlar ama asset pipeline N-istasyonlu tasarlanacak;
  İLK istasyon benim seçimimle **Kadıköy** (en büyük/karakteristik mekan).
- Bağımlılık yönetimi: **git submodule** (vcpkg yok). Konteyner: **rootless Podman**,
  Wayland socket mount. KATI KURAL: **host sistemeye hiçbir şey kurulmaz/dokunulmaz,
  her şey podman içinde**.

## 2. HOST ORTAMI (sabit gerçekler)
- Fedora 44, kernel 7.2.4, x86_64. Kullanıcı: genc (uid 1000, wheel grubu; render
  grubunda DEĞİL ama /dev/dri/renderD128'e ACL ile rw erişimi var — test edildi OK).
- GPU: AMD RX 5600 XT (navi10) → RADV, Mesa 26.1.8, Vulkan 1.4.354.
- Wayland: WAYLAND_DISPLAY=wayland-0, XDG_RUNTIME_DIR=/run/user/1000.
- Podman 5.8.4 rootless, git 2.55. Lokale Türkçe; kod yorumları Türkçe yazılıyor.

## 3. REPO DURUMU (`osas319/metro`, `main`)
- Proje değişiklikleri doğrudan `main` dalına aktarılıyor; son doğrulanmış paket HDR/ACES tonemap geçişidir.
- Submodule'ler eklendi (pinli): third_party/volk, third_party/VulkanMemoryAllocator,
  third_party/glm. (EnTT, Jolt, cgltf vb. SONRAKI fazlarda eklenecek.)
- Çalışma dizininde tüm kaynak hazır; derleme yeşil.

## 4. DOSYA ENVANTERİ
- CMakeLists.txt — CMake 3.28, C++20, volk+glm+SDL3 (CONFIG yoksa pkgconfig fallback),
  VMA include, VK_NO_PROTOTYPES + VMA_VULKAN_VERSION=1003000 define'ları,
  METRO_SHADER_DIR=build/shaders, METRO_VALIDATION opsiyonu (default ON).
- cmake/Shaders.cmake — GLSL→SPIR-V; glslc varsa onu, yoksa glslangValidator kullanır;
  `metro_shaders` custom target; exe buna bağımlı.
- Containerfile — localhost/metro-dev:44 imajı. Fedora 44 + gcc-c++, cmake, ninja-build,
  git, ccache, **glslc + glslang** (DİKKAT: Fedora 44'te "glslang-tools" paketi YOK),
  libdecor, mesa-vulkan-drivers, SDL3-devel (3.4.0!), vulkan-loader-devel, vulkan-tools,
  vulkan-validation-layers.
- tools/container.sh — tek giriş noktası: image / configure / build / shell / run /
  vulkaninfo. Run argümanları: --userns=keep-id, --security-opt label=disable,
  --device /dev/dri, wayland socket mount, SDL_VIDEODRIVER=wayland, repo /work'e mount.
- src/core/Log.hpp — bağımsız renkli logger + METRO_ASSERT.
- src/rhi/VmaImpl.cpp — VMA'nın tek implementation TU'su (VMA_IMPLEMENTATION + volk.h önce).
- src/rhi/VulkanContext.{hpp,cpp} — volkInitialize → instance (SDL uzantıları + opsiyonel
  validation + debug messenger) → surface (SDL_Vulkan_CreateSurface, SDL3 4-arg versiyonu)
  → fiziksel cihaz seçimi (skor: discrete+4, iGPU+2; şartlar: api≥1.2, tek family'de
  graphics+present, swapchain ext, format/present mode >0) → logical device
  (samplerAnisotropy+fillModeNonSolid, volkLoadDevice) → VMA allocator.
- src/rhi/Swapchain.{hpp,cpp} — SRGB+nonlinear format tercihi, FIFO present, extent:
  currentExtent 0xFFFFFFFF/0 ise SDL_GetWindowSizeInPixels, composite-alpha fallback
  (opaque yoksa pre-multiplied→post→inherit), min+1 image, recreate/destroy API.
- src/rhi/Renderer.{hpp,cpp} — render pass (1 renk eki, UNDEFINED→PRESENT_SRC,
  external color-output dependency), grafik pipeline (dynamic viewport/scissor),
  vertex buffer (VMA host-visible+mapped, 3 köşe), framebuffer'lar, command pool +
  2 primary cmd buffer, 2 frame-in-flight semaphore/fence seti, drawFrame akışı
  (fence wait → acquire → record → submit → present; OUT_OF_DATE/SUBOPTIMAL →
  recreateSwapchain; format değişirse renderpass+pipeline de yeniden kurulur),
  METRO_AUTO_EXIT için hazır shutdown yolu.
- src/app/Application.{hpp,cpp} — SDL_Init → pencere (VULKAN|RESIZABLE|HIGH_PIXEL_DENSITY,
  1280x720) → context+renderer init → döngü (event pump, minimize'da sleep, resize →
  onResize, 2 sn'de bir fps log, METRO_AUTO_EXIT=<sn> sonunda temiz çıkış) → shutdown
  (vkDeviceWaitIdle + sıralı yıkım).
- src/main.cpp — try/catch giriş noktası.
- shaders/pbr.vert / pbr.frag — sahne PBR shader'ları; `tonemap.vert` /
  `tonemap.frag` HDR görüntüyü ACES filmic eğriyle swapchain'e indirger.
- README.md — mimari karar tablosu, Forward+ gerekçe taslağı, klasör yapısı, asset
  konvansiyonları (1 birim=1 m, +Y up, sağ-el/glTF; assets/stations/<ad>/meshes|
  materials|textures|lightmaps + station.json manifest; LOD0/1/2 stratejisi), yol haritası.
- .gitignore — build/, build-*/, .cache/, IDE dosyaları.

## 5. DOĞRULANMIŞ OLARAK ÇALIŞANLAR
- `tools/container.sh image` → imaj kuruldu (paket adı düzeltmesi sonrası).
- `tools/container.sh vulkaninfo` → konteyner içinden GPU0: RADV NAVI10 (api 1.4.354),
  GPU1: llvmpipe. Cihaz seçimi discrete'i öne aldığı için RADV seçilecek.
- `tools/container.sh configure` + `build` → BUILD YEŞİL.
- Çalıştırma (`METRO_AUTO_EXIT=N tools/container.sh run`): Wayland driver, validation
  aktif, RADV, swapchain 1280x720/4 image, box.glb yükleme (V:24 I:36), PBR pipeline
  (Cook-Torrance + depth buffer + frame UBO/descriptor + per-submesh malzeme
  push-constant), ~77 FPS, tek VVL uyarısı yok, temiz çıkış.

## 7. ÖĞRENİLEN SDL 3.4 API FARKLARI (Fedora 44 SDL3-devel 3.4.0)
- SDL_Vulkan_GetInstanceExtensions artık pencere ALMAZ: `const char* const*
  SDL_Vulkan_GetInstanceExtensions(Uint32* count)` — statik liste döndürür.
- SDL_Vulkan_GetDrawableSize YOK → SDL_GetWindowSizeInPixels(window, &w, &h).
- SDL_Vulkan_CreateSurface(window, instance, allocator, &surface) — 4 argümanlı.
- Application.hpp'de SDL_Event için ileri bildirim namespace içinde yanlış çözülür;
  SDL3/SDL.h doğrudan include edildi.

## 8. DİKKAT EDİLECEK TUZAKLAR
- Dosya üretirken Türkçe yorum satırlarına 2 kez yabancı (CJK) karakter karıştı ve
  derleme hatası verdi; temizlendi. Yeni yazılan dosyalarda garip karakter olup olmadığını
  kontrol et (grep -P '[\x{4E00}-\x{9FFF}]' src -r gibi).
- SDL3 CONFIG paketi bulunduğu için PkgConfig fallback devrede değil; SDL3::SDL3 linked.
- CMake Vulkan find_package'ı sadece header/loader kontrolü; loader'a link YOK (volk dlopen).

## 9. SIRADAKİ ADIMLAR (öncelik sırasıyla)

Aşamalar 0–4 (pencere → Vulkan → kamera/glTF/PBR → Jolt + Kadıköy prosedürel
sahne → tren/sinyal/kapı → yolcu/ses/tonemap) **tamamlandı**. Kalan işler:

1. Render kalitesi: CSM, SSAO, bloom, TAA (HDR hedefi ve ACES tonemap hazır;
   bu geçişler sahne ve post-process arasına girer).
2. Sahne sistemini gerçek ECS'e taşı (EnTT şu an yalnızca `Application::init`
   içinde varlık oluşturup loglayan bir duman testi; render hâlâ `Renderer`
   içindeki elle yerleştirilmiş placeholder geometriden geliyor).
3. Kadıköy mimari asset üretimi (MODELING.md §16, adım M1'den başlar).
4. `station.json` şemasının asset alanlarıyla genişletilmesi: mesh listesi,
   LOD bağlantıları, malzeme/doku referansları. `StationManifest` + `Renderer`
   kod değişikliği gerektirir; onay bekliyor (MODELING.md §17/3).
5. Ses backend'i şu an SDL3 prosedürel bildirim tonu üretiyor; gerçek ses
   asset'leri ve 3D pozisyonel ses açık iş.

## 10. KULLANICININ ÇALIŞMA KURALLARI (bozma)
- Her aşamada kararların kısa gerekçesini açıkla; kod modüler ve yorumlu olsun.
- Kapsamı asla sessizce büyütme; büyük sistem (tam sinyal ağı, çok istasyon) öncesi onay iste.
- CMake yapılandırmasını güncel tut; derlenebilir kod teslim et.
- Ana sistemeye DOKUNMA — her şey podman konteynerinde.

## 11. SON OTURUM — modelleme organizasyonu (2026-09-13)

Kapsam sessizce büyütülmedi: **hiçbir motor kodu ve hiçbir manifest değeri
değiştirilmedi.** Eklenenler yalnızca doküman, dizin iskelesi ve host
tarafında çalışan bir doğrulayıcı:

- `docs/MODELING.md` — Kadıköy pilotu için tam boru hattı: ölçek/eksen
  konvansiyonu, klasör yapısı, `<kod>_<parça>_lodN` adlandırma şeması ve
  23 durağın istasyon kodları, modüler kit, LOD bütçeleri/geçiş mesafeleri,
  Jolt `_col` stratejisi, navmesh planı, PBR doku/UV kuralları, Blender→glTF
  export checklist, saha ölçümü prosedürü, İBB açık veri kullanımı, M1–M7
  üretim planı ve onay bekleyen 7 karar.
- `assets/stations/kadikoy/{meshes,materials,textures,lightmaps,navmesh,audio}`
  + `reference/` ve `assets/shared/{meshes,materials,textures}` iskelesi
  (her biri `.gitkeep` ile; git boş dizin tutmaz).
- `assets/stations/kadikoy/reference/measurements.csv` — sahada doldurulacak
  34 satırlık ölçüm defteri (`kaynak` ve `guven` kolonlarıyla).
- `tools/validate_station_assets.sh` — manifest alanlarını, bildirilen
  dizinleri, `model`/`navmesh` yollarının varlığını (yoksa uygulama
  açılmıyor), mesh ad şemasını, LOD0↔LOD1/2 ve `_col` bütünlüğünü, doku
  boyut eki/KTX2 kuralını denetler. Hata varsa çıkış kodu 1.

Doğrulananlar: script `bash -n` temiz; gerçek Kadıköy manifestinde **0 hata,
0 uyarı**; kasıtlı bozuk bir fixture ile 4 hata + 7 uyarı ve ikinci turda
"dizin yok" yolu tetiklendi (fixture sonra silindi).

### Önemli bulgular

- **Hat ekseni −Z**: `Renderer` hattı `-routeLength * 0.5` merkezine kuruyor
  ve treni `-trainPosition` ile ilerletiyor. Yani Kadıköy `z = 0`, ileri
  hareket −Z. Modelleme yönü buna göre sabitlendi.
- **Gerçek ölçek ile placeholder çelişkisi**: M4 gerçekte 33,5 km / 23
  istasyon (uçtan uca 52 dk, azami 80 km/sa), ama manifest 2000 m ve 87 m
  eşit aralıklı durak listesi kullanıyor; `track_gauge = 2.4` ise standart
  açıklık 1,435 m yerine geçici. Üçü de onay bekleyen listede.

### Çalışma alanı uyarısı (bu kopya)

Bu çalışma alanı (`İndirilenler/metro-main`) **git deposu değil** ve
`third_party/` altındaki altı submodule dizini **boş** (volk, glm, VMA,
Jolt, entt, cgltf). Bu nedenle burada `cmake` configure/derleme çalışmaz;
host'ta yalnızca `podman` var (cmake/ninja/glslc imajın içinde). Derleme
doğrulaması yapmak için submodule'lerin yeniden klonlanması gerekir.
Doğrulanan çalışma ağacı: `osas319/metro` `main`.
