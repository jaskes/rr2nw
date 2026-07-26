#include "SupervisorShutdownState.h"

namespace {

SSuaShutdownHooks g_suaShutdownHooks = {};
bool g_suaShutdownArmed = false;

}  // namespace

void SUA_ConfigureShutdown(const SSuaShutdownHooks& hooks) {
  g_suaShutdownHooks = hooks;
}

void SUA_ArmShutdown() {
  g_suaShutdownArmed = true;
}

bool SUA_IsShutdownArmed() {
  return g_suaShutdownArmed;
}

void SUA_DeinitEverything() {
  if (!g_suaShutdownArmed) return;

  g_suaShutdownArmed = false;
  if (g_suaShutdownHooks.closeVehiclePanel != 0) {
    g_suaShutdownHooks.closeVehiclePanel();
  }
  if (g_suaShutdownHooks.endSupervisorSeance != 0) {
    g_suaShutdownHooks.endSupervisorSeance();
  }
}
