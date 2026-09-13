#include "app/StationManifest.hpp"

#include <fstream>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <cstddef>
#include <utility>

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
  return end != begin && std::isfinite(value);
}

bool readPositiveNumber(const std::string& source, const char* key,
                        float& value) {
  float parsed = value;
  if (!readNumber(source, key, parsed)) return false;
  if (parsed <= 0.0f) return false;
  value = parsed;
  return true;
}

bool readNonNegativeInteger(const std::string& source, const char* key,
                            size_t& value) {
  float parsed = 0.0f;
  if (!readNumber(source, key, parsed) || parsed < 0.0f ||
      std::floor(parsed) != parsed) {
    return false;
  }
  value = static_cast<size_t>(parsed);
  return true;
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

bool readStops(const std::string& source, std::vector<sim::Stop>& stops) {
  const size_t key = source.find("\"stops\"");
  if (key == std::string::npos) return false;
  size_t cursor = source.find('[', key);
  const size_t end = cursor == std::string::npos ? std::string::npos
                                                  : source.find(']', cursor);
  if (cursor == std::string::npos || end == std::string::npos) return false;

  std::vector<sim::Stop> parsed;
  while (cursor < end) {
    const size_t objectStart = source.find('{', cursor);
    if (objectStart == std::string::npos || objectStart >= end) break;
    const size_t objectEnd = source.find('}', objectStart);
    if (objectEnd == std::string::npos || objectEnd > end) return false;
    const std::string object = source.substr(objectStart, objectEnd - objectStart + 1);
    sim::Stop stop;
    if (!readString(object, "name", stop.name) ||
        !readNumber(object, "position", stop.position) ||
        stop.position < 0.0f ||
        (!parsed.empty() && stop.position <= parsed.back().position)) {
      return false;
    }
    if (object.find("\"dwell_seconds\"") != std::string::npos &&
        (!readNumber(object, "dwell_seconds", stop.dwellSeconds) ||
         stop.dwellSeconds < 0.0f)) {
      return false;
    }
    parsed.push_back(std::move(stop));
    cursor = objectEnd + 1;
  }
  if (parsed.size() < 2) return false;
  stops = std::move(parsed);
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
  readString(source, "model", parsed.modelPath);
  if (source.find("\"position\"") != std::string::npos &&
      !readVector3(source, "position", parsed.spawnPosition)) {
    METRO_ERROR("Station manifest spawn konumu gecersiz: %s", path.c_str());
    return false;
  }
  if (source.find("\"yaw\"") != std::string::npos &&
      !readNumber(source, "yaw", parsed.spawnYaw)) {
    METRO_ERROR("Station manifest spawn yaw gecersiz: %s", path.c_str());
    return false;
  }
  if (source.find("\"pitch\"") != std::string::npos &&
      !readNumber(source, "pitch", parsed.spawnPitch)) {
    METRO_ERROR("Station manifest spawn pitch gecersiz: %s", path.c_str());
    return false;
  }
  if (parsed.spawnPitch < -89.0f || parsed.spawnPitch > 89.0f ||
      parsed.spawnYaw < -360.0f || parsed.spawnYaw > 360.0f) {
    METRO_ERROR("Station manifest kamera acisi aralik disinda: %s",
                path.c_str());
    return false;
  }
  if (source.find("\"block_count\"") != std::string::npos) {
    if (!readNonNegativeInteger(source, "block_count", parsed.blockCount) ||
        parsed.blockCount == 0) {
      METRO_ERROR("Station manifest blok sayisi gecersiz: %s", path.c_str());
      return false;
    }
  }
  float routeLength = parsed.routeLength;
  if (source.find("\"route_length\"") != std::string::npos) {
    if (!readPositiveNumber(source, "route_length", routeLength)) {
      METRO_ERROR("Station manifest rota uzunlugu gecersiz: %s", path.c_str());
      return false;
    }
    parsed.routeLength = routeLength;
  }
  if (source.find("\"passenger_capacity\"") != std::string::npos) {
    if (!readNonNegativeInteger(source, "passenger_capacity",
                                parsed.passengerCapacity) ||
        parsed.passengerCapacity == 0) {
      METRO_ERROR("Station manifest yolcu kapasitesi gecersiz: %s",
                  path.c_str());
      return false;
    }
  }
  if (source.find("\"initial_waiting_passengers\"") != std::string::npos &&
      !readNonNegativeInteger(source, "initial_waiting_passengers",
                              parsed.initialWaitingPassengers)) {
    METRO_ERROR("Station manifest baslangic yolcu sayisi gecersiz: %s",
                path.c_str());
    return false;
  }
  if (parsed.initialWaitingPassengers > parsed.passengerCapacity) {
    METRO_ERROR("Station manifest baslangic yolcu sayisi kapasiteyi asiyor: %s",
                path.c_str());
    return false;
  }
  float stopDwellSeconds = parsed.stopDwellSeconds;
  if (source.find("\"stop_dwell_seconds\"") != std::string::npos) {
    if (!readNumber(source, "stop_dwell_seconds", stopDwellSeconds) ||
        stopDwellSeconds < 0.0f) {
      METRO_ERROR("Station manifest genel dwell suresi gecersiz: %s",
                  path.c_str());
      return false;
    }
    parsed.stopDwellSeconds = stopDwellSeconds;
  }
  for (const auto& parameter : {
           std::pair{"max_speed_mps", &parsed.trainParameters.maxSpeed},
           std::pair{"acceleration_mps2", &parsed.trainParameters.acceleration},
           std::pair{"service_brake_mps2", &parsed.trainParameters.serviceBrake},
           std::pair{"rolling_resistance_mps2",
                     &parsed.trainParameters.rollingResistance}}) {
    if (source.find("\"" + std::string(parameter.first) + "\"") !=
            std::string::npos &&
        !readPositiveNumber(source, parameter.first, *parameter.second)) {
      METRO_ERROR("Station manifest tren parametresi gecersiz: %s (%s)",
                  parameter.first, path.c_str());
      return false;
    }
  }
  if (parsed.trainParameters.serviceBrake <
      parsed.trainParameters.acceleration) {
    METRO_ERROR("Station manifest servis freni ivmelenmeden dusuk: %s",
                path.c_str());
    return false;
  }
  if (source.find("\"stops\"") != std::string::npos &&
      !readStops(source, parsed.stops)) {
    METRO_ERROR("Station manifest durak listesi gecersiz: %s", path.c_str());
    return false;
  }
  if (!parsed.stops.empty() &&
      (std::abs(parsed.stops.front().position) > 0.01f ||
       std::abs(parsed.stops.back().position - parsed.routeLength) > 0.01f)) {
    METRO_ERROR("Station manifest duraklari rota sinirlariyla uyusmuyor: %s",
                path.c_str());
    return false;
  }

  out = std::move(parsed);
  METRO_INFO("Station manifest: %s (%s), mesh dizini: %s",
             out.name.c_str(), out.line.c_str(), out.meshDirectory.c_str());
  return true;
}

} // namespace metro::app
