#pragma once

#include "mathlib.h"
#include "kernel/h/krtypes.h"

class SimulationContext;

struct SRecoveredVehicleControlReplayProbeSummary {
  int recordings = 0;
  int replays = 0;
  int codecRoundTrips = 0;
  int actionRecords = 0;
  int focusRecords = 0;
  int syntheticReleases = 0;
  int simulationFrames = 0;
  int stateMatches = 0;
  int clockMatches = 0;
  int randomMatches = 0;
  int rollbacks = 0;
  int hashMatches = 0;
  int hashSamples = 0;
  int densePresentationSamples = 0;
  int sparsePresentationSamples = 0;
  int denseSimulationTicks = 0;
  int sparseSimulationTicks = 0;
  int denseMaximumTicksPerPresentation = 0;
  int sparseMaximumTicksPerPresentation = 0;
  int sparseCatchUpSamples = 0;
  int cadenceBoundaryChecks = 0;
  int cadenceFocusResets = 0;
  int cadenceCappedSamples = 0;
  double cadenceDroppedSeconds = 0.0;
  unsigned int encodedBytes = 0;
  unsigned int replayEncodedBytes = 0;
  unsigned long long contentFingerprint = 0;
  unsigned long long journalFingerprint = 0;
  unsigned long long replayFingerprint = 0;
  unsigned long long hashStreamFingerprint = 0;
  unsigned long long recordedStateFingerprint = 0;
  unsigned long long replayedStateFingerprint = 0;
};

bool VehicleControlReplayProbe_Run(
    SimulationContext* context, const KR_ObjectID& vehicle,
    const CFVector3& position, double startTime,
    unsigned long long contentFingerprint,
    SRecoveredVehicleControlReplayProbeSummary* summary);
