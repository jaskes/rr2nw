#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "kernel/h/context.h"
#include "obase/route/route.h"
#include "storage/h/savefile.h"

namespace {

constexpr int kDirectEvent = 1999;
constexpr int kQueuedEvent = 2000;

int Fail(const char* message) {
  std::cerr << "legacy-context-route-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool NearlyEqual(double lhs, double rhs) {
  return std::fabs(lhs - rhs) < 1e-9;
}

class ProbeObject final : public KR_Object {
 public:
  int receive_count = 0;
  int last_label = -1;

  int receiveEvent(KR_Event& event) override {
    ++receive_count;
    last_label = event.label;
    return 1;
  }

  bool shouldDump() override { return false; }
};

bool ExerciseContext() {
  SimulationContext context(8, 8);
  ProbeObject probe;
  KR_ObjectID id = context.addObject("context.probe", &probe);
  const char* symbolic_name = context.searchObject(id);
  if (id.isNUL() || probe.getContext() != &context ||
      context.searchObject("context.probe") != id ||
      symbolic_name == nullptr ||
      std::strcmp(symbolic_name, "context.probe") != 0 ||
      context.queryInterface(id, IUnknownIID) != &probe ||
      !context.isExist(id) || !context.isExist("context.probe") ||
      context.isExist("missing.object")) {
    return false;
  }

  KR_Event direct(kDirectEvent, 1.0, id, id);
  context.sendEventNow(direct);
  if (probe.receive_count != 1 || probe.last_label != kDirectEvent) {
    return false;
  }

  KR_Event queued(kQueuedEvent, 2.0, id, id);
  context.addEvent(queued);
  context.poll(3.0);
  if (probe.receive_count != 2 || probe.last_label != kQueuedEvent) {
    return false;
  }

  context.removeObject(id);
  return probe.getContext() == nullptr && probe.getObjectID().isNUL() &&
         !context.isExist(id) && !context.isExist("context.probe");
}

bool ExerciseRouteGeometry() {
  Route route;
  route.m_base = 0;
  route.m_nodeQnty = 3;
  route.m_totalNodePos = 3;
  route.m_node[0] = CFVector3(0.0, 0.0, 0.0);
  route.m_node[1] = CFVector3(10.0, 0.0, 0.0);
  route.m_node[2] = CFVector3(10.0, 0.0, 10.0);
  route.EvaluateLenght();

  const CFVector3 first_midpoint = route.GetPos(0.25);
  const CFVector3 second_midpoint = route.GetPos(0.75);
  return route.GetNodeCnt() == 3 && route.GetLen() == 20 &&
         NearlyEqual(first_midpoint.x, 5.0) &&
         NearlyEqual(first_midpoint.z, 0.0) &&
         NearlyEqual(second_midpoint.x, 10.0) &&
         NearlyEqual(second_midpoint.z, 5.0) &&
         route.queryInterface(IRouteObjectIID) != nullptr;
}

bool ExerciseRouteSave(const char* path) {
  Route::m_totalNodePos = 2;
  Route::m_node[0] = CFVector3(1.0, 2.0, 3.0);
  Route::m_node[1] = CFVector3(4.0, 5.0, 6.0);
  Route::m_napr[0] = CFVector3(7.0, 8.0, 9.0);
  Route::m_length[0] = 0.625;

  PIN_SaveFile output;
  if (!output.OpenWrite(const_cast<char*>(path)) ||
      !Route::SaveStaticData(output)) {
    output.Close();
    return false;
  }
  output.Close();

  Route::m_totalNodePos = 0;
  Route::m_node[0] = CFVector3();
  Route::m_node[1] = CFVector3();
  Route::m_napr[0] = CFVector3();
  Route::m_length[0] = 0.0;

  PIN_SaveFile input;
  if (!input.OpenRead(const_cast<char*>(path)) ||
      !Route::LoadStaticData(input)) {
    input.Close();
    return false;
  }
  input.Close();

  return Route::m_totalNodePos == 2 && NearlyEqual(Route::m_node[0].x, 1.0) &&
         NearlyEqual(Route::m_node[0].y, 2.0) &&
         NearlyEqual(Route::m_node[1].z, 6.0) &&
         NearlyEqual(Route::m_napr[0].y, 8.0) &&
         NearlyEqual(Route::m_length[0], 0.625);
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4, "context contract requires Win32");
  if (argc != 2) {
    return Fail("expected a temporary save path");
  }

  std::remove(argv[1]);
  if (!ExerciseContext()) {
    return Fail("SimulationContext lifecycle or event routing diverged");
  }
  if (!ExerciseRouteGeometry()) {
    return Fail("route geometry or interface contract diverged");
  }
  if (!ExerciseRouteSave(argv[1])) {
    return Fail("route static save round-trip diverged");
  }

  std::remove(argv[1]);
  std::cout << "legacy-context-route-smoke: OK\n";
  return EXIT_SUCCESS;
}
