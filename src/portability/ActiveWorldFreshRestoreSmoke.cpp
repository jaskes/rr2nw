#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

#include "ActiveWorldRuntimeProbe.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/comanmsg.h"
#include "message/groupmsg.h"
#include "obase/comander/CommanderState.h"
#include "obase/group/TankGroupState.h"
#include "storage/h/subject.h"

namespace {

class FreshMember : public ct_Object {
 public:
  int receiveEvent(KR_Event&) override { return 1; }
  bool shouldDump() override { return false; }
};

class FreshMemberTable : public ct_ClassTable {
 public:
  FreshMemberTable() : table_(nullptr) { registerClass("FreshMember"); }

  void allocObjects(int count) override {
    table_ = new (std::nothrow) FreshMember[count];
    if (table_ == nullptr) m_maxObjectQnty = 0;
  }
  void freeObjects() override {
    delete[] table_;
    table_ = nullptr;
    m_maxObjectQnty = 0;
  }
  ct_Object* getObjectPTR(int index) override {
    return index >= 0 && index < m_maxObjectQnty ? &table_[index] : nullptr;
  }

 private:
  FreshMember* table_;
};

FreshMemberTable g_freshMemberTable;

int Fail(const char* message) {
  std::fprintf(stderr, "active-world-fresh-restore-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool SendObjectID(SimulationContext* context, const KR_ObjectID& destination,
                  int label, const KR_ObjectID& value) {
  KR_Event event;
  event.label = label;
  event.source = g_arena.getObjectID();
  event.destination = destination;
  event.timeStamp = Session::m_moment;
  event.data.open(EDO_WRITE).putObjectID(value).close();
  context->sendEventNow(event);
  return true;
}

bool SendName(SimulationContext* context, const KR_ObjectID& destination,
              int label, const char* value) {
  KR_Event event;
  event.label = label;
  event.source = g_arena.getObjectID();
  event.destination = destination;
  event.timeStamp = Session::m_moment;
  event.data.open(EDO_WRITE).putStr(value).close();
  context->sendEventNow(event);
  return true;
}

void RemoveIfPresent(SimulationContext* context, const char* name) {
  if (context->isExist(name))
    context->removeObject(context->searchObject(name));
}

bool BuildSourceGraph(SimulationContext* context, KR_ObjectID* alpha,
                      KR_ObjectID* beta, KR_ObjectID* group,
                      KR_ObjectID* member) {
  const ct_ClassTableID commanderTable =
      g_arena.addClassTable("Commander", 2);
  const ct_ClassTableID groupAttributeTable =
      g_arena.addClassTable("TankGroupTable", 1);
  const ct_ClassTableID groupTable =
      g_arena.addClassTable("TankGroup", 2);
  const ct_ClassTableID memberTable =
      g_arena.addClassTable("FreshMember", 2);
  if (commanderTable == ct_NULLID || groupAttributeTable == ct_NULLID ||
      groupTable == ct_NULLID || memberTable == ct_NULLID)
    return false;
  KR_ObjectID attribute =
      g_arena.newObject(groupAttributeTable, "Group.Attr.Fresh");
  *alpha = g_arena.newObject(commanderTable, "Commander.Alpha");
  *beta = g_arena.newObject(commanderTable, "Commander.Beta");
  *group = g_arena.newObject(groupTable, "Group.Fresh");
  *member = g_arena.newObject(memberTable, "Tank.Dependency");
  if (attribute.isNUL() || alpha->isNUL() || beta->isNUL() ||
      group->isNUL() || member->isNUL())
    return false;

  SendObjectID(context, *group, KR_SET_ATTR, attribute);
  SendName(context, *alpha, COMMANDER_ADD_MEMBER_N, "Group.Fresh");
  SendObjectID(context, *alpha, com_EV_SETHOSTILECOMMANDER, *beta);
  SendObjectID(context, *beta, com_EV_SETFRIENDLYCOMMANDER, *alpha);
  SendObjectID(context, *group, GROUP_ADD_MEMBER, *member);
  return CommanderState_MemberCount(context, *alpha) == 1 &&
         CommanderState_IsHostile(context, *alpha, *beta) &&
         TankGroupState_Commander(context, *group) == *alpha &&
         TankGroupState_HasMember(context, *group, *member);
}

}  // namespace

int main() {
  Session::m_moment = 1.0;
  CommanderState_Link();
  TankGroupState_Link();
  SimulationContext context(64, 128);
  g_arena.openSeance(&context, 1024.0, 1024.0);

  KR_ObjectID alpha, beta, group, member;
  if (!BuildSourceGraph(&context, &alpha, &beta, &group, &member)) {
    g_arena.closeSeance();
    return Fail("could not build the symbolic source graph");
  }

  std::vector<std::uint8_t> bytes;
  SActiveWorldRuntimeProbeSummary capture;
  std::string failure;
  if (!ActiveWorldRuntime_CaptureProbe(
          &context, 0x4652455348435458ull, 17, Session::m_moment,
          "fresh-context", &bytes, &capture, &failure)) {
    g_arena.closeSeance();
    return Fail(failure.c_str());
  }
  const KR_ObjectID oldAlpha = alpha;
  const KR_ObjectID oldBeta = beta;
  const KR_ObjectID oldGroup = group;
  RemoveIfPresent(&context, "Group.Fresh");
  RemoveIfPresent(&context, "Commander.Beta");
  RemoveIfPresent(&context, "Commander.Alpha");
  if (context.isExist("Group.Fresh") || context.isExist("Commander.Alpha") ||
      context.isExist("Commander.Beta")) {
    g_arena.closeSeance();
    return Fail("source owners survived teardown");
  }

  SActiveWorldRuntimeProbeSummary restored = capture;
  if (!ActiveWorldRuntime_RestoreProbe(
          &context, bytes, &restored, &failure)) {
    g_arena.closeSeance();
    return Fail(failure.c_str());
  }
  alpha = context.searchObject("Commander.Alpha");
  beta = context.searchObject("Commander.Beta");
  group = context.searchObject("Group.Fresh");
  const bool fresh = restored.ready && restored.createdOwners == 3 &&
      restored.ownerPhases == 2 && restored.referencePhases == 2 &&
      alpha != oldAlpha && beta != oldBeta && group != oldGroup &&
      CommanderState_MemberCount(&context, alpha) == 1 &&
      CommanderState_IsHostile(&context, alpha, beta) &&
      TankGroupState_Commander(&context, group) == alpha &&
      TankGroupState_HasMember(&context, group, member);
  if (!fresh) {
    g_arena.closeSeance();
    return Fail("decoded owner allocation or symbolic linking diverged");
  }

  RemoveIfPresent(&context, "Group.Fresh");
  RemoveIfPresent(&context, "Commander.Beta");
  RemoveIfPresent(&context, "Commander.Alpha");
  RemoveIfPresent(&context, "Tank.Dependency");
  SActiveWorldRuntimeProbeSummary rejected = capture;
  failure.clear();
  const bool missingRejected =
      !ActiveWorldRuntime_RestoreProbe(&context, bytes, &rejected, &failure) &&
      !failure.empty() && !context.isExist("Group.Fresh") &&
      !context.isExist("Commander.Alpha") &&
      !context.isExist("Commander.Beta");

  KR_ObjectID conflicting =
      g_arena.newObject("FreshMember", "Commander.Alpha");
  SActiveWorldRuntimeProbeSummary collision = capture;
  failure.clear();
  const bool collisionRejected = !conflicting.isNUL() &&
      !ActiveWorldRuntime_RestoreProbe(
          &context, bytes, &collision, &failure) &&
      context.isExist(conflicting) &&
      context.searchObject("Commander.Alpha") == conflicting &&
      !context.isExist("Commander.Beta") && !context.isExist("Group.Fresh");

  g_arena.closeSeance();
  if (!missingRejected)
    return Fail("missing symbolic dependency did not roll owners back");
  if (!collisionRejected)
    return Fail("wrong-class symbolic collision mutated the live graph");
  std::printf(
      "active world fresh restore sections=2 created=3 refs=2 "
      "ids=reallocated missing-dependency=rollback collision=rejected "
      "fingerprint=%llu\n",
      static_cast<unsigned long long>(restored.worldFingerprint));
  return EXIT_SUCCESS;
}
