#include "app/StationManifest.hpp"

#include <fstream>
#include <cstdlib>
#include <sstream>
#include <cstddef>

#include "core/Log.hpp"

namespace metro::app {

namespace {

bool readString(const std::string& source, const char* key, std::string& value) {
  const std::string marker = "\"" + std::string(key) + "\"";
  const size_t keyPosition = source.find(marker);
  if (keyPosition == std::string::npos) return false;
  const size_t colon = source.find(':', keyPosition + marker.size());
  if (colon == std::string::npos) return false;
  const size_t firstQuote = source.find('"', colon + 1);
  if (firstQuote == std::string::npos) return false;
  const size_t secondQuote = source.find('"', firstQuote + 1);
  if (secondQuote == std::string::npos) return false;
  value = source.substr(firstQuote + 1, secondQuote - firstQuote - 1);
  return !value.empty();
}

bool readNumber(const std::string& source, const char* key, float& value) {
  const std::string marker = "\"" + std::string(key) + "\"";
  const size_t keyPosition = source.find(marker);
  if (keyPosition == std::string::npos) return false;
  const size_t colon = source.find(':', keyPosition + marker.size());
  if (colon == std::string::npos) return false;
  char* end = nullptr;
  const char* begin = source.c_str() + colon + 1;
  value = std::strtof(begin, &end);
  return end != begin;
}

bool readVector3(const std::string& source, const char* key, glm::vec3& value) {
  const std::string marker = "\"" + std::string(key) + "\"";
  const size_t keyPosition = source.find(marker);
  if (keyPosition == std::string::npos) return false;
  const size_t open = source.find('[', keyPosition + marker.size());
  const size_t close = source.find(']', open);
  if (open == std::string::npos || close == std::string::npos) return false;
  std::string values = source.substr(open + 1, close - open - 1);
  char* cursor = values.data();
  char* end = nullptr;
  for (float* component : {&value.x, &value.y, &value.z}) {
    *component = std::strtof(cursor, &end);
    if (end == cursor) return false;
    cursor = end;
    while (*cursor == ' ' || *cursor == ',') ++cursor;
  }
  return true;
}

} // namespace

bool StationManifest::load(const std::string& path, StationManifest& out) {
  std::ifstream file(path);
  if (!file) {
    METRO_ERROR("Station manifest okunamadi: %s", path.c_str());
    return false;
  }

  std::ostringstream contents;
  contents << file.rdbuf();
  const std::string source = contents.str();

  StationManifest parsed;
  const bool valid =
      readString(source, "name", parsed.name) &&
      readString(source, "line", parsed.line) &&
      readString(source, "meshes", parsed.meshDirectory) &&
      readString(source, "materials", parsed.materialDirectory) &&
      readString(source, "textures", parsed.textureDirectory) &&
      readString(source, "lightmaps", parsed.lightmapDirectory);
  if (!valid) {
    METRO_ERROR("Station manifest eksik alan iceriyor: %s", path.c_str());
    return false;
  }
  readVector3(source, "position", parsed.spawnPosition);
  readNumber(source, "yaw", parsed.spawnYaw);
  readNumber(source, "pitch", parsed.spawnPitch);
  float blockCount = static_cast<float>(parsed.blockCount);
  if (readNumber(source, "block_count", blockCount) && blockCount >= 1.0f) {
    parsed.blockCount = static_cast<size_t>(blockCount);
  }
  readNumber(source, "route_length", parsed.routeLength);

  out = std::move(parsed);
  METRO_INFO("Station manifest: %s (%s), mesh dizini: %s",
             out.name.c_str(), out.line.c_str(), out.meshDirectory.c_str());
  return true;
}

} // namespace metro::app
