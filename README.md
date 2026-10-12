# Metro — İstanbul M4 Simülasyonu

Vulkan + SDL3 + C++20 ile geliştirilen, sürüş odaklı İstanbul M4 metro simülasyonu.
Linux/Wayland ve Windows 10/11 x64 hedeflenir.

## Hedef


- sürücü kabini ve serbest kamera
- 4 vagonlu tren seti
- çekiş, fren ve otomatik duruş
- kapı animasyonu ve istasyon duruşu
- blok sinyalizasyonu
- yolcu hareket altyapısı
- PBR + HDR + ACES tonemapping
- SDL3 platform penceresi (Linux/Wayland ve Windows)
- veri odaklı istasyon manifestleri
- gerçek asset pipeline'ına geçiş için glTF/GLB desteği

## Mimari

- **Rendering:** Vulkan 1.3, Volk, VMA, GLM
- **Window/Input:** SDL3 / Wayland / Win32
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

### Windows 10/11

Visual Studio 2022'de **Desktop development with C++** iş yükünü, Git'i ve CMake'i
kurun. Vulkan uyumlu ekran kartı sürücüsü gerekir. Bağımlılıklar vcpkg manifestiyle
otomatik yüklenir; SDL3'ün Vulkan desteği, Vulkan başlıkları/yükleyicisi ve `glslc`
shader derleyicisi projeyle eşleşen paketlerden gelir.

PowerShell'de bir kez vcpkg'yi hazırlayın:

```powershell
git clone https://github.com/microsoft/vcpkg.git "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
```

Ardından repo kökünde Visual Studio Developer PowerShell açıp:

```powershell
git submodule update --init --recursive
cmake --preset windows-msvc
cmake --build --preset windows-release --parallel
.\build\windows-msvc\Release\metro.exe
```

Uygulamayı repo kökünden başlatın; istasyon, model ve doku dosyaları göreli
yollarla yüklenir.

### Linux / Wayland

Host sisteme bağımlılık kurmak yerine geliştirme konteyneri kullanılır:

```bash
./tools/container.sh image
./tools/container.sh configure
./tools/container.sh build
./tools/container.sh test
./tools/container.sh run
```

Wayland çalıştırması için araç scripti oturumun Wayland socket'ini ve `/dev/dri` GPU erişimini konteynere aktarır.

## Sürüş

Uygulama doğrudan Kadıköy'den Sabiha Gökçen'e M4 seferini sürüş kamerasında başlatır.
Oyuncu 23 durağı takip eder; sinyal, hız tavsiyesi, kapılar ve yolcu hareketi HUD'da
görünür. Durak panolarındaki numara ve renk şeridi, tünelde yaklaşan istasyonu ayırt
etmeye yardımcı olur. Terminale varınca sefer tamamlandı bildirimi çıkar.

- `W` / `↑`: çekiş
- `S` / `↓`: servis freni
- `Space`: acil fren
- `O` / `C`: kapı aç / kapa
- `F1` / `F2` / `F3`: kabin / takip / serbest kamera
- `F4`: editöre geçiş
- `F5`: sürüşe dön
- `F6`: seferi Kadıköy'den yeniden başlat
- `H`: korna

## Durum

Motor ve temel simülasyon altyapısı çalışıyor. Aktif geliştirme sırası gerçek istasyon
assetleri, gelişmiş ışıklandırma/post-process, daha doğru tren geometrisi/fiziği,
sesler ve istasyonlar arası gerçek hat verileridir.
