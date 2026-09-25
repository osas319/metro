# Metro — İstanbul M4 Simülasyonu

Vulkan + native Wayland + C++20 ile geliştirilen, sürüş odaklı İstanbul M4 metro simülasyonu.

## Hedef


- sürücü kabini ve serbest kamera
- 4 vagonlu tren seti
- çekiş, fren ve otomatik duruş
- kapı animasyonu ve istasyon duruşu
- blok sinyalizasyonu
- yolcu hareket altyapısı
- PBR + HDR + ACES tonemapping
- native Wayland
- veri odaklı istasyon manifestleri
- gerçek asset pipeline'ına geçiş için glTF/GLB desteği

## Mimari

- **Rendering:** Vulkan 1.3, Volk, VMA, GLM
- **Window/Input:** SDL3 / Wayland
- **Physics:** Jolt Physics
- **ECS:** EnTT
- **Models:** cgltf / glTF 2.0
- **Build:** CMake + Ninja
- **Development:** rootless Podman

## Proje yapısı

`src/rhi` Vulkan renderer'ı, `src/sim` tren/sinyal/fizik/yolcu simülasyonunu,
`src/app` uygulama ve istasyon manifestini, `assets/stations` istasyon assetlerini,
`shaders` GLSL shaderlarını içerir.

## Geliştirme

Host sisteme bağımlılık kurmak yerine geliştirme konteyneri kullanılır:

```bash
./tools/container.sh image
./tools/container.sh configure
./tools/container.sh build
./tools/container.sh test
./tools/container.sh run
```

Wayland çalıştırması için araç scripti oturumun Wayland socket'ini ve `/dev/dri` GPU erişimini konteynere aktarır.

## Durum

Motor ve temel simülasyon altyapısı çalışıyor. Aktif geliştirme sırası gerçek istasyon
assetleri, gelişmiş ışıklandırma/post-process, daha doğru tren geometrisi/fiziği,
sesler ve istasyonlar arası gerçek hat verileridir.
