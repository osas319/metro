# M4 İstasyon Modelleme ve Asset Boru Hattı

Bu doküman, istasyon mimarisinin **fotogrametri/elle modellemeden motora** kadar
izleyeceği yolu tanımlar. Pilot istasyon **Kadıköy**'dür; kurallar N istasyonlu
bir hattı (23 durak) kaldıracak şekilde tasarlandı, tek istasyona özel kısayol
yok.

Kaynak seviyesi etiketleri:

- **[DOĞRULANDI]** — kaynağı gösterilmiş resmî/ölçülmüş değer.
- **[PLACEHOLDER]** — prototipi ayakta tutan geçici değer, gerçek ölçüm bekliyor.
- **[TBD]** — henüz ölçülmedi; saha ölçümü veya kaynak toplama ile doldurulacak.

---

## 1. Karar özeti

| Konu | Karar | Gerekçe |
|---|---|---|
| Pilot istasyon | **Kadıköy** | Hattın en büyük ve en karakteristik mekanı; LOD, sinyal ve kalabalık senaryolarını en çok zorlayan istasyon |
| Üretim yöntemi | **Manuel Blender + saha ölçümü** | Temiz topoloji, kontrollü UV, doğru ölçek ve LOD ayrımı doğrudan modelciden çıkar; fotogrametri retopoloji borcu yaratır |
| Fotogrametri | **İkincil** | Sadece pürüzlülük/normal detay ve kaba kütle doğrulaması için; ana üretim yolu değil |
| Ölçek | 1 motor birimi = 1 metre | glTF 2.0 ve Blender ile dönüşümsüz |
| Eksen | +Y yukarı, sağ-el, **hat ekseni −Z** | Motor konvansiyonu; bkz. §3 |

Manuel modelleme seçildiği için boru hattının kritik darboğazı **saha ölçümü
disiplinidir**: ölçülmemiş hiçbir duvar modellenmez (§12).

---

## 2. Gerçek hat verisi

| Parametre | Değer | Kaynak |
|---|---|---|
| Hat uzunluğu | 33,5 km **[DOĞRULANDI]** | Metro İstanbul M4 hat detayı |
| İstasyon sayısı (işletmede) | 23 **[DOĞRULANDI]** | Metro İstanbul M4 hat detayı |
| Uçtan uca seyahat süresi | 52 dk **[DOĞRULANDI]** | Wikipedia — M4 (İstanbul Metrosu) |
| Azami hız | 80 km/sa **[DOĞRULANDI]** | Wikipedia M4 hat künyesi |
| Ray açıklığı | **1,435 mm** standart açıklık **[DOĞRULANDI]** | Wikipedia M4 hat künyesi |
| Elektrifikasyon | **1.500 V DC havai hat** (üçüncü ray değil) **[DOĞRULANDI]** | Wikipedia M4 hat künyesi |
| Araç filosu | 144 vagon = 36 adet **4'lü set** **[DOĞRULANDI]** | Metro İstanbul M4 hat detayı |
| Araç üreticisi | **CAF** (İspanyol) 4'lü setler **[DOĞRULANDI]** | Urban Transport Magazine, Ekim 2022 |
| Ortalama istasyon aralığı | 33,5 km ÷ 22 ≈ **1,52 km** (hesaplandı) | Türetilmiş |
| Gerçek aralık örnekleri | Kadıköy–Ayrılık Çeşmesi **1.148 m**, Ayrılık Çeşmesi–Acıbadem **1.287 m** **[DOĞRULANDI]** | NAVITIME M4 hat verisi (kalan 21 aralık bu kaynaktan toplanacak) |

> **Düzeltme (özgün brifing varsayımı):** Brifingde tren fiziğinin "Alstom
> Metropolis benzeri" araçlara göre ayarlanması istenmiş. Hat gerçekte **CAF
> üretimi 4'lü setlerle** işletiliyor ve **1.500 V DC havai hat** ile besleniyor.
> Yani çekiş/fren eğrileri CAF aracına göre kalibre edilmeli; havai hat,
> tünel kesiti ve pantograf boşluğu modellemesini de etkiler. Fizik
> parametrelerini değiştirmeden önce karar onayınızı istiyorum.

### Motorda düzeltilmesi gereken placeholder'lar

Bunlar **kasıtlı olarak değiştirilmedi**; sahne ölçeğini büyütmek ayrı bir karar
(§17). `assets/stations/kadikoy/station.json`:

| Alan | Şu an | Olması gereken | Etki |
|---|---|---|---|
| `route_length` | 33500.0 **[DOĞRULANDI]** | 33500.0 | Gerçek hat ölçeği |
| `stop` konumları | İlk iki aralık doğrulanmış; kalanlar yaklaşık **[TBD]** | Saha/NAVITIME verisiyle gerçek chainage | Sinyal/blok ve varış frenleme mesafesi |
| `track_gauge` | 1.435 **[DOĞRULANDI]** | 1.435 | Ray ve araç ölçeği |
| `platform_width` | 4.0 **[PLACEHOLDER]** | Saha ölçümü **[TBD]** | Peron geometrisi (gerçek ada peronları tipik olarak belirgin biçimde daha geniş) |

> Not: `stop` listesi ve `route_length` değiştirilmeden gerçek mimari mesh
> yüklenmez — aksi hâlde peron uzunluğu ile durak konumları çelişir.

---

## 3. Dünya konvansiyonları

Bu blok motordaki mevcut davranışla birebir hizalıdır; modelci bunları
sorgulamadan uygular.

- **Birim:** 1 birim = 1 metre. Blender sahnesi metrik, `Unit Scale = 1.0`,
  `Separate Units` kapalı.
- **Yukarı eksen:** +Y (glTF 2.0 native). Blender glTF export'unda `+Y Up`
  işaretli kalır; motorda eksen dönüşümü **yapılmaz**.
- **Hat ekseni:** **−Z ileri**. `Renderer` tüm hat gövdesini
  `-routeLength * 0.5` merkezine yerleştirir ve treni `-trainPosition` ile
  ilerletir. Yani:
  - Kadıköy durağı `z = 0`,
  - hat −Z yönüne uzanır, trenin ileri hareketi **−Z**,
  - peron uzunluğu Z ekseni boyunca serilir (−Z'ye doğru).
- **Ray konumu:** ray merkezi `x = 0`; iki ray `x = ±track_gauge/2`.
- **Peronlar:** hat merkezine göre simetrik, `x = ±platform_width`.
- **Yolcu/navgraph düzlemi:** düğümler `(x, z)` çifti ile tanımlı; Y zeminden
  ölçülür.
- **Köken (origin) kuralı:** her istasyonun kendi glTF'i **kendi peron merkezine
  göre yerel** modellenir. İstasyonun hattaki konumu `station.json` içindeki
  `platform.edge_position` / `stop_position` ile verilir; böylece 23 istasyon
  aynı yerel uzayda üretilip hattaki yeri veriyle belirlenir.
- **Grid:** modelleme 0,25 m ızgarada; yapısal modüller 1,0 m katı.
  Normalleştirme öncesi tüm vertex'ler 1 mm'ye yuvarlanır (z-fighting ve
  birleştirme sorunlarını önler).

---

## 4. Klasör yapısı

```
assets/
  stations/
    kadikoy/
      station.json            # manifest — motorun okuduğu tek giriş noktası
      meshes/                 # glTF/GLB parçaları (LOD dahil)
      materials/              # malzeme tanımları (gerekirse .json/.mtlx)
      textures/               # KTX2/Basis dokular
      lightmaps/              # baked ışık haritaları (HDR)
      navmesh/                # yaya grafiği (mevcut: düğüm listesi, hedef: Recast)
      audio/                  # istasyona özel sesler (anons, ortam)
      reference/              # KADEME DIŞI kaynak veri: ölçüm defteri, notlar
  shared/                     # istasyonlar arası ortak kit (bank, turnike, tabela)
    meshes/ materials/ textures/
```

Kurallar:

- `reference/` motor tarafından **okunmaz**; telif/ölçüm kaynaklarının izini
  sürer. Büyük foto arşivi repoda tutulmaz (§13, Git LFS).
- İki istasyonun dokusunu ayırmak gerekmiyorsa `shared/` altındaki kit
  kullanılır; istasyona özgü olan her şey istasyon klasöründe kalır.
- Yeni istasyon eklemek = klasör kopyalama + `station.json` yazma; motor
  tarafında kod değişikliği gerekmez (manifest alanları veri odaklı).

---

## 5. Adlandırma konvansiyonu

Her istasyon 3 harfli bir kod alır ve bu kod **asset adlarının ön eki**dir.
Kodlar bir kez sabitlenir (sonradan yeniden adlandırma tüm referansları kırar).

Önerilen kodlar (23 durak — sabitlenmesi onay bekliyor):

| Durak | Kod | Durak | Kod | Durak | Kod |
|---|---|---|---|---|---|
| Kadıköy | `kdk` | Bostancı | `bsn` | Yakacık-Adnan Kahveci | `yak` |
| Ayrılık Çeşmesi | `ayc` | Küçükyalı | `kck` | Pendik | `pnd` |
| Acıbadem | `acb` | Maltepe | `mlp` | Tavşantepe | `tvs` |
| Ünalan | `unl` | Huzurevi | `hzr` | Fevzi Çakmak-Hastane | `fck` |
| Göztepe | `gzt` | Gülsuyu | `gls` | Yayalar-Şehitler | `yys` |
| Yenisahra | `ysh` | Esenkent | `esk` | Kurtköy | `krk` |
| Kozyatağı | `kzy` | Hastane-Adliye | `hsa` | Sabiha Gökçen | `sgh` |
| | | Soğanlık | `sgl` | | |
| | | Kartal | `krt` | | |

Şema: `<kod>_<parça>[_<varyant>][_lodN][_col].glb`

| Tür | Kural | Örnek |
|---|---|---|
| Mesh (LOD'lu) | `<kod>_<parça>_lod0..2.glb` | `kdk_platform_floor_lod0.glb` |
| Mesh (LOD'suz, tek kullanımlık) | `<kod>_<parça>.glb` | `kdk_stairs_north.glb` |
| Collision | `<kod>_<parça>_col.glb` | `kdk_platform_floor_col.glb` |
| Ortak kit mesh | `sh_<parça>_lodN.glb` | `sh_bench_01_lod0.glb` |
| Doku (PBR) | `<ad>_<tür>_<boyut>.ktx2` | `kdk_floor_tiles_basecolor_2k.ktx2` |
| Doku türleri | `basecolor`, `normal`, `roughness`, `metallic`, `ao`, `emissive`, `orm` | `kdk_column_orm_2k.ktx2` |
| Lightmap | `<kod>_<grup>_lightmap_<boyut>.exr` | `kdk_platform_lightmap_2k.exr` |
| Ses | `<kod>_<olay>_<varyant>.ogg` | `kdk_announce_arrival_01.ogg` |

Kurallar: tamamı **küçük harf**, Türkçe karakter yok (ASCII), boşluk yerine
alt çizgi, sürüm son eki `_v01`. Manifest'te görünen isimlerle dosya adı
**birebir** aynı olmalı — doğrulayıcı bunu denetler (§15).

---

## 6. Modüler kit ve ölçek disiplini

İstasyon, elle modellenmiş tek bir dev mesh **değil**, tekrar eden modüllerden
kurulur. Bu, hem 23 istasyona ölçeklenmeyi hem LOD'u hem de değişiklik
maliyetini düşürür.

- **Modül ızgarası:** 1,0 m. Peron kenarı, kolon aralığı, tavan paneli, kapı
  modülü bu ızgaranın katı.
- **Tekrar eden modüller** (`shared/`): bank, turnike, bilet makinesi, çöp
  kutusu, yönlendirme tabelası, aydınlatma armatürü, acil çıkış kapısı,
  kamera, yangın dolabı.
- **İstasyona özgü hacimler:** peron plakası, tavan, kolon düzeni, merdiven/
  yürüyen merdiven boşlukları, çıkış tünelleri, gişe holü, asansör kuyusu.
- **Kolon ve duvar kalınlığı [TBD]** — saha ölçümü ile doldurulur; kit
  modüllerinin ölçüsü ölçümden sonra dondurulur ve *tüm* istasyonlarda aynı
  kalır.
- Modelleme sırasında `track_gauge`/`platform_width` gibi manifest değerleri
  **referans** olarak alınır, ancak gerçek mimari mesh geldiğinde motor
  varsayımları yerine mesh'in kendisi geçerlidir (§17).

---

## 7. LOD stratejisi

Üç seviye: LOD0 peron içi detay, LOD1 istasyon genel hacmi, LOD2 uzak
görünüm/tünel gövdesi. Bu, README'deki mevcut stratejiyi operasyonel hâle
getirir.

| Seviye | Kapsam | Geçiş mesafesi | Üçgen bütçesi (istasyon toplamı) |
|---|---|---|---|
| LOD0 | Peron içi: yer, duvar, tavan, kolon, ekipman, tabelalar | 0–35 m | ≤ 330 k |
| LOD1 | Aynı hacim, detay ekipman basitleştirilmiş | 35–90 m | ≤ 100 k |
| LOD2 | Dış kabuk/tünel gövdesi, tabelalar hariç | 90 m + / frustum dışı | ≤ 25 k |

Kategori bazlı LOD0 bütçesi: peron kabuğu (yer+duvar+tavan+kolon) ≤ 120 k,
ekipman ≤ 150 k, tünel/çıkışlar ≤ 60 k.

Ek kurallar:

- **Silüet korunur:** LOD1/LOD2, LOD0'ın dış hattını (peron kenarı, kolon
  ritmi, tavan yüksekliği) korumak zorunda; geçişte sıçrama olmamalı.
- **LOD pivotu aynı**, transform'lar temiz; LOD'lar ayrı dosya, aynı isim
  gövdesi + `_lodN`.
- Tünel gibi doğrusal yapılar için LOD2 tekrar eden segment olarak üretilir
  (bir segment mesh'i, motor tarafında çoğaltılır).
- Occlusion culling hedefi: peron ↔ gişe holü birbirini görmemeli; LOD
  üretirken portal/bölme sınırları belgelenir.
- Her LOD için "LOD kartı" çıkarılır (üçgen sayısı, doku, geçiş mesafesi) ve
  istasyon `reference/` klasöründe tutulur.

---

## 8. Collision (Jolt) stratejisi

Render mesh'i **asla** collision olarak kullanılmaz; ayrı `_col` mesh üretilir.

- **Zemine/perona:** basit kutu döşemeleri. Trimesh'ten kaçınılır — Jolt'ta
  maliyetli ve gereksiz.
- **Kolon, bank, duvar:** convex hull.
- **Merdiven, rampa:** eğimli kutu (basamak basamak collision yok) — yaya
  ajanının hareketi için rampa tercih edilir.
- **Yürüyen merdiven:** hareketli platform ileride kinematik gövdeyle
  temsil edilecek; şimdilik statik rampa + ayrı görsel mesh.
- Tüm `_col` mesh'ler `PhysicsWorld`'ün mevcut manifest ölçülerinden
  kurduğu statik gövdelerin yerini alacak (henüz bağlanmadı, §17).
- **Tren:** gövde ≈ 2,8 × 3,2 m ölçülerinde kinematik kutu (manifest
  `train_width`/`train_height`); gerçek vagon mesh'i geldiğinde collision
  yine `_col` kutusu olarak kalır — render mesh'i değişse bile fizik sabit
  kalır.

---

## 9. Yaya grafiği (navmesh)

Mevcut durum: `PassengerNavGraph`, satır başına `x z komşu...` biçiminde bir
düğüm listesi okuyor; dosya verilmezse durak konumlarından doğrusal bir
fallback kuruluyor.

Hedef akış:

1. **Şimdi:** istasyon `navmesh/` klasörüne elle çıkarılmış düğüm grafiği
   yazmak, peron → merdiven → gişe holü bağlantılarını kurmak. Bu, gerçek
   mesh gelmeden yolcu akışını gerçek topolojiye taşır.
2. **Sonra:** Recast/Detour ile peron mesh'inden otomatik navmesh pişirme
   (`_navmesh` üretimi), düğüm grafiğinin yerini alır; kalabalık için RVO2.
3. Kural: navmesh düğümleri **peron kenarından güvenli mesafede** kalır
   (ray açıklığı + güvenlik payı); yolcular asla hattın üzerine yönlenmez.

> Uyarı: manifest'e `navmesh` alanı ancak dosya gerçekten mevcutken eklenir.
> `StationManifest::load`, dosya bulunamazsa yüklemeyi başarısız sayıyor ve
> uygulama açılmıyor.

---

## 10. Doku, UV ve malzeme kuralları

- **PBR metallic-roughness** (glTF şeması). Motordaki `SubMesh` şu an
  `baseColorFactor`, `metallic`, `roughness` taşıyor; doku haritaları
  geldiğinde aynı yapı genişletilecek.
- **UV kanalları:** UV0 = yüzey dokuları, UV1 = lightmap. Lightmap UV'si
  üretimi `xatlas` ile otomatikleştirilecek (§17).
- **Texel yoğunluğu:** büyük mimari yüzeyler (yer/duvar/tavan) 512 px/m,
  ekipman ve tabelalar 1024 px/m. Atlas boyutu 2K (4K yalnızca geniş
  duvar/yer panolarında).
- **Sıkıştırma:** KTX2 + Basis Universal; albedo/normal için UASTC ya da
  yüksek kaliteli BC7, ORM/roughness için ayrı kanal.
- **Metallik kuralı:** kereste/beton/cam gibi dielektrikler `metallic = 0`;
  paslanmaz korkuluk/travers `metallic = 1`. Emisyon yalnızca tabela ve
  armatürlerde (bloom'un girdisi budur).
- **Malzeme sayısını sınırla:** istasyon başına ≤ 40 malzeme; tekrar eden
  yüzeyler atlas ile birleştirilir. Her malzeme `materials/` altında okunabilir
  bir adla tanımlanır (`kdk_floor_tiles.json` gibi).
- **Cam:** peron camı ve korkuluk camı ayrı malzeme (ileride alpha/transparan
  geçiş gerektirecek).

---

## 11. Blender → glTF export checklist

Her export'ta birebir uygulanır:

1. Sahne birimi metrik, `Unit Scale = 1.0`, tüm nesnelerde
   `Object > Apply > All Transforms`.
2. Eksen: `+Y Up` **işaretli**; motor ham yükler, dönüşüm yok.
3. Export: glTF 2.0 Binary (`.glb`), `Apply Modifiers`, `UVs`,
   `Normals`, `Tangents` (normal map kullanan mesh'lerde),
   `Materials: Export`, `Draco: OFF` (motor Draco açmıyor).
4. Doku yolları: `textures/` altına göreli; motorun çözebileceği yollar (§15).
5. Triangle count ve isimlendirme doğrulaması: `tools/validate_station_assets.sh`.
6. Kamera/ışık nesneleri export edilmez (kamera ve ışık motordan gelir).
7. Her LOD ayrı `.glb`; aynı isim gövdesi + `_lodN` son eki.
8. Export sonrası mesh'i motor içinde yükleyip ölçek ve yön kontrolü yapılır:
   peron kenarı ile ray merkezi arasındaki mesafe ölçümle uyuşmalı.

---

## 12. Saha ölçümü prosedürü

Manuel modelleme seçildiği için ölçüm **tek gerçek kaynak**tır.

- **Araçlar:** lazerli mesafe ölçer (±2 mm), 2 m çelik şerit metre, nivo
  seviyesi, telefon + tripod, ölçek çubuğu (renkli, 1 m), not defteri
  (`reference/measurements.csv`).
- **Yöntem:** istasyonu bölgelere ayır (peron, tavan, gişe holü, merdivenler,
  çıkışlar). Her bölge için referans bir köşe seç ve tüm ölçüleri **o köşeye
  göre** kaydet — böylece Blender'da tek noktadan inşa edilir.
- **Zorunlu ölçümler:** peron genişliği, peron yüksekliği (ray üstünden),
  tavan yüksekliği, kolon en/boy aralığı, kapı modülü genişliği, basamak
  ölçüleri, tünel kesiti, tabela konum ve ölçüleri, ekipman konumları.
- **Her ölçü için kaynak ve güven yazılır:** `measured` / `photo-scaled` /
  `estimated`. `estimated` satırlar modele girmeden önce teyit edilir.
- Fotoğraf: aynı fokal uzunluk (tercihen 24 mm eşdeğeri), geniş örtüşme
  (%70), sabit beyaz dengesi, ölçek çubuğu her sahnede görünür (§13 için
  fotogrametri opsiyonunu açık tutar).

Ölçüm defteri şablonu: `assets/stations/kadikoy/reference/measurements.csv`.

---

## 13. Referans veri ve lisans

- Fotoğraf ve ölçüm notları `reference/` altında tutulur; **bunlar kaynak
  veridir, motor asset'i değildir** ve derlemeye girmez.
- Büyük foto arşivi ve fotogrametri çıktıları için **Git LFS** kullanılacak
  (henüz kurulmadı, §17). O zamana kadar arşiv repo dışında tutulur ve
  `reference/README.md` içinde yolu ve tarihi yazılır.
- Kamusal alanda çekim yapılırken kişilerin okunabilir yüzü kullanılmaz;
  fotogrametri için insan kalabalığının olmadığı saatler tercih edilir.
- Metro İstanbul'a ait logo/tabela grafikleri için kullanım kapsamı netleşene
  kadar **nötr/placeholder tabela** üretilir; gerçek marka grafikleri ayrı
  karar (dağıtım biçimine bağlı).

---

## 14. İBB açık veri kullanımı

`data.ibb.gov.tr` üzerinden alınacaklar yalnızca **referans ve doğrulama**
amaçlıdır — mimari çizim içermez:

- İstasyon adı, ilçe, hat konumu → `station.json` alanları ve durak sırası.
- Günlük/yolcu sayıları → kalabalık simülasyonunun saatlik dağılımı için
  girdi (biniş/iniş oranları).
- Yürüyen merdiven/asansör sayıları → ekipman yerleşimi ve erişilebilirlik
  rotası senaryoları.
- Ölçek doğrulama: toplam hat uzunluğu ve istasyon aralıkları, resmî
  değerlerle karşılaştırılıp `route_length`/`stop` konumları güncellenir.

Çıkarılacak sayısal veri `reference/` altında kaynağı ve erişim tarihiyle
saklanır.

---

## 15. Doğrulama

Otomatik: `tools/validate_station_assets.sh [istasyon-dizini]`

Denetledikleri:

- manifest okunabilir; `name`, `line`, `meshes`, `materials`, `textures`,
  `lightmaps` alanları dolu,
- manifest'teki tüm dizinler mevcut,
- `model` ve `navmesh` yolları gerçekten var (yoksa uygulama açılmıyor),
- mesh adları §5 şemasına uyuyor, `_lod0` olan her parçanın `_lod1`
  karşılığı var (yoksa uyarı),
- LOD0 mesh'lerin `_col` karşılığı var (yoksa uyarı),
- doku adlarında `_1k/_2k/_4k` boyut eki var,
- ASCII dışı karakter içeren asset adı yok.

Manuel kontrol listesi (her istasyon tesliminde):

1. Mesh ölçeği: peron kenarı–ray merkezi mesafesi ölçümle uyuşuyor mu?
2. Hat ekseni −Z mi? (Tren ileri hareketi −Z olmalı.)
3. LOD geçişleri gözle fark ediliyor mu? Silüet korunuyor mu?
4. Tüm ekipman collision'ı var mı? Yolcular duvardan geçiyor mu?
5. Malzeme sayısı ≤ 40 mı? Metallic değerleri fiziksel mi?
6. Tabela metinleri gerçek mi, placeholder mı? (Karar §13.)
7. Validation layer uyarısı olmadan 5 sn çalışıyor mu?
   `METRO_AUTO_EXIT=5 tools/container.sh run`

---

## 16. Kadıköy üretim planı

Teslim sırası — her adım bir sonrakinin girdisi:

| Adım | Çıktı | Not |
|---|---|---|
| M1 | Ölçüm defteri + referans foto seti | Modellemeden önce tamamlanır |
| M2 | Peron kabuğu: yer, duvar, tavan, kolon (LOD0/1/2) | İlk motor entegrasyonu; ölçek doğrulaması burada yapılır |
| M3 | Düşey sirkülasyon: merdivenler, yürüyen merdiven, asansör | Collision rampaları dâhil |
| M4 | Gişe holü + çıkışlar, turnikeler, bilet makineleri | Navgraph düğümleri bu adımda gerçek topolojiye bağlanır |
| M5 | Ekipman ve tabelalar (shared kit + istasyon özel) | Malzeme atlasları dondurulur |
| M6 | Lightmap pişirme + emisyon ayarı | Baked GI; CSM ile birlikte değerlendirilecek |
| M7 | Tren gövdesi LOD0/1/2 + `_col` | Araç modeli ayrı iş kalemi |

Her adım sonunda `station.json` güncellenir ve §15 doğrulaması koşulur.

---

## 17. Onay bekleyen kararlar / sonraki adımlar

Bunlar bu turda **yapılmadı** — kapsam büyümesi oldukları için onay gerekiyor:

1. `route_length` 2000 → 33500 ve gerçek chainage'lı durak listesi (§2).
   Sahne ölçeğini ve tren dinamiğini doğrudan değiştirir. Gerçek aralık
   verisi NAVITIME M4 hat verisinden toplanabilir (ilk iki aralık doğrulandı).
2. `track_gauge` 2.4 → 1.435 (§2). Ray/araç ölçeğini değiştirir.
3. `station.json` şemasının asset alanlarıyla genişletilmesi: mesh listesi,
   LOD bağlantıları, malzeme/doku referansları, collider eşleşmeleri.
   `StationManifest` ve `Renderer` kod değişikliği gerektirir.
4. Git LFS kurulumu ve `reference/` arşiv politikası (§13).
5. Araç zinciri eklemeleri: KTX2/Basis (`basisu`), lightmap UV (`xatlas`),
   navmesh (`Recast/Detour`), tavan gerektiren ölçümler için kalabalık (RVO2).
6. Yaya grafiği: elle düğüm grafiği mi, önce Recast mi? (§9)
7. İstasyon kodlarının sabitlenmesi (§5) — sonradan değiştirmek pahalı.
