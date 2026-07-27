#ifndef RR2NW_RECOVERED_LEGACY_SCRIPT_HOST_H
#define RR2NW_RECOVERED_LEGACY_SCRIPT_HOST_H

#include "kernel/h/object.h"
#include "sc.h"

class ct_Arena;

enum ERecoveredLegacyScriptHostIssue {
  RECOVERED_LEGACY_SCRIPT_HOST_EVENT_POOL_EXHAUSTED = 1u << 0,
  RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT = 1u << 1,
  RECOVERED_LEGACY_SCRIPT_HOST_ARENA_UNAVAILABLE = 1u << 2,
  RECOVERED_LEGACY_SCRIPT_HOST_CLASS_TABLE_FAILURE = 1u << 3,
  RECOVERED_LEGACY_SCRIPT_HOST_OBJECT_CREATION_FAILURE = 1u << 4,
  RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_INTERFACE_FAILURE = 1u << 5,
  RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE = 1u << 6,
  RECOVERED_LEGACY_SCRIPT_HOST_INVALID_STACK_REFERENCE = 1u << 7
};

class RecoveredLegacyScriptHost {
 public:
  enum { kConstantCount = 12 };

  explicit RecoveredLegacyScriptHost(ct_Arena* arena);

  void Reset();
  bool IsHealthy() const;
  unsigned int Issues() const;
  const char* LastError() const;

  int OpenEventData(s_EventDataOpen style);
  void CloseEventData(int eventIndex);
  void DescendEventData(int eventIndex, int tag, int index);
  void AscendEventData(int eventIndex);
  void WriteInt(int eventIndex, int value);
  void WriteFloat(int eventIndex, double value);
  void WriteString(int eventIndex, const char* value);
  void WriteObjectID(int eventIndex, const KR_ObjectID& object);
  void SendEventNow(int eventIndex, int label,
                    const KR_ObjectID& destination);
  KR_ObjectID SearchObject(const char* name);
  int AddClassTable(const char* name, int capacity);
  KR_ObjectID NewObject(int classTable, const char* name);
  KR_ObjectID NewObject(const char* className, const char* name);
  KR_ObjectID LoadRoute(int classTable, const char* fileName,
                        const char* routeName);
  bool WriteScriptInteger(TProcessContext* process, int reference,
                          int value);

  static TLinkExtern* Bindings();
  static TLinkConstExtern* Constants();
  static void ResetConstantLinks();
  static int CopyLinkedConstants(TLinkConstExtern* destination,
                                 int capacity);

 private:
  enum { kEventCount = 8 };

  struct ScriptEvent : public KR_Event {
    bool inUse;
    ScriptEvent() : inUse(false) {}
  };

  ScriptEvent* Event(int eventIndex, const char* operation);
  bool ArenaReady(const char* operation);
  void Report(unsigned int issue, const char* message);

  ct_Arena* m_arena;
  unsigned int m_issues;
  char m_lastError[256];
  ScriptEvent m_events[kEventCount];
};

#endif
