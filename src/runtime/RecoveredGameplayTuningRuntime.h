#pragma once

#include <cstddef>
#include <cstdint>

class SimulationContext;

struct SRecoveredGameplayTuningSummary {
  int schemaVersion = 0;
  unsigned int vehiclePatchCount = 0;
  unsigned int projectilePatchCount = 0;
  unsigned int peoplePatchCount = 0;
  unsigned int tankPatchCount = 0;
  unsigned int projectileBallisticProofs = 0;
  unsigned int projectileBallisticMoves = 0;
  unsigned int secondaryProjectileReferenceProofs = 0;
  unsigned int secondaryProjectileBallisticProofs = 0;
  unsigned int secondaryProjectileBallisticMoves = 0;
  unsigned int peopleLifecycleProofs = 0;
  unsigned int tankLifecycleProofs = 0;
  std::uint64_t tuningFingerprint = 0;
  std::uint64_t vehicleAttributeFingerprint = 0;
  std::uint64_t vehicleReferenceFingerprint = 0;
  std::uint64_t bulletAttributeFingerprint = 0;
  std::uint64_t peopleGameplayFingerprint = 0;
  std::uint64_t tankGameplayFingerprint = 0;
  int defaultVehiclePresent = 0;
  double defaultMaxSpeed = 0.0;
  double defaultReverseSpeed = 0.0;
  double defaultAccelerationTime = 0.0;
  double defaultTurnSpeed = 0.0;
  double defaultPrimaryFireInterval = 0.0;
  double defaultSecondaryFireInterval = 0.0;
  double defaultDamagePower = 0.0;
  char defaultSecondaryProjectile[64] = {};
  int primaryProjectilePresent = 0;
  double primaryProjectileSpeed = 0.0;
  char observedPeople[64] = {};
  double observedPeopleMovementSpeed = 0.0;
  double observedPeopleInitialHealth = 0.0;
  double observedPeopleFireInterval = 0.0;
  int observedPeopleBurstCount = 0;
  char observedTank[64] = {};
  double observedTankMaxSpeed = 0.0;
  double observedTankAttackPower = 0.0;
  double observedTankAttackDelay = 0.0;
};

enum ERecoveredGameplayTuningIssue {
  RECOVERED_GAMEPLAY_TUNING_IO_FAILURE = 1u << 0,
  RECOVERED_GAMEPLAY_TUNING_TOO_LARGE = 1u << 1,
  RECOVERED_GAMEPLAY_TUNING_MALFORMED = 1u << 2,
  RECOVERED_GAMEPLAY_TUNING_UNSUPPORTED_SCHEMA = 1u << 3,
  RECOVERED_GAMEPLAY_TUNING_INVALID_VALUE = 1u << 4,
  RECOVERED_GAMEPLAY_TUNING_DUPLICATE_TARGET = 1u << 5,
  RECOVERED_GAMEPLAY_TUNING_RETAIL_ROSTER_MISMATCH = 1u << 6,
  RECOVERED_GAMEPLAY_TUNING_UNKNOWN_TARGET = 1u << 7,
  RECOVERED_GAMEPLAY_TUNING_UNSUPPORTED_DYNAMIC = 1u << 8,
  RECOVERED_GAMEPLAY_TUNING_TRANSACTION_FAILURE = 1u << 9,
  RECOVERED_GAMEPLAY_TUNING_ALLOCATION_FAILURE = 1u << 10
};

// Applies the reserved RR2NW/gameplay-tuning.json overlay after the retail
// Vehicle/Bullet/People/Tank attribute rosters exist and before references
// resolve or live subjects are created.
// An active mod without that exact manifest target is a valid no-tuning case.
bool RecoveredGameplayTuning_Apply(SimulationContext* context);
bool RecoveredGameplayTuning_FinalizeVehicleReferences(
    SimulationContext* context);
bool RecoveredGameplayTuning_FinalizeTankLifecycle(
    SimulationContext* context, double timeStamp);
bool RecoveredGameplayTuning_FinalizePeopleLifecycle(
    SimulationContext* context, double timeStamp);
void RecoveredGameplayTuning_Release(SimulationContext* context);
bool RecoveredGameplayTuning_IsActive();
unsigned int RecoveredGameplayTuning_Issues();
const char* RecoveredGameplayTuning_LastError();
const SRecoveredGameplayTuningSummary* RecoveredGameplayTuning_Summary();

// Pure schema validator used by tooling/tests before a retail roster exists.
// Runtime application performs the additional symbolic-object/dynamic checks.
bool RecoveredGameplayTuning_ValidateText(
    const char* text, std::size_t length, char* error,
    std::size_t errorSize);

// Post-tuning roster admission never blesses arbitrary data: Apply first
// proves the untouched retail roster, and these gates then require the exact
// committed post-transaction fingerprint plus resolved symbolic references.
bool RecoveredGameplayTuning_AcceptsVehicleRoster(
    SimulationContext* context);
bool RecoveredGameplayTuning_AcceptsBulletRoster(
    SimulationContext* context);
bool RecoveredGameplayTuning_AcceptsVehicleReferences(
    SimulationContext* context);
bool RecoveredGameplayTuning_AcceptsBulletReferences(
    SimulationContext* context);
bool RecoveredGameplayTuning_AcceptsPeopleRoster(
    SimulationContext* context);
bool RecoveredGameplayTuning_AcceptsTankRoster(
    SimulationContext* context);
