#include "rhi/Renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/Log.hpp"
#include "rhi/VulkanContext.hpp"

namespace metro::rhi {

std::vector<Renderer::SceneInstance> Renderer::buildKadikoyScene(
    float trainPosition, float trainSpeed, float doorOpenFraction, const std::vector<bool>& occupiedBlocks,
    const std::vector<glm::vec2>& passengerPositions) const {
  std::vector<SceneInstance> instances;
  instances.reserve(1024);

  const glm::vec3 o = mPlatformEdgePosition;
  const float halfRoute = mRouteLength * 0.5f;
  // Yalnızca trenin çevresindeki statik detayları üret. Ana zemin/tünel
  // tekil kutular olarak kalır; tekrarlayan travers/kolon/aydınlatmalar
  // görünür bölgeyle sınırlandırılır. Bu, 33.5 km hatta CPU/draw-call
  // maliyetini dramatik biçimde düşürür.
  const float visibleStart = std::max(0.0f, trainPosition - 220.0f);
  const float visibleEnd = std::min(mRouteLength, trainPosition + 420.0f);
  const float visibleMinZ = -visibleEnd;
  const float visibleMaxZ = -visibleStart;

  auto addBox = [&](glm::vec3 pos, glm::vec3 scale, glm::vec4 color,
                    float roughness = 0.7f, float materialId = 0.0f) {
    SceneInstance s{};
    s.transform = glm::scale(glm::translate(glm::mat4(1.0f), o + pos), scale);
    s.color = color;
    s.roughness = roughness;
    s.materialId = materialId;
    instances.push_back(s);
  };

  // Ana tünel gövdesi: zemin, iki platform ve koyu tavan.
  addBox({0.0f, -1.5f, -halfRoute},
          {mPlatformWidth * 2.0f + 3.0f, 0.15f, halfRoute},
          {0.22f, 0.25f, 0.30f, 1.0f}, 0.92f, 1.0f);
  // İstasyon dışındaki dar servis/yürüyüş yolu.
  const float walkwayX = mTrackGauge * 0.5f + 0.60f;
  addBox({-walkwayX, -0.30f, -halfRoute},
          {0.95f, 0.22f, halfRoute},
          {0.26f, 0.28f, 0.31f, 1.0f}, 0.90f, 2.0f);
  addBox({walkwayX, -0.30f, -halfRoute},
          {0.95f, 0.22f, halfRoute},
          {0.26f, 0.28f, 0.31f, 1.0f}, 0.90f, 2.0f);
  addBox({0.0f, 4.0f, -halfRoute},
          {9.0f, 0.15f, halfRoute},
          {0.13f, 0.16f, 0.20f, 1.0f}, 0.96f);

  // Raylar ve traversler.
  for (float x : {-mTrackGauge * 0.5f, mTrackGauge * 0.5f}) {
    addBox({x, -1.24f, -halfRoute}, {0.055f, 0.055f, halfRoute},
           {0.72f, 0.74f, 0.78f, 1.0f}, 0.28f, 3.0f);
  }
  const float firstTraverse = std::floor(visibleStart / 2.0f) * 2.0f;
  for (float p = firstTraverse; p <= visibleEnd; p += 2.0f) {
    const float z = -p;
    addBox({0.0f, -1.27f, z}, {1.0f, 0.035f, 0.10f},
           {0.26f, 0.28f, 0.30f, 1.0f}, 0.85f);
  }

  // Kolonlar ve üst kirişler: gerçek asset gelene kadar istasyonun ritmini verir.
  for (float z = -mColumnSpacing; z > -mRouteLength; z -= mColumnSpacing) {
    if (z < visibleMinZ || z > visibleMaxZ) continue;
    for (float x : {-mPlatformWidth - 3.0f, mPlatformWidth + 3.0f})
      addBox({x, 1.2f, z}, {0.35f, 2.8f, 0.35f},
             {0.34f, 0.36f, 0.40f, 1.0f}, 0.72f);
    addBox({0.0f, 3.55f, z}, {mPlatformWidth + 3.5f, 0.18f, 0.30f},
           {0.27f, 0.29f, 0.34f, 1.0f}, 0.82f);
  }

  // Tünel duvarları ve kablo kanalları.
  addBox({-8.6f, 1.25f, -halfRoute}, {0.18f, 2.75f, halfRoute},
         {0.16f, 0.18f, 0.22f, 1.0f}, 0.96f);
  addBox({8.6f, 1.25f, -halfRoute}, {0.18f, 2.75f, halfRoute},
         {0.16f, 0.18f, 0.22f, 1.0f}, 0.96f);
  addBox({-7.9f, 2.35f, -halfRoute}, {0.08f, 0.14f, halfRoute},
         {0.10f, 0.11f, 0.13f, 1.0f}, 0.78f);
  addBox({7.9f, 2.35f, -halfRoute}, {0.08f, 0.14f, halfRoute},
         {0.10f, 0.11f, 0.13f, 1.0f}, 0.78f);

  // 1500 V havai hat için basit temas teli ve taşıyıcı sistemi.
  addBox({0.0f, 3.05f, -halfRoute}, {0.025f, 0.025f, halfRoute},
         {0.58f, 0.60f, 0.62f, 1.0f}, 0.22f);
  for (float z = -12.0f; z > -mRouteLength; z -= 24.0f) {
    if (z < visibleMinZ || z > visibleMaxZ) continue;
    if (z < visibleMinZ || z > visibleMaxZ) continue;
    addBox({0.0f, 3.02f, z},
           {mPlatformWidth + 3.3f, 0.035f, 0.035f},
           {0.42f, 0.44f, 0.48f, 1.0f}, 0.35f);
    for (float x : {-mPlatformWidth - 3.0f, mPlatformWidth + 3.0f}) {
      addBox({x, 1.85f, z}, {0.09f, 1.15f, 0.09f},
             {0.28f, 0.30f, 0.34f, 1.0f}, 0.72f);
    }
  }

  // İstasyon armatürleri ve reklam/tabela panelleri.
  for (float z = -8.0f; z > -120.0f; z -= 16.0f) {
    addBox({0.0f, 3.55f, z}, {2.4f, 0.06f, 0.20f},
           {0.86f, 0.90f, 0.82f, 1.0f}, 0.25f);
    for (float x : {-mPlatformWidth + 0.75f, mPlatformWidth - 0.75f}) {
      addBox({x, 2.05f, z}, {0.75f, 0.70f, 0.06f},
             {0.04f, 0.16f, 0.24f, 1.0f}, 0.35f);
    }
  }

  // Kadıköy başlangıç bölümünde banklar, çöp kutuları ve turnike adaları.
  for (float z : {-18.0f, -42.0f, -66.0f, -90.0f}) {
    for (float x : {-mPlatformWidth + 1.1f, mPlatformWidth - 1.1f}) {
      addBox({x, 0.32f, z}, {0.75f, 0.10f, 0.28f},
             {0.20f, 0.23f, 0.27f, 1.0f}, 0.68f);
      addBox({x, 0.55f, z}, {0.75f, 0.42f, 0.08f},
             {0.30f, 0.33f, 0.38f, 1.0f}, 0.62f);
    }
  }

  // Basit 5x5 bitmap font: kritik istasyon tabelalarını gerçek geometri olarak çizer.
  auto glyph = [](char ch) -> std::array<std::string_view, 5> {
    switch (ch) {
      case 'A': return {"01110","10001","11111","10001","10001"};
      case 'D': return {"11110","10001","10001","10001","11110"};
      case 'E': return {"11111","10000","11110","10000","11111"};
      case 'I': return {"11111","00100","00100","00100","11111"};
      case 'K': return {"10001","10010","11100","10010","10001"};
      case 'M': return {"10001","11011","10101","10001","10001"};
      case 'O': return {"01110","10001","10001","10001","01110"};
      case 'T': return {"11111","00100","00100","00100","00100"};
      case 'Y': return {"10001","01010","00100","00100","00100"};
      case '4': return {"10010","10010","11111","00010","00010"};
      default: return {"00000","00000","00000","00000","00000"};
    }
  };
  auto addText = [&](std::string_view text, glm::vec3 origin,
                     float pixel = 0.065f, float gap = 0.018f) {
    float cursorX = origin.x;
    for (char raw : text) {
      const char ch = static_cast<char>(std::toupper(
          static_cast<unsigned char>(raw)));
      if (ch == ' ') {
        cursorX += pixel * 3.0f;
        continue;
      }
      const auto rows = glyph(ch);
      for (size_t row = 0; row < rows.size(); ++row) {
        for (size_t col = 0; col < rows[row].size(); ++col) {
          if (rows[row][col] != '1') continue;
          addBox({cursorX + static_cast<float>(col) * (pixel + gap),
                  origin.y - static_cast<float>(row) * (pixel + gap),
                  origin.z},
                 {pixel * 0.45f, pixel * 0.45f, 0.035f},
                 {0.90f, 0.94f, 0.82f, 1.0f}, 0.20f, 5.0f);
        }
      }
      cursorX += 5.0f * (pixel + gap) + pixel;
    }
  };

  addBox({0.0f, 3.38f, -24.0f}, {3.25f, 0.52f, 0.08f},
         {0.025f, 0.14f, 0.22f, 1.0f}, 0.35f, 8.0f);
  addText("KADIKOY", {-2.55f, 3.62f, -24.10f});

  addBox({0.0f, 3.12f, -44.0f}, {1.25f, 0.42f, 0.06f},
         {0.025f, 0.14f, 0.22f, 1.0f}, 0.35f, 8.0f);
  addText("M4", {-0.36f, 3.28f, -44.10f}, 0.055f, 0.014f);

  // İstasyon geometrisi: yalnızca gerçek durak noktalarında iki yan peron.
  // Peron kotu rayın yaklaşık 1 m üstündedir; parçalar tek bir hacme bağlanır.
  constexpr float stationHalfLength = 58.0f;
  for (size_t i = 0; i < mStopPositions.size(); ++i) {
    const float p = mStopPositions[i];
    if (p < 0.0f || p > mRouteLength) continue;
    if (p + stationHalfLength < visibleStart || p - stationHalfLength > visibleEnd) continue;

    const float z = -p;
    const float trackHalf = mTrackGauge * 0.5f;
    const float platformInner = trackHalf + 0.14f;
    const float platformOuter = mPlatformWidth;
    const float platformCenter = (platformInner + platformOuter) * 0.5f;
    const float platformHalfWidth = (platformOuter - platformInner) * 0.5f;

    for (float side : {-1.0f, 1.0f}) {
      // Ana peron döşemesi.
      addBox({side * platformCenter, -0.50f, z},
             {platformHalfWidth, 0.50f, stationHalfLength},
             {0.40f, 0.42f, 0.45f, 1.0f}, 0.88f, 2.0f);

      // Peron arka tarafında düşük süpürgelik/duvar tabanı.
      addBox({side * (platformOuter - 0.12f), 0.45f, z},
             {0.12f, 0.95f, stationHalfLength},
             {0.25f, 0.27f, 0.31f, 1.0f}, 0.90f, 1.0f);

      // Ray tarafında sarı güvenlik + ince taktil şerit.
      addBox({side * (platformInner + 0.10f), 0.03f, z},
             {0.045f, 0.025f, stationHalfLength - 0.8f},
             {0.96f, 0.70f, 0.08f, 1.0f}, 0.42f, 6.0f);
      addBox({side * (platformInner + 0.28f), 0.05f, z},
             {0.07f, 0.035f, stationHalfLength - 0.8f},
             {0.76f, 0.76f, 0.72f, 1.0f}, 0.72f, 2.0f);

      // Tavana bağlı istasyon aydınlatmaları; artık havada duran taşıyıcı yok.
      for (float lightP = p - 45.0f; lightP <= p + 45.0f; lightP += 15.0f) {
        addBox({side * (platformOuter - 1.0f), 3.28f, -lightP},
               {0.75f, 0.045f, 0.12f},
               {0.95f, 0.92f, 0.82f, 1.0f}, 0.18f, 5.0f);
      }
    }

    // İstasyonun adı/hat tabelası merkezi eksende, tavana bağlı.
    addBox({0.0f, 3.38f, z},
           {2.15f, 0.38f, 0.06f},
           {0.025f, 0.14f, 0.22f, 1.0f}, 0.35f, 8.0f);

    // Yan duvarlarda tabelayı gerçekten duvara bağlayan kısa kollar.
    for (float side : {-1.0f, 1.0f}) {
      addBox({side * 8.18f, 2.55f, z},
             {0.12f, 0.75f, 0.08f},
             {0.20f, 0.22f, 0.25f, 1.0f}, 0.60f, 3.0f);
      addBox({side * 8.02f, 2.55f, z},
             {0.85f, 0.45f, 0.035f},
             {0.04f, 0.20f, 0.32f, 1.0f}, 0.40f, 8.0f);
    }
  }

  // Kadıköy peronundaki temel mobilya; yalnız ilk 120 m içinde.
  if (visibleStart < 120.0f) {
    for (float z : {-18.0f, -42.0f, -66.0f}) {
      for (float side : {-1.0f, 1.0f}) {
        const float x = side * 2.9f;
        addBox({x, 0.05f, z},
               {0.80f, 0.08f, 0.28f},
               {0.20f, 0.23f, 0.27f, 1.0f}, 0.68f);
        addBox({x, 0.30f, z},
               {0.80f, 0.28f, 0.08f},
               {0.30f, 0.33f, 0.38f, 1.0f}, 0.62f);
      }
    }
  }

  // Her gerçek stop için sinyal direği + durak işareti.
  for (size_t i = 0; i < mStopPositions.size(); ++i) {
    const float p = mStopPositions[i];
    if (p < 0.0f || p > mRouteLength) continue;
    const bool terminal = i + 1 == mStopPositions.size();
    const bool nextStop = p > trainPosition + 0.5f &&
                          (i == 0 || mStopPositions[i - 1] <= trainPosition + 0.5f);
    const glm::vec4 signalColor =
        terminal ? glm::vec4(0.90f, 0.18f, 0.10f, 1.0f)
        : nextStop ? glm::vec4(0.10f, 0.85f, 0.95f, 1.0f)
                   : glm::vec4(0.90f, 0.65f, 0.08f, 1.0f);
    addBox({mPlatformWidth + 1.0f, 0.9f, -p},
           {0.10f, 0.9f, 0.10f}, signalColor, 0.35f);
    addBox({mPlatformWidth + 1.0f, 1.85f, -p},
           {0.25f, 0.18f, 0.08f}, signalColor, 0.25f);
    addBox({-mPlatformWidth - 0.9f, 2.4f, -p},
           {0.9f, 0.55f, 0.06f}, {0.04f, 0.30f, 0.48f, 1.0f}, 0.45f);
  }

  // Blok sinyalleri.
  const float blockLength = mRouteLength / static_cast<float>(std::max<size_t>(mBlockCount, 1));
  const size_t occupiedBlock = std::min(
      static_cast<size_t>(std::max(trainPosition, 0.0f) / blockLength),
      mBlockCount > 0 ? mBlockCount - 1 : 0);
  for (size_t block = 0; block < mBlockCount; ++block) {
    const float p = blockLength * static_cast<float>(block);
    const bool occupied =
        block < occupiedBlocks.size() && occupiedBlocks[block];
    const bool nextBlock = block == occupiedBlock + 1;
    const glm::vec4 aspectColor =
        occupied ? glm::vec4(0.88f, 0.08f, 0.06f, 1.0f)
        : nextBlock ? glm::vec4(0.95f, 0.72f, 0.08f, 1.0f)
                    : glm::vec4(0.08f, 0.80f, 0.25f, 1.0f);
    addBox({-mPlatformWidth - 1.25f, 1.0f, -p},
           {0.12f, 1.0f, 0.12f}, aspectColor, 0.30f);
    addBox({-mPlatformWidth - 1.25f, 2.05f, -p},
           {0.24f, 0.08f, 0.08f}, aspectColor, 0.20f);
  }

  // 4 vagonlu CAF M4 seti. Fizik noktası setin referans noktasıdır;
  // görsel olarak araçlar birbirine bağlı tek bir kompozisyon oluşturur.
  constexpr float carLength = 19.5f;
  constexpr float carGap = 0.25f;
  constexpr size_t carCount = 4;
  const float setLength =
      carCount * carLength + static_cast<float>(carCount - 1) * carGap;
  const float setCenter = -trainPosition;

  for (size_t car = 0; car < carCount; ++car) {
    const float carZ =
        setCenter + (static_cast<float>(car) - 1.5f) * (carLength + carGap);

    addBox({0.0f, 0.05f, carZ},
           {mTrainWidth * 0.52f, mTrainHeight * 0.52f, carLength * 0.48f},
           {0.055f, 0.28f, 0.56f, 1.0f}, 0.50f, 7.0f);
    addBox({0.0f, 0.88f, carZ},
           {mTrainWidth * 0.53f, 0.40f, carLength * 0.47f},
           {0.78f, 0.80f, 0.77f, 1.0f}, 0.30f);
    addBox({0.0f, 1.48f, carZ},
           {mTrainWidth * 0.48f, 0.18f, carLength * 0.45f},
           {0.70f, 0.73f, 0.77f, 1.0f}, 0.25f);

    // Koyu cam bandı.
    addBox({-mTrainWidth * 0.535f, 0.82f, carZ},
           {0.035f, 0.34f, carLength * 0.38f},
           {0.02f, 0.065f, 0.10f, 1.0f}, 0.10f, 4.0f);
    addBox({mTrainWidth * 0.535f, 0.82f, carZ},
           {0.035f, 0.34f, carLength * 0.38f},
           {0.02f, 0.065f, 0.10f, 1.0f}, 0.10f, 4.0f);

    // Dört vagonun her birinde iki kapı bölgesi.
    for (float doorZ : {-carLength * 0.28f, carLength * 0.28f}) {
      const float slide = 0.42f * doorOpenFraction;
      addBox({-mTrainWidth * 0.54f - slide, 0.76f, carZ + doorZ},
             {0.028f, 0.56f, 0.43f},
             {0.62f, 0.66f, 0.71f, 1.0f}, 0.20f);
      addBox({mTrainWidth * 0.54f + slide, 0.76f, carZ + doorZ},
             {0.028f, 0.56f, 0.43f},
             {0.62f, 0.66f, 0.71f, 1.0f}, 0.20f);
    }

    // Boji bölgeleri.
    for (float bogieZ : {-carLength * 0.33f, carLength * 0.33f}) {
      addBox({0.0f, -0.76f, carZ + bogieZ},
             {mTrainWidth * 0.38f, 0.16f, 0.45f},
             {0.10f, 0.11f, 0.13f, 1.0f}, 0.70f, 3.0f);
    }

    // Tavan üzerindeki ana ekipman.
    addBox({0.0f, 1.78f, carZ},
           {0.22f, 0.10f, carLength * 0.20f},
           {0.26f, 0.28f, 0.30f, 1.0f}, 0.72f, 3.0f);
  }

  // Körük/bağlantı noktaları.
  for (size_t car = 0; car + 1 < carCount; ++car) {
    const float z =
        setCenter + (static_cast<float>(car) - 1.0f) * (carLength + carGap);
    addBox({0.0f, 0.22f, z},
           {mTrainWidth * 0.22f, 0.46f, carGap * 0.45f},
           {0.08f, 0.09f, 0.11f, 1.0f}, 0.75f);
  }

  // Ön kabin burnu ve aydınlatma.
  addBox({0.0f, 0.60f, setCenter - setLength * 0.50f},
         {mTrainWidth * 0.45f, 0.30f, 0.08f},
         {0.52f, 0.62f, 0.72f, 1.0f}, 0.18f);
  for (float x : {-0.68f, 0.68f}) {
    addBox({x, 0.70f, setCenter - setLength * 0.50f - 0.10f},
           {0.11f, 0.11f, 0.05f},
           {1.0f, 0.95f, 0.72f, 1.0f}, 0.16f, 5.0f);
  }

  // Kabin ön camı ve sürücü konsolu: kamera kabin içinde olduğunda sürüş hissini artırır.
  addBox({0.0f, 1.15f, -trainPosition - 38.0f},
         {mTrainWidth * 0.46f, 0.50f, 0.05f},
         {0.025f, 0.055f, 0.075f, 1.0f}, 0.10f);
  addBox({0.0f, 0.72f, -trainPosition - 37.5f},
         {mTrainWidth * 0.30f, 0.18f, 0.70f},
         {0.08f, 0.10f, 0.12f, 1.0f}, 0.65f);
  addBox({0.0f, 0.88f, -trainPosition - 37.0f},
         {mTrainWidth * 0.18f, 0.08f, 0.12f},
         {0.18f, 0.65f, 0.78f, 1.0f}, 0.25f, 5.0f);

  // Kabin gösterge paneli: hız çubuğu, fren lambası ve kapı durumu.
  const float speedKmh = std::clamp(trainSpeed * 3.6f, 0.0f, 80.0f);
  const float speedFraction = speedKmh / 80.0f;
  addBox({-0.65f, 0.92f, -trainPosition - 37.0f},
         {1.55f, 0.035f, 0.08f},
         {0.05f, 0.07f, 0.09f, 1.0f}, 0.40f);
  addBox({-0.65f + speedFraction * 1.55f, 0.98f, -trainPosition - 37.0f},
         {0.035f, 0.08f, 0.10f},
         {1.0f, 0.72f, 0.16f, 1.0f}, 0.18f);
  addBox({0.90f, 0.92f, -trainPosition - 37.0f},
         {0.16f, 0.10f, 0.10f},
         {doorOpenFraction > 0.05f ? 0.10f : 0.70f,
          doorOpenFraction > 0.05f ? 0.85f : 0.72f,
          doorOpenFraction > 0.05f ? 0.22f : 0.12f, 1.0f}, 0.25f);
  addBox({1.25f, 0.92f, -trainPosition - 37.0f},
         {0.16f, 0.10f, 0.10f},
         {speedKmh < 0.1f ? 0.10f : 0.75f,
          speedKmh < 0.1f ? 0.85f : 0.12f,
          0.12f, 1.0f}, 0.25f);

  // Yürüyen yolcular.
  for (const glm::vec2& p : passengerPositions) {
    addBox({p.x, 0.25f, p.y}, {0.16f, 0.50f, 0.16f},
           {0.16f, 0.62f, 0.80f, 1.0f}, 0.82f);
  }

  return instances;
}


// SPIR-V dizini: ortam değişkeniyle ezilebilir; varsayılan CMake tanımı.
static std::string shaderDir() {
  if (const char* env = std::getenv("METRO_SHADER_DIR")) return env;
  return METRO_SHADER_DIR;
}

VkShaderModule Renderer::loadShader(const char* filename) {
  std::ifstream file(shaderDir() + "/" + filename, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::runtime_error(std::string("Shader okunamiyor: ") + shaderDir() + "/" + filename +
                             " (once 'tools/container.sh build' calistirilmali)");
  }
  const std::streamsize size = file.tellg();
  if (size == 0 || size % 4 != 0) {
    throw std::runtime_error(std::string("Shader bozuk (SPIR-V 4 bayt hizali olmali): ") + filename);
  }
  std::vector<char> bytes(static_cast<size_t>(size));
  file.seekg(0);
  file.read(bytes.data(), size);

  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = static_cast<size_t>(size);
  ci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(mCtx->device(), &ci, nullptr, &module) != VK_SUCCESS) {
    throw std::runtime_error(std::string("vkCreateShaderModule: ") + filename);
  }
  return module;
}

VkFormat Renderer::pickDepthFormat() const {
  const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                 VK_FORMAT_D24_UNORM_S8_UINT};
  for (VkFormat f : candidates) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(mCtx->physicalDevice(), f, &props);
    if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
      return f;
    }
  }
  return VK_FORMAT_D16_UNORM;
}

bool Renderer::init(VulkanContext& ctx, SDL_Window* window,
                    const std::string& manifestModelPath, float routeLength,
                    float platformWidth, float trackGauge, float trainWidth,
                    float trainHeight, float columnSpacing,
                    const glm::vec3& platformEdgePosition,
                    const glm::vec3& stopPosition,
                    const std::vector<float>& stopPositions, size_t blockCount) {
  mCtx = &ctx;
  mWindow = window;
  mRouteLength = routeLength > 0.0f ? routeLength : 2000.0f;
  mPlatformWidth = platformWidth > 0.0f ? platformWidth : 4.0f;
  mTrackGauge = trackGauge > 0.0f ? trackGauge : 2.4f;
  mTrainWidth = trainWidth > 0.0f ? trainWidth : 2.8f;
  mTrainHeight = trainHeight > 0.0f ? trainHeight : 3.2f;
  mColumnSpacing = columnSpacing > 0.0f ? columnSpacing : 24.0f;
  mPlatformEdgePosition = platformEdgePosition;
  mStopPosition = stopPosition;
  mStopPositions = stopPositions;
  mBlockCount = blockCount > 0 ? blockCount : 8;
  const char* environmentModel = std::getenv("METRO_MODEL_PATH");

  try {
    mSwapchain.create(ctx, window);
    createRenderPass();      // sahne (PBR) — HDR offscreen hedefe
    createDepthResources();
    createHdrResources();    // HDR renk hedefleri + sampler
    createFrameUniforms();
    createPipeline();
    createFramebuffers();    // sahne framebuffer'ları (HDR+depth)
    createPostRenderPass();  // post-process (tonemap) — swapchain'e
    createPostResources();   // HDR örnekleme descriptor'ları
    createPostPipeline();
    createPostFramebuffers();
    createCommandObjects();
    const std::string selectedModel =
        environmentModel != nullptr
            ? environmentModel
            : (manifestModelPath.empty() ? "assets/box.glb" : manifestModelPath);
    if (!mModel.load(ctx, mCommandPool, selectedModel)) {
      throw std::runtime_error(std::string("Model yuklenemedi: ") + selectedModel);
    }
    createSyncObjects();
  } catch (const std::exception& e) {
    METRO_ERROR("Renderer init: %s", e.what());
    return false;
  }
  METRO_INFO("Renderer hazir (PBR+HDR+ACES tonemap, model: %s, %u frame-in-flight)",
             environmentModel != nullptr
                 ? environmentModel
                 : (manifestModelPath.empty() ? "assets/box.glb"
                                               : manifestModelPath.c_str()),
             MaxFramesInFlight);
  return true;
}

void Renderer::createRenderPass() {
  // Sahne geçişi artık swapchain'e değil HDR offscreen hedefe yazar; format
  // swapchain'den bağımsız olduğu için burada mSwapFormat GÜNCELLENMEZ
  // (bkz. createPostRenderPass — swapchain formatına bağlı olan odur).
  mDepthFormat = pickDepthFormat();

  VkAttachmentDescription color{};
  color.format = kHdrFormat;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // Post-process geçişi bu görüntüyü örnekleyecek; pass sonunda hazır olsun.
  color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth{};
  depth.format = mDepthFormat;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription attachments[] = {color, depth};

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef{};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;

  VkSubpassDependency deps[2]{};
  // Önceki karenin post-process okumasıyla bu karenin renk eki yazmasını çakıştırma.
  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  // Bu karenin renk eki yazmasıyla post-process geçişinin örneklemesini çakıştırma.
  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 2;
  ci.pAttachments = attachments;
  ci.subpassCount = 1;
  ci.pSubpasses = &subpass;
  ci.dependencyCount = 2;
  ci.pDependencies = deps;

  if (vkCreateRenderPass(mCtx->device(), &ci, nullptr, &mRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateRenderPass");
  }
}

void Renderer::createDepthResources() {
  VkImageCreateInfo img{};
  img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  img.imageType = VK_IMAGE_TYPE_2D;
  img.format = mDepthFormat;
  img.extent.width = mSwapchain.extent().width;
  img.extent.height = mSwapchain.extent().height;
  img.extent.depth = 1;
  img.mipLevels = 1;
  img.arrayLayers = 1;
  img.samples = VK_SAMPLE_COUNT_1_BIT;
  img.tiling = VK_IMAGE_TILING_OPTIMAL;
  img.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_AUTO;

  if (vmaCreateImage(mCtx->allocator(), &img, &alloc, &mDepthImage, &mDepthAlloc, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("vmaCreateImage (depth)");
  }

  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = mDepthImage;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = mDepthFormat;
  view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  // Kombine depth+stencil formatlarında stencil bölümü de işaretle.
  if (mDepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || mDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
    view.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  view.subresourceRange.levelCount = 1;
  view.subresourceRange.layerCount = 1;
  if (vkCreateImageView(mCtx->device(), &view, nullptr, &mDepthView) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateImageView (depth)");
  }
}

void Renderer::createHdrResources() {
  // Sampler pipeline ömründe yaşar (extent'ten bağımsız); yalnız ilk
  // kurulumda yaratılır — mDescSetLayout ile aynı desen.
  if (mHdrSampler == VK_NULL_HANDLE) {
    VkSamplerCreateInfo samplerCi{};
    samplerCi.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCi.magFilter = VK_FILTER_LINEAR;
    samplerCi.minFilter = VK_FILTER_LINEAR;
    samplerCi.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCi.maxLod = 1.0f;
    if (vkCreateSampler(mCtx->device(), &samplerCi, nullptr, &mHdrSampler) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateSampler (HDR)");
    }
  }

  // Her swapchain image'i kendi HDR hedefine yazar (frame UBO'yla aynı
  // per-image desen); extent değişince destroySwapchainDependent + burada
  // yeniden kurulur.
  const uint32_t count = mSwapchain.imageCount();
  mHdrImages.resize(count);
  mHdrAllocs.resize(count);
  mHdrViews.resize(count);

  for (uint32_t i = 0; i < count; ++i) {
    VkImageCreateInfo img{};
    img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img.imageType = VK_IMAGE_TYPE_2D;
    img.format = kHdrFormat;
    img.extent.width = mSwapchain.extent().width;
    img.extent.height = mSwapchain.extent().height;
    img.extent.depth = 1;
    img.mipLevels = 1;
    img.arrayLayers = 1;
    img.samples = VK_SAMPLE_COUNT_1_BIT;
    img.tiling = VK_IMAGE_TILING_OPTIMAL;
    img.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(mCtx->allocator(), &img, &alloc, &mHdrImages[i], &mHdrAllocs[i], nullptr) !=
        VK_SUCCESS) {
      throw std::runtime_error("vmaCreateImage (HDR)");
    }

    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = mHdrImages[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = kHdrFormat;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    if (vkCreateImageView(mCtx->device(), &view, nullptr, &mHdrViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateImageView (HDR)");
    }
  }
}

void Renderer::createFrameUniforms() {
  const uint32_t count = mSwapchain.imageCount();

  // Set layout pipeline ömründe yaşar; yalnız ilk kurulumda yaratılır.
  if (mDescSetLayout == VK_NULL_HANDLE) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(mCtx->device(), &layoutCi, nullptr, &mDescSetLayout) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateDescriptorSetLayout");
    }
  }

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = count;

  VkDescriptorPoolCreateInfo poolCi{};
  poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolCi.maxSets = count;
  poolCi.poolSizeCount = 1;
  poolCi.pPoolSizes = &poolSize;
  if (vkCreateDescriptorPool(mCtx->device(), &poolCi, nullptr, &mDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateDescriptorPool");
  }

  // Her swapchain image'i kendi UBO'sunu kullanır: acquire dönene kadar o
  // image'in önceki karesi bitmiş olur, mapped yazım güvenlidir.
  mFrameUboBuffers.resize(count);
  mFrameUboAllocs.resize(count);
  mFrameUboMapped.resize(count);
  mDescriptorSets.resize(count);

  for (uint32_t i = 0; i < count; ++i) {
    VkBufferCreateInfo buf{};
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.size = sizeof(FrameUniforms);
    buf.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo alloc{};
    alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;
    alloc.usage = VMA_MEMORY_USAGE_AUTO;

    VmaAllocationInfo out{};
    if (vmaCreateBuffer(mCtx->allocator(), &buf, &alloc, &mFrameUboBuffers[i],
                        &mFrameUboAllocs[i], &out) != VK_SUCCESS) {
      throw std::runtime_error("vmaCreateBuffer (frame UBO)");
    }
    mFrameUboMapped[i] = out.pMappedData;

    VkDescriptorSetAllocateInfo setAi{};
    setAi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAi.descriptorPool = mDescriptorPool;
    setAi.descriptorSetCount = 1;
    setAi.pSetLayouts = &mDescSetLayout;
    if (vkAllocateDescriptorSets(mCtx->device(), &setAi, &mDescriptorSets[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkAllocateDescriptorSets");
    }

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mFrameUboBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(FrameUniforms);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = mDescriptorSets[i];
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(mCtx->device(), 1, &write, 0, nullptr);
  }
}

void Renderer::createPipeline() {
  // Viewport/scissor dinamik: pencere boyutu değişince pipeline yeniden
  // derlenmez, sadece record sırasında güncellenir.
  VkShaderModule vert = loadShader("pbr.vert.spv");
  VkShaderModule frag = loadShader("pbr.frag.spv");

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";

  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = sizeof(Vertex);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attrs[3]{};
  attrs[0].location = 0; // inPosition
  attrs[0].binding = 0;
  attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrs[0].offset = offsetof(Vertex, pos);
  attrs[1].location = 1; // inNormal
  attrs[1].binding = 0;
  attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrs[1].offset = offsetof(Vertex, normal);
  attrs[2].location = 2; // inTexCoord
  attrs[2].binding = 0;
  attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
  attrs[2].offset = offsetof(Vertex, texCoord);

  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInput.vertexBindingDescriptionCount = 1;
  vertexInput.pVertexBindingDescriptions = &binding;
  vertexInput.vertexAttributeDescriptionCount = 3;
  vertexInput.pVertexAttributeDescriptions = attrs;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_BACK_BIT; // artık gerçek 3D sahne + depth var
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_TRUE;
  depth.depthWriteEnable = VK_TRUE;
  depth.depthCompareOp = VK_COMPARE_OP_LESS;

  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blendAttachment;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamicStates;

  // Model matrisi + malzeme faktörleri: 88 bayt (vertex+fragment).
  VkPushConstantRange pcRange{};
  pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pcRange.offset = 0;
  pcRange.size = sizeof(ModelPush);

  VkPipelineLayoutCreateInfo layoutCi{};
  layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutCi.setLayoutCount = 1;
  layoutCi.pSetLayouts = &mDescSetLayout;
  layoutCi.pushConstantRangeCount = 1;
  layoutCi.pPushConstantRanges = &pcRange;

  if (vkCreatePipelineLayout(mCtx->device(), &layoutCi, nullptr, &mPipelineLayout) != VK_SUCCESS) {
    vkDestroyShaderModule(mCtx->device(), vert, nullptr);
    vkDestroyShaderModule(mCtx->device(), frag, nullptr);
    throw std::runtime_error("vkCreatePipelineLayout");
  }

  VkGraphicsPipelineCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci.stageCount = 2;
  ci.pStages = stages;
  ci.pVertexInputState = &vertexInput;
  ci.pInputAssemblyState = &inputAssembly;
  ci.pViewportState = &viewportState;
  ci.pRasterizationState = &raster;
  ci.pMultisampleState = &multisample;
  ci.pDepthStencilState = &depth;
  ci.pColorBlendState = &blend;
  ci.pDynamicState = &dynamic;
  ci.layout = mPipelineLayout;
  ci.renderPass = mRenderPass;
  ci.subpass = 0;

  const VkResult r = vkCreateGraphicsPipelines(mCtx->device(), VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline);

  vkDestroyShaderModule(mCtx->device(), vert, nullptr); // pipeline modülü kopyaladı
  vkDestroyShaderModule(mCtx->device(), frag, nullptr);
  if (r != VK_SUCCESS) throw std::runtime_error("vkCreateGraphicsPipelines");
}

void Renderer::createFramebuffers() {
  // Sahne geçişi artık HDR offscreen hedefe yazar (swapchain view değil).
  mFramebuffers.resize(mHdrViews.size());
  for (size_t i = 0; i < mHdrViews.size(); ++i) {
    VkImageView attachments[] = {mHdrViews[i], mDepthView};

    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = mRenderPass;
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.width = mSwapchain.extent().width;
    ci.height = mSwapchain.extent().height;
    ci.layers = 1;
    if (vkCreateFramebuffer(mCtx->device(), &ci, nullptr, &mFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateFramebuffer");
    }
  }
}

void Renderer::createPostRenderPass() {
  // Bu render pass swapchain formatına bağlıdır — mSwapFormat burada takip
  // edilir (recreateSwapchain format değişimini bu değere göre algılar).
  mSwapFormat = mSwapchain.format();

  VkAttachmentDescription color{};
  color.format = mSwapFormat;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  // Tam ekran üçgen her pikseli baştan yazar; önceki içerik önemsiz.
  color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorRef{};
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;

  // Sunum motorunun okumasıyla bu geçişin renk eki yazmasını çakıştırma.
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 1;
  ci.pAttachments = &color;
  ci.subpassCount = 1;
  ci.pSubpasses = &subpass;
  ci.dependencyCount = 1;
  ci.pDependencies = &dep;

  if (vkCreateRenderPass(mCtx->device(), &ci, nullptr, &mPostRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateRenderPass (post)");
  }
}

void Renderer::createPostResources() {
  // Set layout pipeline ömründe yaşar; yalnız ilk kurulumda yaratılır.
  if (mPostDescSetLayout == VK_NULL_HANDLE) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutCi{};
    layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCi.bindingCount = 1;
    layoutCi.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(mCtx->device(), &layoutCi, nullptr, &mPostDescSetLayout) !=
        VK_SUCCESS) {
      throw std::runtime_error("vkCreateDescriptorSetLayout (post)");
    }
  }

  const uint32_t count = mSwapchain.imageCount();

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = count;

  VkDescriptorPoolCreateInfo poolCi{};
  poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolCi.maxSets = count;
  poolCi.poolSizeCount = 1;
  poolCi.pPoolSizes = &poolSize;
  if (vkCreateDescriptorPool(mCtx->device(), &poolCi, nullptr, &mPostDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateDescriptorPool (post)");
  }

  // Her swapchain image'in post-process seti kendi HDR görünümünü örnekler
  // (imageIndex ile sahne geçişinin yazdığı HDR hedefle birebir eşleşir).
  mPostDescriptorSets.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    VkDescriptorSetAllocateInfo setAi{};
    setAi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAi.descriptorPool = mPostDescriptorPool;
    setAi.descriptorSetCount = 1;
    setAi.pSetLayouts = &mPostDescSetLayout;
    if (vkAllocateDescriptorSets(mCtx->device(), &setAi, &mPostDescriptorSets[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkAllocateDescriptorSets (post)");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = mHdrSampler;
    imageInfo.imageView = mHdrViews[i];
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = mPostDescriptorSets[i];
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(mCtx->device(), 1, &write, 0, nullptr);
  }
}

void Renderer::createPostPipeline() {
  // Tam ekran üçgen: vertex buffer yok, gl_VertexIndex'ten üretilir.
  VkShaderModule vert = loadShader("tonemap.vert.spv");
  VkShaderModule frag = loadShader("tonemap.frag.spv");

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";

  // Vertex girişi yok: konumlar/UV shader içinde gl_VertexIndex'ten üretilir.
  VkPipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE; // tek büyük üçgen; kırpma anlamsız
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth{}; // derinlik testi yok (tam ekran quad)
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blendAttachment;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamicStates;

  VkPipelineLayoutCreateInfo layoutCi{};
  layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutCi.setLayoutCount = 1;
  layoutCi.pSetLayouts = &mPostDescSetLayout;

  if (vkCreatePipelineLayout(mCtx->device(), &layoutCi, nullptr, &mPostPipelineLayout) != VK_SUCCESS) {
    vkDestroyShaderModule(mCtx->device(), vert, nullptr);
    vkDestroyShaderModule(mCtx->device(), frag, nullptr);
    throw std::runtime_error("vkCreatePipelineLayout (post)");
  }

  VkGraphicsPipelineCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci.stageCount = 2;
  ci.pStages = stages;
  ci.pVertexInputState = &vertexInput;
  ci.pInputAssemblyState = &inputAssembly;
  ci.pViewportState = &viewportState;
  ci.pRasterizationState = &raster;
  ci.pMultisampleState = &multisample;
  ci.pDepthStencilState = &depth;
  ci.pColorBlendState = &blend;
  ci.pDynamicState = &dynamic;
  ci.layout = mPostPipelineLayout;
  ci.renderPass = mPostRenderPass;
  ci.subpass = 0;

  const VkResult r =
      vkCreateGraphicsPipelines(mCtx->device(), VK_NULL_HANDLE, 1, &ci, nullptr, &mPostPipeline);

  vkDestroyShaderModule(mCtx->device(), vert, nullptr);
  vkDestroyShaderModule(mCtx->device(), frag, nullptr);
  if (r != VK_SUCCESS) throw std::runtime_error("vkCreateGraphicsPipelines (post)");
}

void Renderer::createPostFramebuffers() {
  mPostFramebuffers.resize(mSwapchain.views().size());
  for (size_t i = 0; i < mSwapchain.views().size(); ++i) {
    VkImageView attachment = mSwapchain.views()[i];

    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = mPostRenderPass;
    ci.attachmentCount = 1;
    ci.pAttachments = &attachment;
    ci.width = mSwapchain.extent().width;
    ci.height = mSwapchain.extent().height;
    ci.layers = 1;
    if (vkCreateFramebuffer(mCtx->device(), &ci, nullptr, &mPostFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateFramebuffer (post)");
    }
  }
}

void Renderer::createCommandObjects() {
  VkCommandPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = mCtx->graphicsFamily();
  if (vkCreateCommandPool(mCtx->device(), &pool, nullptr, &mCommandPool) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateCommandPool");
  }

  const uint32_t count = mSwapchain.imageCount();
  mCommands.resize(count);
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = mCommandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = count;
  if (vkAllocateCommandBuffers(mCtx->device(), &alloc, mCommands.data()) != VK_SUCCESS) {
    throw std::runtime_error("vkAllocateCommandBuffers");
  }
}

void Renderer::createSyncObjects() {
  VkSemaphoreCreateInfo sem{};
  sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT; // ilk karede bekleme anında dönsün

  // Tüm senkronizasyon nesneleri swapchain image sayısı kadar:
  // N image → N acquire semaforu, N render-bitim semaforu, N fence.
  // Böylece hiçbir nesne henüz present'ten dönmeden yeniden kullanılmaz.
  const uint32_t count = mSwapchain.imageCount();
  mImageAvailable.resize(count);
  mRenderFinished.resize(count);
  mInFlight.resize(count);

  for (uint32_t i = 0; i < count; ++i) {
    if (vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mImageAvailable[i]) != VK_SUCCESS ||
        vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mRenderFinished[i]) != VK_SUCCESS ||
        vkCreateFence(mCtx->device(), &fence, nullptr, &mInFlight[i]) != VK_SUCCESS) {
      throw std::runtime_error("senkronizasyon nesnesi olusturulamadi");
    }
  }
  mFrame = 0;
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                                   const app::Camera& camera, float trainPosition, float trainSpeed, float doorOpenFraction,
                                   const std::vector<bool>& occupiedBlocks,
                                   const std::vector<glm::vec2>& passengerPositions) {
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) throw std::runtime_error("vkBeginCommandBuffer");

  // Frame uniform'ları yaz (acquire dönmüş olmalı → UBO[imageIndex] boşta).
  const float aspect = static_cast<float>(mSwapchain.extent().width) /
                       static_cast<float>(mSwapchain.extent().height);
  FrameUniforms fu{};
  fu.viewProj = camera.getProjectionMatrix(aspect) * camera.getViewMatrix();
  fu.lightDir = glm::vec4(glm::normalize(glm::vec3(0.25f, -1.0f, -0.35f)), 0.0f);
  fu.cameraPos = glm::vec4(camera.position, 1.0f);
  // Far, trenin hareketiyle birlikte ilerler; tüneldeki yakın yüzeyleri aydınlatır.
  fu.headlightPos = glm::vec4(0.0f, 1.05f, -trainPosition - 41.5f, 1.0f);
  fu.headlightColor = glm::vec4(1.0f, 0.92f, 0.78f, 18.0f);
  std::memcpy(mFrameUboMapped[imageIndex], &fu, sizeof(fu));

  VkClearValue clears[2]{};
  clears[0].color = {{0.09f, 0.12f, 0.18f, 1.0f}}; // istasyon ambient rengi
  clears[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = mRenderPass;
  rp.framebuffer = mFramebuffers[imageIndex];
  rp.renderArea.extent = mSwapchain.extent();
  rp.clearValueCount = 2;
  rp.pClearValues = clears;
  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = static_cast<float>(mSwapchain.extent().width);
  viewport.height = static_cast<float>(mSwapchain.extent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = mSwapchain.extent();
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1,
                          &mDescriptorSets[imageIndex], 0, nullptr);

  const std::vector<SceneInstance> instances =
      buildKadikoyScene(trainPosition, trainSpeed, doorOpenFraction, occupiedBlocks, passengerPositions);
  mModel.bind(cmd);
  for (const SceneInstance& instance : instances) {
    for (size_t i = 0; i < mModel.subMeshCount(); ++i) {
      const SubMesh& sub = mModel.subMesh(i);
      ModelPush push{};
      push.model = instance.transform;
      push.baseColor = instance.color;
      push.metallic = sub.metallic;
      push.roughness = instance.roughness;
      push.materialId = instance.materialId;
      vkCmdPushConstants(cmd, mPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(ModelPush), &push);
      mModel.drawSubMesh(cmd, i);
    }
  }

  vkCmdEndRenderPass(cmd);

  // --- Post-process: HDR hedefi örnekle, ACES tonemap uygula, swapchain'e yaz ---
  VkClearValue postClear{};
  postClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // loadOp DONT_CARE; yalnız API gereği dolu

  VkRenderPassBeginInfo postRp{};
  postRp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  postRp.renderPass = mPostRenderPass;
  postRp.framebuffer = mPostFramebuffers[imageIndex];
  postRp.renderArea.extent = mSwapchain.extent();
  postRp.clearValueCount = 1;
  postRp.pClearValues = &postClear;
  vkCmdBeginRenderPass(cmd, &postRp, VK_SUBPASS_CONTENTS_INLINE);

  // Dinamik viewport/scissor render pass sınırları arasında korunur ama
  // netlik için burada da açıkça belirtiyoruz.
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPostPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPostPipelineLayout, 0, 1,
                          &mPostDescriptorSets[imageIndex], 0, nullptr);
  vkCmdDraw(cmd, 3, 1, 0, 0); // tam ekran üçgen; vertex/index buffer yok

  vkCmdEndRenderPass(cmd);
  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) throw std::runtime_error("vkEndCommandBuffer");
}

void Renderer::drawFrame(const app::Camera& camera, float trainPosition, float trainSpeed, float doorOpenFraction,
                         const std::vector<bool>& occupiedBlocks,
                         const std::vector<glm::vec2>& passengerPositions) {
  const VkDevice dev = mCtx->device();
  const uint32_t syncCount = static_cast<uint32_t>(mInFlight.size());

  // 1) Bu karenin fence'i: önceki submit bitmiş mi?
  vkWaitForFences(dev, 1, &mInFlight[mFrame], VK_TRUE, UINT64_MAX);

  uint32_t imageIndex = 0;
  const VkResult acquired =
      vkAcquireNextImageKHR(dev, mSwapchain.handle(), UINT64_MAX, mImageAvailable[mFrame],
                            VK_NULL_HANDLE, &imageIndex);
  if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return;
  }
  if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
    METRO_ERROR("vkAcquireNextImageKHR: %d", static_cast<int>(acquired));
    return;
  }

  vkResetFences(dev, 1, &mInFlight[mFrame]);

  // 2) Komut tamponunu bu image için yeniden yaz.
  vkResetCommandBuffer(mCommands[mFrame], 0);
  recordCommandBuffer(mCommands[mFrame], imageIndex, camera, trainPosition,
                      trainSpeed, doorOpenFraction, occupiedBlocks, passengerPositions);

  // 3) Submit: renk çıktısı aşamasına kadar bekle.
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &mImageAvailable[mFrame];
  submit.pWaitDstStageMask = &waitStage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &mCommands[mFrame];
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &mRenderFinished[mFrame];

  if (vkQueueSubmit(mCtx->graphicsQueue(), 1, &submit, mInFlight[mFrame]) != VK_SUCCESS) {
    throw std::runtime_error("vkQueueSubmit");
  }

  // 4) Present. handle() değer döndürdüğü için yerel değişkene al (adres
  // geçiciye bağlanmasın).
  VkSwapchainKHR swapchainHandle = mSwapchain.handle();
  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &mRenderFinished[mFrame];
  present.swapchainCount = 1;
  present.pSwapchains = &swapchainHandle;
  present.pImageIndices = &imageIndex;

  const VkResult presented = vkQueuePresentKHR(mCtx->graphicsQueue(), &present);
  if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain();
  } else if (presented != VK_SUCCESS) {
    METRO_ERROR("vkQueuePresentKHR: %d", static_cast<int>(presented));
  }

  mFrame = (mFrame + 1) % syncCount;
}

void Renderer::onResize() {
  recreateSwapchain();
}

void Renderer::destroySwapchainDependent() {
  for (VkFramebuffer fb : mFramebuffers) vkDestroyFramebuffer(mCtx->device(), fb, nullptr);
  mFramebuffers.clear();
  for (VkFramebuffer fb : mPostFramebuffers) vkDestroyFramebuffer(mCtx->device(), fb, nullptr);
  mPostFramebuffers.clear();

  // Depth buffer extent'e bağlı → yık, recreate'te yeniden kurulur.
  if (mDepthView) vkDestroyImageView(mCtx->device(), mDepthView, nullptr);
  mDepthView = VK_NULL_HANDLE;
  if (mDepthImage) vmaDestroyImage(mCtx->allocator(), mDepthImage, mDepthAlloc);
  mDepthImage = VK_NULL_HANDLE;
  mDepthAlloc = VK_NULL_HANDLE;

  // HDR renk hedefleri de extent'e bağlı (sampler hariç — o pipeline ömründe).
  for (VkImageView v : mHdrViews) vkDestroyImageView(mCtx->device(), v, nullptr);
  mHdrViews.clear();
  for (size_t i = 0; i < mHdrImages.size(); ++i) {
    vmaDestroyImage(mCtx->allocator(), mHdrImages[i], mHdrAllocs[i]);
  }
  mHdrImages.clear();
  mHdrAllocs.clear();

  // Post-process descriptor'ları HDR view'lara işaret eder → view'larla birlikte yık.
  mPostDescriptorSets.clear();
  if (mPostDescriptorPool) vkDestroyDescriptorPool(mCtx->device(), mPostDescriptorPool, nullptr);
  mPostDescriptorPool = VK_NULL_HANDLE;

  // Frame UBO + descriptor'lar image sayısına bağlı.
  for (size_t i = 0; i < mFrameUboBuffers.size(); ++i) {
    vmaDestroyBuffer(mCtx->allocator(), mFrameUboBuffers[i], mFrameUboAllocs[i]);
  }
  mFrameUboBuffers.clear();
  mFrameUboAllocs.clear();
  mFrameUboMapped.clear();
  mDescriptorSets.clear();
  if (mDescriptorPool) vkDestroyDescriptorPool(mCtx->device(), mDescriptorPool, nullptr);
  mDescriptorPool = VK_NULL_HANDLE;

  // Tüm senkronizasyon nesneleri ve command buffer'ları yık.
  for (auto& s : mImageAvailable) if (s) vkDestroySemaphore(mCtx->device(), s, nullptr);
  for (auto& s : mRenderFinished) if (s) vkDestroySemaphore(mCtx->device(), s, nullptr);
  for (auto& f : mInFlight) if (f) vkDestroyFence(mCtx->device(), f, nullptr);
  mImageAvailable.clear();
  mRenderFinished.clear();
  mInFlight.clear();

  if (!mCommands.empty()) {
    vkFreeCommandBuffers(mCtx->device(), mCommandPool, static_cast<uint32_t>(mCommands.size()), mCommands.data());
    mCommands.clear();
  }
}

void Renderer::recreateSwapchain() {
  vkDeviceWaitIdle(mCtx->device());
  destroySwapchainDependent();

  mSwapchain.recreate(*mCtx, mWindow);

  const uint32_t count = mSwapchain.imageCount();
  mImageAvailable.resize(count);
  mRenderFinished.resize(count);
  mInFlight.resize(count);

  VkSemaphoreCreateInfo sem{};
  sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fence{};
  fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < count; ++i) {
    if (vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mImageAvailable[i]) != VK_SUCCESS ||
        vkCreateSemaphore(mCtx->device(), &sem, nullptr, &mRenderFinished[i]) != VK_SUCCESS ||
        vkCreateFence(mCtx->device(), &fence, nullptr, &mInFlight[i]) != VK_SUCCESS) {
      throw std::runtime_error("senkronizasyon nesnesi olusturulamadi (recreate)");
    }
  }

  mCommands.resize(count);
  VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = mCommandPool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = count;
  if (vkAllocateCommandBuffers(mCtx->device(), &alloc, mCommands.data()) != VK_SUCCESS) {
    throw std::runtime_error("vkAllocateCommandBuffers (recreate)");
  }

  mFrame = 0;

  // Derinlik + HDR hedefleri + frame UBO'ları yeni image sayısı/extent'e
  // göre yeniden kur. Sahne render pass'ı (mRenderPass) HDR formatına bağlı
  // olduğu için swapchain format değişiminden ETKİLENMEZ — yalnız post-process
  // (swapchain'e yazan) geçiş bundan etkilenir.
  createDepthResources();
  createHdrResources();
  createFrameUniforms();

  if (mSwapchain.format() != mSwapFormat) {
    METRO_WARN("Swapchain formati degisti (%u -> %u); post render pass + pipeline yenileniyor",
               static_cast<unsigned>(mSwapFormat), static_cast<unsigned>(mSwapchain.format()));
    vkDestroyPipeline(mCtx->device(), mPostPipeline, nullptr);
    mPostPipeline = VK_NULL_HANDLE;
    vkDestroyRenderPass(mCtx->device(), mPostRenderPass, nullptr);
    mPostRenderPass = VK_NULL_HANDLE;
    createPostRenderPass(); // mSwapFormat'i günceller
    createPostPipeline();
  }
  createFramebuffers();     // sahne (HDR+depth)
  createPostResources();    // HDR view'lar değişti → descriptor'ları yeniden yaz
  createPostFramebuffers(); // swapchain hedefli
}

void Renderer::shutdown() {
  if (mCtx == nullptr || mCtx->device() == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(mCtx->device());

  destroySwapchainDependent();

  if (mCommandPool) vkDestroyCommandPool(mCtx->device(), mCommandPool, nullptr);
  mCommandPool = VK_NULL_HANDLE;

  mModel.destroy(*mCtx);

  if (mPipeline) vkDestroyPipeline(mCtx->device(), mPipeline, nullptr);
  mPipeline = VK_NULL_HANDLE;
  if (mPipelineLayout) vkDestroyPipelineLayout(mCtx->device(), mPipelineLayout, nullptr);
  mPipelineLayout = VK_NULL_HANDLE;
  if (mDescSetLayout) vkDestroyDescriptorSetLayout(mCtx->device(), mDescSetLayout, nullptr);
  mDescSetLayout = VK_NULL_HANDLE;
  if (mRenderPass) vkDestroyRenderPass(mCtx->device(), mRenderPass, nullptr);
  mRenderPass = VK_NULL_HANDLE;

  // Post-process (tonemap) pipeline ömürlü kaynakları.
  if (mHdrSampler) vkDestroySampler(mCtx->device(), mHdrSampler, nullptr);
  mHdrSampler = VK_NULL_HANDLE;
  if (mPostPipeline) vkDestroyPipeline(mCtx->device(), mPostPipeline, nullptr);
  mPostPipeline = VK_NULL_HANDLE;
  if (mPostPipelineLayout) vkDestroyPipelineLayout(mCtx->device(), mPostPipelineLayout, nullptr);
  mPostPipelineLayout = VK_NULL_HANDLE;
  if (mPostDescSetLayout) vkDestroyDescriptorSetLayout(mCtx->device(), mPostDescSetLayout, nullptr);
  mPostDescSetLayout = VK_NULL_HANDLE;
  if (mPostRenderPass) vkDestroyRenderPass(mCtx->device(), mPostRenderPass, nullptr);
  mPostRenderPass = VK_NULL_HANDLE;

  mSwapchain.destroy(*mCtx);
}

} // namespace metro::rhi
