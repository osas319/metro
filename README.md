# Metro — İstanbul M4 Hattı Simülasyonu

Sıfırdan yazılmış bir oyun motoru (Vulkan 1.3 + native Wayland) üzerinde,
Kadıköy–Sabiha Gökçen (M4) metro hattının gerçekçi simülasyonu.

## Mimari kararlar (kısaca)

| Karar | Seçim | Gerekçe |
|---|---|---|
| Dil | C++20 | Toolchain olgun; C++23'e özgü gereksinim henüz yok, gerektiğinde yükseltilir |
| Grafik API | Vulkan 1.3, `VK_KHR_wayland_surface` | Modern PBR pipeline, kesin kaynak kontrolü; SDL3 Wayland'ı native açar (XWayland yok) |
| Loader | volk | Ekstra .so bağımlılığı olmadan loader'ı runtime'da yükleme |
| Bellek | VMA | Vulkan bellek ayırma/tahsisat yönetimi |
| Matematik | GLM | Vulkan/GLSL ile birebir konvansiyon |
| Pencere/Girdi | SDL3 | Native Wayland + libdecor desteği, olgun girdi soyutlaması |
| Derleme | CMake + Ninja, GLSL→SPIR-V build adımı | Konteyner içinde bağımsız, tekrarlanabilir derleme |

Render pipeline (ileride): **Forward+**. Gerekçe: Tünel/istasyon sahneleri
çok sayıda ışık içerir ama opak geometri ağırlıklı; forward+ tek geçişte
TAA/SSAO gibi post-process zinciriyle en az bellek çöplüğüyle çalışır,
transparan malzemeler (peron camları) deferred'da ikinci bir G-Buffer
hilesi gerektirir. Karar uygulamaya geçerken ayrıntılı gerekçeyle yeniden
değerlendirilecek.

## Geliştirme ortamı (Podman — host'a hiçbir şey kurulmaz)

```bash
tools/container.sh image        # geliştirme imajını derle (bir kez)
tools/container.sh vulkaninfo   # container içinden GPU görünür mü? (RADV)
tools/container.sh configure    # cmake configure (Debug)
tools/container.sh build        # derle
tools/container.sh run          # çalıştır
tools/container.sh shell        # container içinde bash
```

Test/CI koşusu için: `METRO_AUTO_EXIT=5 tools/container.sh run` → uygulama
5 saniye sonra kendini kapatır (kafasız doğrulama için).

Varsayılan örnek model yerine başka bir glTF/GLB yüklemek için:
`METRO_MODEL_PATH=assets/stations/kadikoy/meshes/platform.glb tools/container.sh run`.

Kamera: pencereye tıklayıp fareyi yakalayın; **WASD** yatay hareket, **Q/E**
dikey hareket, **Shift** hızlı hareket, **ESC** fare yakalamayı bırakır.

## Klasör yapısı

```
src/
  main.cpp            giriş noktası
  core/               log, assert, temel yardımcılar
  app/                pencere, ana döngü, uygulama durumu
  rhi/                Vulkan soyutlaması (context, swapchain, renderer)
shaders/              GLSL kaynakları (build'de SPIR-V'ye derlenir)
assets/               istasyon/modeller/dokular (pipeline olgunlaşınca)
third_party/          git submodule'ler (volk, VMA, GLM, ...)
tools/                geliştirme araçları (container.sh, ...)
cmake/                CMake yardımcı modülleri
```

## Asset ölçüleri ve sahne konvansiyonları

- **Birim:** 1 engine birimi = 1 metre. **Eksen:** +Y yukarı, sağ-el
  (glTF 2.0 native) — Blender'dan glTF export alırken eksen dönüşümü
  yapılmaz, motor glTF'i ham yükler.
- **İstasyon düzeni:** `assets/stations/<istasyon-adı>/` altında
  `meshes/`, `materials/`, `textures/`, `lightmaps/` + `station.json`
  manifest (peron kenarı, durma noktası, asansör/merdiven konumları).
- **LOD stratejisi:** LOD0 = peron içi detay (kiosk, tabelalar, banklar),
  LOD1 = istasyon genel hacim, LOD2 = tünel/dış görünüm basitleştirilmiş.
  Tren: LOD0 iç+dış, LOD1 dış kabuk, LOD2 uzak görünüm.
  Ayrıntılı modelleme planı modelleme aşamasına geçerken dokümanlanacak.

## Durum / yol haritası

- [x] Aşama 0: SDL3 + Wayland pencere → Vulkan instance/device/swapchain → üçgen
- [x] Aşama 1: kamera + glTF mesh yükleme + PBR (metallic-roughness)
- [ ] Aşama 2: Jolt fiziği + Kadıköy istasyonu sahnesi
- [ ] Aşama 3: tren fiziği, sinyal/blok sistemi, kapı/peron mantığı
- [ ] Aşama 4: yolcu simülasyonu (navmesh kalabalık), ses, tam hat
