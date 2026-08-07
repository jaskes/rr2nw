#ifndef RR2NW_RECOVERED_LEGACY_SCRIPT_HOST_H
#define RR2NW_RECOVERED_LEGACY_SCRIPT_HOST_H

#include "kernel/h/object.h"
#include "sc.h"

#include <string>
#include <vector>

class ct_Arena;

void RecoveredLegacyScriptHost_SetRouteCapacityFloor(int capacity);

enum ERecoveredLegacyScriptHostIssue {
  RECOVERED_LEGACY_SCRIPT_HOST_EVENT_POOL_EXHAUSTED = 1u << 0,
  RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT = 1u << 1,
  RECOVERED_LEGACY_SCRIPT_HOST_ARENA_UNAVAILABLE = 1u << 2,
  RECOVERED_LEGACY_SCRIPT_HOST_CLASS_TABLE_FAILURE = 1u << 3,
  RECOVERED_LEGACY_SCRIPT_HOST_OBJECT_CREATION_FAILURE = 1u << 4,
  RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_INTERFACE_FAILURE = 1u << 5,
  RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE = 1u << 6,
  RECOVERED_LEGACY_SCRIPT_HOST_INVALID_STACK_REFERENCE = 1u << 7,
  RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_TABLE_FAILURE = 1u << 8,
  RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_NODE_FAILURE = 1u << 9,
  RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_DATA_FAILURE = 1u << 10,
  RECOVERED_LEGACY_SCRIPT_HOST_PROJECT_CREATION_FAILURE = 1u << 11,
  RECOVERED_LEGACY_SCRIPT_HOST_UNSUPPORTED_OPERATION = 1u << 12,
  RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT_DESTINATION = 1u << 13,
  RECOVERED_LEGACY_SCRIPT_HOST_HOWITZER_HOLDER_FAILURE = 1u << 14
};

class RecoveredLegacyScriptHost {
 public:
  enum { kConstantCount = 36 };

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
  void WriteVector(int eventIndex, double x, double y, double z);
  void WriteString(int eventIndex, const char* value);
  void WriteObjectID(int eventIndex, const KR_ObjectID& object);
  void IssueEvent(int eventIndex, int label, double timeStamp,
                  const KR_ObjectID& destination);
  void SendEventNow(int eventIndex, int label,
                    const KR_ObjectID& destination);
  KR_ObjectID SearchObject(const char* name);
  int SearchClassTable(const char* name);
  bool RemoveObject(const char* name, double from);
  bool ForceRemoveObject(const char* name);
  int AddClassTable(const char* name, int capacity);
  KR_ObjectID NewObject(int classTable, const char* name);
  KR_ObjectID NewObject(const char* className, const char* name);
  KR_ObjectID LoadRoute(int classTable, const char* fileName,
                        const char* routeName);
  int SetCommander(const char* objectName, const char* commanderName);
  bool SetCommanderRelation(const KR_ObjectID& commander,
                            const KR_ObjectID& relativeCommander,
                            bool hostile);
  double CurrentTime() const;
  bool UpdateAttributes();
  int SetDamage(const char* objectName, double damage);
  bool DeleteHowitzer(const char* holderName);
  void Unsupported(const char* operation);
  void BeginObjectTransaction();
  int ReclaimUnreferencedRoutes(const KR_ObjectID* preserved,
                                int preservedCount);
  bool RollbackObjectTransaction();
  void CommitObjectTransaction();
  int TransactionCreatedObjectCount() const;
  bool CreateProjectTable(int projectCapacity, int nodeCapacity,
                          int heapCapacity);
  int NewProjectNode(int command, int left, int right);
  bool OpenProjectData(int node);
  bool CloseProjectData(int node);
  bool ProjectWriteInt(int node, int value);
  bool ProjectWriteFloat(int node, double value);
  bool ProjectWriteString(int node, const char* value);
  bool ProjectNodeSetLink(int node, int left, int right);
  int ProjectNodeNull() const;
  KR_ObjectID NewProject(const char* name, int node, bool permanent);
  void DeferMissionHowitzer(int classTable, const char* attributeName,
                            const char* holderName, double startTime,
                            const char* objectName);
  void DeferMissionDestroyable(const char* attributeName,
                               const char* scriptName,
                               const char* objectName);
  bool ProjectTableCreated() const;
  int ProjectNodeCount() const;
  int ProjectCount() const;
  int ProjectDataBytes() const;
  int DeferredMissionHowitzerCount() const;
  int DeferredMissionDestroyableCount() const;
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

  struct ReclaimedRoute {
    std::string name;
    std::vector<double> coordinates;
  };

  ScriptEvent* Event(int eventIndex, const char* operation);
  bool ArenaReady(const char* operation);
  bool ProjectNodeValid(int node, bool allowNull = false) const;
  bool ReserveProjectData(int node, int bytes);
  void Report(unsigned int issue, const char* message);

  ct_Arena* m_arena;
  unsigned int m_issues;
  char m_lastError[256];
  ScriptEvent m_events[kEventCount];
  bool m_projectTableCreated;
  bool m_projectDataOpen;
  int m_projectCapacity;
  int m_projectNodeCapacity;
  int m_projectHeapCapacity;
  int m_projectNodeCount;
  int m_projectCount;
  int m_projectDataBytes;
  int m_openProjectNode;
  int m_deferredMissionHowitzerCount;
  int m_deferredMissionDestroyableCount;
  bool m_objectTransactionActive;
  std::vector<KR_ObjectID> m_transactionCreatedObjects;
  std::vector<KR_ObjectID> m_transactionExistingRoutes;
  std::vector<KR_ObjectID> m_transactionPinnedRoutes;
  std::vector<ReclaimedRoute> m_transactionReclaimedRoutes;
};

#endif
