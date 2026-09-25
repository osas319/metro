#pragma once

namespace metro::sim {

class Train {
public:
  struct Parameters {
    float maxSpeed = 22.2f;
    float acceleration = 1.2f;
    float serviceBrake = 2.4f;
    float rollingResistance = 0.08f;
  };

  void setParameters(Parameters parameters);
  void update(float dt, bool throttle, bool brake, bool signalClear = true,
              float routeLength = 0.0f);

  float speed() const { return mSpeed; }
  float position() const { return mPosition; }
  bool doorsOpen() const { return mDoorsOpen; }
  float doorOpenFraction() const { return mDoorOpenFraction; }
  bool requestDoorsOpen(bool open, bool platformAligned);

private:
  float mPosition = 0.0f;
  float mSpeed = 0.0f;
  bool mDoorsOpen = false;
  float mDoorOpenFraction = 0.0f;
  Parameters mParameters;
};

} // namespace metro::sim
