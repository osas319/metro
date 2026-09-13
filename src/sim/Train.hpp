#pragma once

namespace metro::sim {

class Train {
public:
  void update(float dt, bool throttle, bool brake, bool signalClear = true,
              float routeLength = 0.0f);

  float speed() const { return mSpeed; }
  float position() const { return mPosition; }
  bool doorsOpen() const { return mDoorsOpen; }
  void setDoorsOpen(bool open) { mDoorsOpen = open; }

private:
  float mPosition = 0.0f;
  float mSpeed = 0.0f;
  bool mDoorsOpen = false;
};

} // namespace metro::sim
