#include "sim/Signal.hpp"
#include "sim/Train.hpp"
#include "sim/PassengerSystem.hpp"
#include "sim/Route.hpp"
#include "sim/PhysicsWorld.hpp"
#include "sim/PassengerNavGraph.hpp"
#include "audio/AudioEventQueue.hpp"
#include "app/StationManifest.hpp"

#include <cassert>
#include <limits>
#include <vector>

int main() {
  metro::sim::Train train;
  train.update(1.0f, true, false);
  assert(train.speed() > 0.0f);
  assert(train.position() > 0.0f);

  assert(!train.requestDoorsOpen(true, false));
  assert(!train.doorsOpen());
  train.update(2.0f, false, true);
  assert(train.speed() == 0.0f);
  assert(train.requestDoorsOpen(true, true));
  assert(train.doorsOpen());
  train.update(1.0f, true, false);
  assert(train.speed() == 0.0f);
  assert(train.requestDoorsOpen(false, false));
  assert(!train.doorsOpen());

  metro::sim::Train signalTrain;
  signalTrain.update(1.0f, true, false, false);
  assert(signalTrain.speed() == 0.0f);
  assert(signalTrain.position() == 0.0f);

  metro::sim::Train terminalTrain;
  terminalTrain.update(1.0f, true, false, true, 0.5f);
  terminalTrain.update(1.0f, true, false, true, 0.5f);
  assert(terminalTrain.position() <= 0.5f);
  assert(terminalTrain.speed() == 0.0f);

  metro::sim::Train brakingTrain;
  for (int i = 0; i < 600 && brakingTrain.position() < 25.0f; ++i) {
    brakingTrain.update(1.0f / 60.0f, true, false, true, 25.0f);
  }
  assert(brakingTrain.position() <= 25.0f);
  assert(brakingTrain.speed() == 0.0f);

  metro::sim::Train largeFrameTrain;
  largeFrameTrain.update(10.0f, true, false, true, 5.0f);
  assert(largeFrameTrain.position() == 5.0f);
  assert(largeFrameTrain.speed() == 0.0f);

  metro::sim::PhysicsWorld physics;
  metro::sim::Train fixedStepTrain;
  physics.step(0.5f, fixedStepTrain, true, false, true, 100.0f);
  assert(fixedStepTrain.position() > 0.0f);
  assert(physics.interpolatedPosition(fixedStepTrain) <=
         fixedStepTrain.position());
  assert(physics.interpolatedPosition(fixedStepTrain) >= 0.0f);
  assert(physics.interpolationAlpha() >= 0.0f &&
         physics.interpolationAlpha() < 1.0f);
  metro::sim::PhysicsWorld guardedPhysics(
      {.fixedStep = 0.0f,
       .maxSubsteps = 0,
       .trainWidth = -1.0f,
       .trainHeight = 0.0f,
       .trackGauge = std::numeric_limits<float>::quiet_NaN()});
  assert(guardedPhysics.fixedStep() > 0.0f);
  guardedPhysics.step(0.1f, fixedStepTrain, true, false, true, 100.0f);
  assert(guardedPhysics.interpolationAlpha() >= 0.0f &&
         guardedPhysics.interpolationAlpha() < 1.0f);
  guardedPhysics.reset(fixedStepTrain.position());
  assert(guardedPhysics.interpolationAlpha() == 0.0f);
  assert(guardedPhysics.interpolatedPosition(fixedStepTrain) ==
         fixedStepTrain.position());
  guardedPhysics.reset(std::numeric_limits<float>::quiet_NaN());
  assert(guardedPhysics.interpolatedPosition(fixedStepTrain) == 0.0f);

  metro::sim::Train guardedTrain;
  guardedTrain.setParameters({.maxSpeed = -1.0f,
                              .acceleration = 0.0f,
                              .serviceBrake = 0.1f,
                              .rollingResistance = -1.0f});
  guardedTrain.update(1.0f, true, false);
  assert(guardedTrain.speed() > 0.0f);

  metro::sim::PhysicsWorld stopPhysics;
  metro::sim::Train stopTrain;
  for (int i = 0; i < 600 && stopTrain.position() < 25.0f; ++i) {
    stopPhysics.step(1.0f / 60.0f, stopTrain, true, false, true, 25.0f);
  }
  assert(stopTrain.position() == 25.0f);
  assert(stopTrain.speed() == 0.0f);

  metro::sim::BlockSignal signal(3);
  assert(signal.blockCount() == 3);
  assert(signal.canEnter(1));

  metro::sim::PassengerSystem passengers(2);
  passengers.update(0.5f, true, true);
  assert(passengers.onboard() == 1);
  passengers.update(0.5f, true, true);
  assert(passengers.onboard() == 2);
  passengers.update(1.0f, true, true);
  assert(passengers.onboard() == 2);
  passengers.unloadAtTerminal();
  assert(passengers.onboard() == 0);
  assert(passengers.alightedTotal() == 2);
  passengers.update(1.0f, true, true, false);
  assert(passengers.onboard() == 0);
  metro::sim::PassengerSystem stopPassengers(16, 16);
  stopPassengers.update(4.0f, true, true);
  assert(stopPassengers.onboard() == 8);
  stopPassengers.serviceStop(1, 4, false);
  assert(stopPassengers.alightedAtStops() > 0);
  assert(stopPassengers.onboard() < 8);
  stopPassengers.serviceStop(3, 4, true);
  assert(stopPassengers.onboard() == 0);
  stopPassengers.setBoardingDestination(2);

  metro::sim::PassengerNavGraph navGraph;
  assert(navGraph.buildLinear(4, 900.0f));
  assert(navGraph.nodeCount() == 4);
  assert(navGraph.node(4) == nullptr);
  const auto navPath = navGraph.shortestPath(0, 3);
  assert((navPath == std::vector<size_t>{0, 1, 2, 3}));
  assert(navGraph.shortestPath(3, 3).size() == 1);
  metro::sim::PassengerAgent walker(navGraph.shortestPath(0, 3), 300.0f);
  walker.update(1.0f, navGraph);
  assert(walker.node() == 1);
  assert(walker.progress() == 0.0f);
  assert(walker.state() == metro::sim::PassengerAgent::State::Walking);
  const auto walkerPosition = walker.position(navGraph);
  assert(walkerPosition.y < -290.0f && walkerPosition.y > -310.0f);
  walker.update(2.0f, navGraph);
  assert(walker.node() == 3);
  assert(walker.progress() == 1.0f);
  assert(walker.state() == metro::sim::PassengerAgent::State::Arrived);
  metro::sim::PassengerSystem walkingPassengers;
  walkingPassengers.setNavGraph(navGraph);
  assert(walkingPassengers.addWalkingAgent(0, 2, 300.0f) == 0);
  assert(walkingPassengers.addWalkingAgent(3, 99) ==
         std::numeric_limits<size_t>::max());
  walkingPassengers.updateWalkingAgents(2.0f);
  assert(walkingPassengers.walkingAgents().front().node() == 2);
  assert(walkingPassengers.walkingAgents().front().state() ==
         metro::sim::PassengerAgent::State::Arrived);
  assert(walkingPassengers.activeWalkingAgents() == 0);
  metro::audio::AudioEventQueue audioEvents;
  audioEvents.push({metro::audio::EventType::DoorOpened, 1});
  audioEvents.push({metro::audio::EventType::StopArrived, 1});
  metro::audio::Event audioEvent{};
  assert(audioEvents.pending() == 2);
  assert(audioEvents.tryPop(audioEvent));
  assert(audioEvent.type == metro::audio::EventType::DoorOpened);
  assert(audioEvents.tryPop(audioEvent));
  assert(audioEvents.pending() == 0);

  metro::sim::Route route;
  route.buildBlockStops(3, 900.0f);
  assert(route.stopCount() == 4);
  assert(route.stops().front().position == 0.0f);
  assert(route.stops().back().position == 900.0f);
  assert(route.nextStopPosition(0.0f, 900.0f) == 300.0f);
  assert(route.nextStopPosition(300.0f, 900.0f) == 600.0f);
  assert(route.nextStopPosition(900.0f, 900.0f) == 900.0f);
  assert(route.nextStop(0.0f, 900.0f).name == "M4 duragi 1");
  route.setStops({{"Kadikoy", 0.0f}, {"Pendik", 450.0f},
                  {"Sabiha Gokcen Havalimani", 900.0f}});
  assert(route.stopCount() == 3);
  assert(route.nextStop(0.0f, 900.0f).name == "Pendik");
  route.setStops({{"Invalid", 0.0f}, {"Out of order", 450.0f},
                  {"Invalid terminal", 400.0f}});
  assert(route.stopCount() == 3);
  assert(route.nextStop(0.0f, 900.0f).name == "Pendik");
  route.setStops({{"Invalid", -1.0f}, {"Terminal", 900.0f}});
  assert(route.stopCount() == 3);
  route.setStops({{"Invalid", 0.0f}, {"Terminal", 900.0f, -2.0f}});
  assert(route.stopCount() == 3);
  signal.setOccupied(1, true);
  assert(!signal.canEnter(1));
  assert(signal.canEnter(5));
  const auto occupiedBlocks = signal.occupiedBlocks();
  assert(occupiedBlocks.size() == 3);
  assert(!occupiedBlocks[0]);
  assert(occupiedBlocks[1]);
  assert(!occupiedBlocks[2]);
  signal.resize(5);
  assert(signal.blockCount() == 5);
  assert(signal.canEnter(1));
  assert(signal.canEnter(5));

  metro::app::StationManifest station;
  assert(metro::app::StationManifest::load(
      "assets/stations/kadikoy/station.json", station));
  assert(station.stops.size() == 23);
  assert(station.stops.front().name == "Kadikoy");
  assert(station.stops.back().name == "Sabiha Gokcen Havalimani");
  assert(station.stops.back().position == station.routeLength);
  assert(station.stops.front().dwellSeconds == 3.0f);
  assert(station.stops.back().dwellSeconds == 0.0f);
  assert(station.modelPath == "assets/box.glb");
  assert(station.initialWaitingPassengers == 24);
  assert(station.blockCount == 8);
  assert(station.passengerCapacity == 320);
  assert(station.routeLength == 2000.0f);
  assert(station.platformWidth == 4.0f);
  assert(station.trackGauge == 2.4f);
  assert(station.trainWidth == 2.8f);
  assert(station.trainHeight == 3.2f);
  assert(station.columnSpacing == 24.0f);
  assert(station.platformEdgePosition == glm::vec3(0.0f));
  assert(station.stopPosition == glm::vec3(0.0f));
  assert(station.stopDwellSeconds == 3.0f);
  assert(station.physicsFixedStep > 0.016f &&
         station.physicsFixedStep < 0.017f);
  assert(station.physicsMaxSubsteps == 4);
  assert(station.trainParameters.serviceBrake >=
         station.trainParameters.acceleration);
  assert(station.spawnPosition.y == 1.7f);
  assert(station.spawnYaw == -90.0f);
  assert(station.spawnPitch == 0.0f);
  return 0;
}
