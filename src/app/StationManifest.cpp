#include "app/StationManifest.hpp"

#include <fstream>
#include <sstream>

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

  out = std::move(parsed);
  METRO_INFO("Station manifest: %s (%s), mesh dizini: %s",
             out.name.c_str(), out.line.c_str(), out.meshDirectory.c_str());
  return true;
}

} // namespace metro::app
