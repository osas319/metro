#include <exception>

#include "app/Application.hpp"
#include "core/Log.hpp"

int main() {
  try {
    metro::app::Application app;
    return app.run();
  } catch (const std::exception& e) {
    METRO_ERROR("Yakalanmamis istisna: %s", e.what());
    return 2;
  }
}
