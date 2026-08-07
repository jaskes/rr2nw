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
#include "storage/h/subject.h"

namespace {

constexpr int kDirectEvent = 1999;
constexpr int kQueuedEvent = 2000;
constexpr int kEqualFirstEvent = 2001;
constexpr int kEqualSecondEvent = 2002;

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

class OverflowProbeObject final : public ct_Object {
 public:
  void Bind(ct_ClassTable* master, int index) {
    m_master = master;
    m_index = index;
  }

  int receiveEvent(KR_Event&) override { return 1; }
  bool shouldDump() override { return false; }
};

class OverflowProbeTable final : public ct_ClassTable {
 public:
  OverflowProbeTable() { registerClass("ContextOverflowProbe"); }

 protected:
  void allocObjects(int count) override {
    objects_ = new OverflowProbeObject[count];
    for (int index = 0; index < count; ++index) {
      objects_[index].Bind(this, index);
    }
  }

  void freeObjects() override {
    delete[] objects_;
    objects_ = nullptr;
  }

  ct_Object* getObjectPTR(int index) override { return &objects_[index]; }

 private:
  OverflowProbeObject* objects_ = nullptr;
};

OverflowProbeTable g_overflow_probe_table;

bool ExerciseContext() {
  SimulationContext context(8, 8);
  ProbeObject probe;
  ProbeObject restored_probe;
  KR_ObjectID id = context.addObject("context.probe", &probe);
  const char* symbolic_name = context.searchObject(id);
  if (id.isNUL() || probe.getContext() != &context ||
      context.objectFreeCount() != 7 ||
      context.searchObject("context.probe") != id ||
      symbolic_name == nullptr ||
      std::strcmp(symbolic_name, "context.probe") != 0 ||
      context.queryInterface(id, IUnknownIID) != &probe ||
      !context.isExist(id) || !context.isExist("context.probe") ||
      context.isExist("missing.object")) {
    return false;
  }

  KR_ObjectID rejected_id =
      context.addObject("context.restored", &restored_probe, id);
  if (!rejected_id.isNUL() || restored_probe.getContext() != nullptr ||
      context.objectFreeCount() != 7 || !context.isExist(id) ||
      context.searchObject("context.probe") != id ||
      context.isExist("context.restored")) {
    return false;
  }

  KR_Event direct(kDirectEvent, 1.0, id, id);
  context.sendEventNow(direct);
  if (probe.receive_count != 1 || probe.last_label != kDirectEvent) {
    return false;
  }

  KR_Event queued(kQueuedEvent, 2.0, id, id);
  context.addEvent(queued);
  KR_Event copied[2];
  const int copied_count = context.copyEvents(kQueuedEvent, id, copied, 2);
  if (copied_count != 1 || copied[0].label != kQueuedEvent ||
      copied[0].source != id || copied[0].destination != id ||
      !NearlyEqual(copied[0].timeStamp, 2.0) || copied[0].data.size() != 0 ||
      context.copyEvents(kQueuedEvent, id, nullptr, 0) != 1 ||
      context.copyEvents(kQueuedEvent, id, nullptr, 1) != -1) {
    return false;
  }
  context.poll(3.0);
  if (probe.receive_count != 2 || probe.last_label != kQueuedEvent) {
    return false;
  }

  KR_Event equal_first(kEqualFirstEvent, 4.0, id, id);
  KR_Event equal_second(kEqualSecondEvent, 4.0, KR_ObjectID::NUL(), id);
  context.addEvent(equal_first);
  context.addEvent(equal_second);
  KR_Event all[3];
  KR_Event destination[2];
  if (context.eventCount() != 2 || context.eventFreeCount() != 6 ||
      context.copyAllEvents(all, 3) != 2 ||
      all[0].label != kEqualSecondEvent ||
      all[1].label != kEqualFirstEvent ||
      context.copyEventsTo(kEqualSecondEvent, id, destination, 2) != 1 ||
      destination[0].source != KR_ObjectID::NUL() ||
      context.removeEventsTo(kEqualSecondEvent, id) != 1 ||
      context.copyEventsTo(kEqualSecondEvent, id, nullptr, 0) != 0 ||
      context.eventCount() != 1 || context.eventFreeCount() != 7 ||
      context.removeEventsTo(kEqualFirstEvent, id) != 1 ||
      context.eventCount() != 0 || context.eventFreeCount() != 8) {
    return false;
  }

  context.removeObject(id);
  if (probe.getContext() != nullptr || !probe.getObjectID().isNUL() ||
      context.isExist(id) || context.isExist("context.probe") ||
      context.objectFreeCount() != 8) {
    return false;
  }

  const KR_ObjectID restored_id =
      context.addObject("context.restored", &restored_probe, id);
  if (restored_id != id || restored_probe.getContext() != &context ||
      context.objectFreeCount() != 7 || !context.isExist(id) ||
      context.searchObject("context.restored") != id) {
    return false;
  }

  context.clearObjects();
  return restored_probe.getContext() == nullptr &&
         restored_probe.getObjectID().isNUL() && !context.isExist(id) &&
         !context.isExist("context.restored") &&
         context.objectFreeCount() == 8;
}

bool ExerciseClassTableEviction() {
  SimulationContext context(8, 16);
  ct_Arena arena;
  arena.openSeance(&context, 128.0, 128.0);
  const ct_ClassTableID table =
      arena.addClassTable("ContextOverflowProbe", 1);
  if (table == ct_NULLID) {
    return false;
  }
  g_overflow_probe_table.setAddMode(CT_KILLFIRST);

  KR_ObjectID first = arena.newObject(table, "overflow.first");
  KR_ObjectID second = arena.newObject(table, "overflow.second");
  if (first.isNUL() || second.isNUL() || first == second ||
      context.isExist(first) || context.isExist("overflow.first") ||
      !context.isExist(second) ||
      context.searchObject("overflow.second") != second ||
      context.objectFreeCount() != 14) {
    return false;
  }

  arena.closeSeance();
  context.clearObjects();
  return context.objectFreeCount() == 16 &&
         !context.isExist("overflow.second");
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
  if (!ExerciseClassTableEviction()) {
    return Fail("class-table eviction left stale context ownership");
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
