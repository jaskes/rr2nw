#ifndef RR2NW_RECOVERED_PRESENTATION_TRACE_H
#define RR2NW_RECOVERED_PRESENTATION_TRACE_H

#include <cstdint>

struct SRecoveredPresentationTraceEvent {
  std::uint64_t sequence;
  char source[48];
  char reason[64];
  char outcome[24];
  char asset[260];
};

void RecoveredPresentationTrace_Reset();
std::uint64_t RecoveredPresentationTrace_Record(const char* source,
                                                const char* reason,
                                                const char* outcome,
                                                const char* asset);
int RecoveredPresentationTrace_Count();
bool RecoveredPresentationTrace_Event(int index,
                                      SRecoveredPresentationTraceEvent* event);

#endif
