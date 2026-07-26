#ifndef RR2NW_SUPERVISOR_SHUTDOWN_STATE_H
#define RR2NW_SUPERVISOR_SHUTDOWN_STATE_H

typedef void (*TSuaShutdownStep)();

struct SSuaShutdownHooks {
  TSuaShutdownStep closeVehiclePanel;
  TSuaShutdownStep endSupervisorSeance;
};

void SUA_ConfigureShutdown(const SSuaShutdownHooks& hooks);
void SUA_ArmShutdown();
bool SUA_IsShutdownArmed();
void SUA_DeinitEverything();

#endif  // RR2NW_SUPERVISOR_SHUTDOWN_STATE_H
