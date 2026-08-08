#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "rsx.h"

#define LAST_H__VIEW
#include "game.h"
#include "h/super.h"
#include "obase/taxi/taxi.h"
#include "storage/h/classtab.h"
#include "suavik.h"
#include "TimeRuntimeState.h"

CViewObjectRef* pVesselObj = nullptr;

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-vehicle-services-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool SameGuid(const GUID& value, unsigned long data1, unsigned short data2,
              unsigned short data3, const unsigned char (&data4)[8]) {
  if (value.Data1 != data1 || value.Data2 != data2 || value.Data3 != data3) {
    return false;
  }
  for (int index = 0; index < 8; ++index) {
    if (value.Data4[index] != data4[index]) {
      return false;
    }
  }
  return true;
}

bool ExerciseRsxIdentities() {
  const unsigned char class_tail[8] = {0x98, 0x5d, 0x00, 0xaa,
                                       0x00, 0x3b, 0x43, 0xaf};
  const unsigned char interface_tail[8] = {0xa0, 0x0b, 0x44, 0x45,
                                           0x53, 0x54, 0x00, 0x00};
  return SameGuid(CLSID_RSXCACHEDEMITTER, 0x4b2ce920, 0x1c45, 0x11d0,
                  class_tail) &&
         SameGuid(CLSID_RSXDIRECTLISTENER, 0x4b2ce922, 0x1c45, 0x11d0,
                  class_tail) &&
         SameGuid(IID_IRSXCachedEmitter, 0xe78f762d, 0x96cb, 0x11cf,
                  interface_tail) &&
         SameGuid(IID_IRSXDirectListener, 0xe78f7634, 0x96cb, 0x11cf,
                  interface_tail);
}

bool ExerciseTaxiRegistry() {
  return ct_Storage::searchClassTable("TaxiAttr") == &__attrTaxiTable &&
         __attrTaxiTable.objectsType() == OBJECT_ATTRIBUTE;
}

bool ExerciseTimeService() {
  SUA_BindSession(nullptr);
  g_timer.m_aspect = 1.0;
  g_timer.m_curTime = 4000.0;
  g_timer.m_pauseTime = 0.0;
  g_timer.m_prevTime = 5000;
  g_timer.m_startTick = 5000;

  SUA_SkipTime(1.25);
  if (g_timer.m_curTime != 4001.25 || g_timer.m_startTick != 6250 ||
      g_timer.ConvertSysTime(6250) != 0.1 ||
      g_timer.ConvertSysTime(8250) != 2.0) {
    return false;
  }

  Session empty_session;
  SUA_BindSession(&empty_session);
  SUA_ProcessEvents();

  SimulationContext context(8, 8);
  Session stepped_session(&g_timer, nullptr);
  stepped_session.Add(&context);
  Session::m_simulationTick = 41u;
  Session::m_moment = 4.0;
  Session::m_viewTime = 4.0;
  Session::m_frameSec = 0.0;
  SUA_BindSession(&stepped_session);
  if (!SUA_ProcessEventsAt(4.025) ||
      Session::m_simulationTick != 42u ||
      std::fabs(Session::m_viewTime - 4.025) > 1.0e-9 ||
      std::fabs(Session::m_frameSec - 0.025) > 1.0e-9) {
    SUA_BindSession(nullptr);
    return false;
  }
  SSimulationClockState accepted;
  if (!SUA_CaptureSimulationClock(&accepted)) {
    SUA_BindSession(nullptr);
    return false;
  }
  if (SUA_ProcessEventsAt(4.0) ||
      SUA_ProcessEventsAt((std::numeric_limits<double>::quiet_NaN)()) ||
      Session::m_simulationTick != accepted.tick ||
      Session::m_viewTime != accepted.viewTime ||
      Session::m_frameSec != accepted.frameSeconds) {
    SUA_BindSession(nullptr);
    return false;
  }
  SUA_BindSession(nullptr);
  return true;
}

}  // namespace

int main() {
  static_assert(sizeof(void*) == 4, "vehicle services require Win32 ABI");

  if (!ExerciseRsxIdentities()) {
    return Fail("legacy RSX identities diverged");
  }
  if (!ExerciseTaxiRegistry()) {
    return Fail("TaxiAttr registry ownership diverged");
  }
  if (!ExerciseTimeService()) {
    return Fail("timer skip/session binding diverged");
  }

  std::cout << "legacy-vehicle-services-smoke: OK\n";
  return EXIT_SUCCESS;
}
