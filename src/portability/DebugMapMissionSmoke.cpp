#include <cstdlib>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "dmap.h"
#include "kernel/h/context.h"
#include "message/hardmsg.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-debug-map-mission-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool ExerciseMissionPool() {
  DebugMap map;
  for (int i = 0; i < MAX_MISSIONS; ++i) {
    if (map.CreateMission("mission") != i) {
      return false;
    }
  }
  if (map.CreateMission("overflow") != -1) {
    return false;
  }

  map.DeleteMission(3);
  if (map.CreateMission("replacement") != 3) {
    return false;
  }

  map.ClearMission(-1);
  map.DeleteMission(MAX_MISSIONS);
  map.ClearMissions();
  return map.CreateMission("after-clear") == 0;
}

class HardwareProbe final : public KR_Object {
 public:
  int subscribe_count = 0;
  int unsubscribe_count = 0;
  KR_ObjectID subscriber = KR_ObjectID::NUL();
  int subscription_type = -1;

  int receiveEvent(KR_Event& event) override {
    if (event.label == CTRL_SUBSCRIBE) {
      event.data.open(EDO_READ)
          .getObjectID(subscriber)
          .getInt(subscription_type)
          .close();
      ++subscribe_count;
      return 1;
    }
    if (event.label == CTRL_UNSUBSCRIBE) {
      event.data.open(EDO_READ).getObjectID(subscriber).close();
      ++unsubscribe_count;
      return 1;
    }
    return 0;
  }

  bool shouldDump() override { return false; }
};

bool ExerciseHardwareProtocol() {
  SimulationContext context(8, 8);
  HardwareProbe hardware;
  DebugMap map;
  context.addObject("Hardware", &hardware);
  KR_ObjectID map_id = context.addObject("DebugMapSmoke", &map);
  context.poll(1.0);

  if (hardware.subscribe_count != 1 || hardware.subscriber != map_id ||
      hardware.subscription_type != EXCLUSIVE) {
    return false;
  }

  context.removeObject(map_id);
  const bool unsubscribed = hardware.unsubscribe_count == 1 &&
                            hardware.subscriber == map_id;
  context.removeObject(hardware.getObjectID());
  return unsubscribed;
}

}  // namespace

int main() {
  static_assert(sizeof(void*) == 4, "DebugMap contract requires Win32");
  if (!ExerciseMissionPool()) {
    return Fail("bounded mission pool lifecycle diverged");
  }
  if (!ExerciseHardwareProtocol()) {
    return Fail("hardware subscription protocol diverged");
  }

  std::cout << "legacy-debug-map-mission-smoke: OK\n";
  return EXIT_SUCCESS;
}
