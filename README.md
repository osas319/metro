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
tools/container.sh test         # simülasyon çekirdeği testleri
tools/container.sh run          # çalıştır
tools/container.sh shell        # container içinde bash
```

Test/CI koşusu için: `METRO_AUTO_EXIT=5 tools/container.sh run` → uygulama
5 saniye sonra kendini kapatır (kafasız doğrulama için).

Varsayılan örnek model yerine başka bir glTF/GLB yüklemek için:
`METRO_MODEL_PATH=assets/stations/kadikoy/meshes/platform.glb tools/container.sh run`.

İstasyon manifestini değiştirmek için `METRO_STATION_MANIFEST` kullanılabilir;
varsayılan değer `assets/stations/kadikoy/station.json` dosyasıdır. Başlatma,
manifestteki temel alanları doğrular ve eksik/okunamayan dosyada durur.
Örnek kutu asset'ini görmek için başlangıç kamerası kutunun 3 metre önünde
konumlanır; pencereye tıklayıp fareyi yakaladıktan sonra WASD ile gezilebilir.
Gerçek istasyon modelleri eklenene kadar renderer bu mesh'i zemin, iki peron
kenarı, raylar ve taşıyıcı kolonlar olarak çoğaltır; mavi nesne tren
placeholder'ıdır.
Tren placeholder'ı simülasyondaki konumunu ray ekseninde takip eder; Yukarı
okla çekiş verildiğinde sahnede ilerler, Aşağı okla frenlenir.
Placeholder koridoru manifestteki 2000 metrelik prototip rotayı kapsar.
Fare yakalanmadığında kamera treni takip eder; pencereye tıklamak serbest
kameraya geçiş yapar.

Kamera: pencereye tıklayıp fareyi yakalayın; **WASD** yatay hareket, **Q/E**
dikey hareket, **Shift** hızlı hareket, **ESC** fare yakalamayı bırakır.

Tren prototipi: **Yukarı ok** çekiş, **Aşağı ok** servis freni uygular.
Model 0-80 km/saat aralığında hızlanır ve konumu metre cinsinden ilerletir;
bu kontrol modeli Jolt'un kinematik tren gövdesine fixed-step içinde senkronlanır.
Jolt dünyası zemin ve peron çarpışma hacimlerini de taşır; ray kısıtları ve
gerçek dinamik tren gövdesi sonraki fizik alt adımında eklenecektir.
Tren dururken **O** kapıları açar, **C** kapatır; kapılar açıkken çekiş
uygulanmaz.
Terminale ulaşan tren otomatik durur ve kapılarını açar; yolcu transferi
terminal durağında kendiliğinden başlar.

İstasyon durağında kapılar açıkken yolcular yarım saniyede bir biner; kapasite
tren başına 320 yolcuyla sınırlıdır. Bu geçici sistem ileride navmesh ve
istasyon hedefleriyle genişletilecektir.
Kapasite `station.json` içindeki `passenger_capacity` alanından değiştirilebilir.
Başlangıçta peronda bekleyen yolcu sayısı `initial_waiting_passengers` ile,
renderer modeli ise `model` alanıyla belirlenebilir. `model` için göreli yollar
proje kökünden çözülür; `METRO_MODEL_PATH` tanımlıysa manifest değerini geçersiz
kılar.
Placeholder peron genişliği `platform_width` alanından metre cinsinden okunur;
zemin, peron ve tavan geometrisi `route_length` boyunca otomatik uzatılır.
Ray açıklığı `track_gauge` alanıyla ayarlanabilir; raylar hattın merkezine göre
simetrik yerleştirilir.
Placeholder tren gövdesi `train_width` ve `train_height` alanlarıyla ray
ölçeğine göre ayarlanabilir.
Kolonlar `column_spacing` değerine göre rota boyunca tekrarlanır.
`platform.edge_position` placeholder peron grubunun dünya konumunu,
`platform.stop_position` ise tren gövdesinin başlangıç hizasını belirler.
Blok sinyal direkleri `block_count` değerine göre rota boyunca yerleştirilir;
tren işgal ettiği blokta kırmızı, boş bloklarda yeşil gösterilir.
Terminal durağında mevcut yolcular iner ve iniş sayacı simülasyon telemetrisi
olarak tutulur.

Rota, manifestteki `stops` listesiyle veri tabanlı durak konumlarını kullanır.
Kadıköy manifesti M4 hattının Kadıköy-Sabiha Gökçen arasındaki durak adlarını
ve prototip metre konumlarını içerir. Liste verilmezse geriye dönük uyumluluk
için blok sayısından eşit aralıklı geçici duraklar üretilir.
Ara duraklarda kapılar manifestteki durak `dwell_seconds` değeri kadar açık
kalır ve otomatik kapanır; değer belirtilmezse genel varsayılan kullanılır.
Terminal durağında kapılar açık bırakılır.
Ara durak süresi `station.json` içindeki `stop_dwell_seconds` alanından
ayarlanabilir.

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

Mevcut prototipte Jolt öncesi tren hareketi, sabit zaman adımlı fizik dünyası,
kapı durumu ve 8 bloklu temel
işgal/sinyal mantığı çalışır durumdadır. Kırmızı sinyal ön blok için servis
frenini zorlar. Bunlar Faz 2-3 için geçici çekirdek
simülasyon katmanıdır; gerçek hat geometrisi ve fizik entegrasyonu geldiğinde
yerine genişletilecektir.
Fizik dünyasının `physics_fixed_step` ve `physics_max_substeps` değerleri
manifestten ayarlanabilir; değerler belirtilmezse 60 Hz ve 4 alt-adım kullanılır.
Render karesinde tren konumu, sabit adımlar arasındaki accumulator oranıyla
interpolasyonlanır; bu sayede değişken ekran yenileme hızlarında kamera ve tren
hareketi daha akıcı kalır.
Jolt Physics v5.2.0 sabit commit ile `third_party/JoltPhysics` submodule'ü
olarak bağlanır. Uygulama ve test hedefleri yalnızca Jolt çekirdek kütüphanesini
derler; örnekler, viewer ve Jolt test uygulamaları kapalıdır.
`PhysicsWorld`, her fixed-step'te Jolt `PhysicsSystem` güncellemesini çalıştırır
ve manifest ölçülerinden zemin, iki platform ve kinematik tren gövdeleri kurar.
İki rayın ve kolonların statik çarpışma gövdeleri de aynı `track_gauge`,
`route_length`, `platform_width` ve `column_spacing` ölçülerinden üretilir;
gövde oluşturma ve kapanış hataları açıkça bildirilir.

Kadıköy asset pipeline başlangıç manifesti:
`assets/stations/kadikoy/station.json`. Manifest metre birimini, glTF eksen
konvansiyonunu, asset klasörlerini ve kamera/peron başlangıç noktalarını tanımlar;
loader entegrasyonu model alanını da kullanır. `METRO_MODEL_PATH` verilirse
ortam değişkeni manifestteki model yoluna önceliklidir.
