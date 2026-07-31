#pragma once

#include <cstddef>
#include <cstdint>

class SimulationContext;

struct SRecoveredScriptEventSummary {
  int schemaVersion = 0;
  unsigned int eventCount = 0;
  unsigned int explosionCount = 0;
  unsigned int sparkCount = 0;
  unsigned int queuedCount = 0;
  unsigned int semanticProofCount = 0;
  std::uint64_t eventFingerprint = 0;
  double minimumDelay = 0.0;
  double maximumDelay = 0.0;
};

enum ERecoveredScriptEventIssue {
  RECOVERED_SCRIPT_EVENT_IO_FAILURE = 1u << 0,
  RECOVERED_SCRIPT_EVENT_TOO_LARGE = 1u << 1,
  RECOVERED_SCRIPT_EVENT_MALFORMED = 1u << 2,
  RECOVERED_SCRIPT_EVENT_UNSUPPORTED_SCHEMA = 1u << 3,
  RECOVERED_SCRIPT_EVENT_DUPLICATE_ID = 1u << 4,
  RECOVERED_SCRIPT_EVENT_UNKNOWN_ATTRIBUTE = 1u << 5,
  RECOVERED_SCRIPT_EVENT_RUNTIME_UNAVAILABLE = 1u << 6,
  RECOVERED_SCRIPT_EVENT_CAPACITY_FAILURE = 1u << 7,
  RECOVERED_SCRIPT_EVENT_TRANSACTION_FAILURE = 1u << 8,
  RECOVERED_SCRIPT_EVENT_SEMANTIC_PROOF_FAILURE = 1u << 9,
  RECOVERED_SCRIPT_EVENT_ALLOCATION_FAILURE = 1u << 10
};

// Applies the reserved RR2NW/script-events.json contract after the recovered
// Explosion/Spark attribute, subject and active-world layers are ready.
// Schema 1 deliberately exposes only finite delayed Explosion/Spark creation;
// raw legacy labels, payload bytes and object authority remain private.
bool RecoveredScriptEvents_Apply(SimulationContext* context,
                                 double startTime);
void RecoveredScriptEvents_Release(SimulationContext* context);
bool RecoveredScriptEvents_IsActive();
unsigned int RecoveredScriptEvents_Issues();
const char* RecoveredScriptEvents_LastError();
const SRecoveredScriptEventSummary* RecoveredScriptEvents_Summary();

// Pure parser/schema admission for hermetic tooling and tests. Runtime Apply
// additionally resolves the named retail attributes and proves EVT1 capture.
bool RecoveredScriptEvents_ValidateText(const char* text,
                                        std::size_t length,
                                        char* error,
                                        std::size_t errorSize);
