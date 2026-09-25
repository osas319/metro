#pragma once

namespace metro::sim {

class Train {
public:
  struct Parameters {
    float maxSpeed = 22.2f;
    float acceleration = 1.2f;
    float serviceBrake = 2.4f;
    float emergencyBrake = 3.6f;
    float rollingResistance = 0.08f;
    float maxJerk = 3.0f;
    float tractionResponse = 4.0f;
  };

  void setParameters(Parameters parameters);
  void reset(float position = 0.0f);
  void update(float dt, bool throttle, bool brake, bool signalClear = true,
              float routeLength = 0.0f, bool emergencyBrake = false);

  float speed() const { return mSpeed; }
  float acceleration() const { return mAcceleration; }
  float position() const { return mPosition; }
  bool doorsOpen() const { return mDoorsOpen; }
  float doorOpenFraction() const { return mDoorOpenFraction; }
  bool requestDoorsOpen(bool open, bool platformAligned);

  float maxSpeed() const { return mParameters.maxSpeed; }
  float serviceBrake() const { return mParameters.serviceBrake; }
  float brakingDistance() const;
  float recommendedSpeed(float distanceMeters) const;

private:
  float mPosition = 0.0f;
  float mSpeed = 0.0f;
  float mAcceleration = 0.0f;
  bool mDoorsOpen = false;
  float mDoorOpenFraction = 0.0f;
  float mTractionCommand = 0.0f;
  float mBrakeCommand = 0.0f;
  Parameters mParameters;
};

} // namespace metro::sim
