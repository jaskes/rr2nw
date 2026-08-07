#include "RecoveredPresentationTrace.h"

#include <array>
#include <cstdio>

namespace {

constexpr int kTraceCapacity = 32;
std::array<SRecoveredPresentationTraceEvent, kTraceCapacity> g_events = {};
int g_eventCount = 0;
std::uint64_t g_nextSequence = 1;

void CopyText(char* destination, int capacity, const char* source) {
  std::snprintf(destination, static_cast<std::size_t>(capacity), "%s",
                source == nullptr ? "" : source);
}

}  // namespace

void RecoveredPresentationTrace_Reset() {
  g_events = {};
  g_eventCount = 0;
  g_nextSequence = 1;
}

std::uint64_t RecoveredPresentationTrace_Record(const char* source,
                                                const char* reason,
                                                const char* outcome,
                                                const char* asset) {
  const std::uint64_t sequence = g_nextSequence++;
  int destination = 0;
  if (g_eventCount < kTraceCapacity) {
    destination = g_eventCount++;
  } else {
    for (int index = 1; index < kTraceCapacity; ++index)
      g_events[index - 1] = g_events[index];
    destination = kTraceCapacity - 1;
  }
  SRecoveredPresentationTraceEvent& event = g_events[destination];
  event = {};
  event.sequence = sequence;
  CopyText(event.source, sizeof(event.source), source);
  CopyText(event.reason, sizeof(event.reason), reason);
  CopyText(event.outcome, sizeof(event.outcome), outcome);
  CopyText(event.asset, sizeof(event.asset), asset);
  return sequence;
}

int RecoveredPresentationTrace_Count() { return g_eventCount; }

bool RecoveredPresentationTrace_Event(int index,
                                      SRecoveredPresentationTraceEvent* event) {
  if (event == nullptr || index < 0 || index >= g_eventCount) return false;
  *event = g_events[index];
  return true;
}
