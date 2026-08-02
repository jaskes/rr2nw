#pragma once

class CViewScene;

struct SRecoveredStaticMechanismSummary {
  bool initialized;
  bool targetLevel;
  int bindingCount;
  int waterwheelBindings;
  int flagBindings;
  int changedBindings;
  int sampledPoses;
  int restoredModifiers;
  unsigned long long fingerprint;
};

bool RecoveredStaticMechanism_Initialize(
    CViewScene* scene, const char* levelDirectory, double startTime,
    SRecoveredStaticMechanismSummary* summary);
void RecoveredStaticMechanism_Release();
const SRecoveredStaticMechanismSummary* RecoveredStaticMechanism_State();
const char* RecoveredStaticMechanism_LastError();
