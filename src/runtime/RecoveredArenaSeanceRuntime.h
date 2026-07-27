#ifndef RR2NW_RECOVERED_ARENA_SEANCE_RUNTIME_H
#define RR2NW_RECOVERED_ARENA_SEANCE_RUNTIME_H

class SimulationContext;

enum ERecoveredArenaSeanceIssue {
  RECOVERED_ARENA_SEANCE_INVALID_CONTEXT = 1u << 0,
  RECOVERED_ARENA_SEANCE_OPEN_FAILURE = 1u << 1,
  RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE = 1u << 2,
  RECOVERED_ARENA_SEANCE_SCRIPT_INITIALIZATION_FAILURE = 1u << 3,
  RECOVERED_ARENA_SEANCE_SCRIPT_COMPILE_FAILURE = 1u << 4,
  RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE = 1u << 5,
  RECOVERED_ARENA_SEANCE_SCRIPT_TIMEOUT = 1u << 6,
  RECOVERED_ARENA_SEANCE_VEHICLE_TABLE_MISSING = 1u << 7,
  RECOVERED_ARENA_SEANCE_VEHICLE_OBJECT_MISSING = 1u << 8,
  RECOVERED_ARENA_SEANCE_VEHICLE_INTERFACE_MISSING = 1u << 9,
  RECOVERED_ARENA_SEANCE_SCRIPT_HOST_FAILURE = 1u << 10
};

int RecoveredArenaSeance_Initialize(SimulationContext* context,
                                    double startTime);
void RecoveredArenaSeance_Release();
bool RecoveredArenaSeance_IsOpen();
bool RecoveredArenaSeance_ScriptCompleted();
bool RecoveredArenaSeance_VehicleReady();
unsigned int RecoveredArenaSeance_Issues();
const char* RecoveredArenaSeance_LastError();

#endif
