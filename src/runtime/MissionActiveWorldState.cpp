#include "MissionActiveWorldState.h"

#include "RecoveredLevelRuntime.h"
#include "dmap.h"
#include "h/vehicle.h"
#include "i/route.i"
#include "kernel/h/context.h"
#include "message/recrcenmsg.h"
#include "obase/recrcen/RecruitCenterSubjectState.h"
#include "obase/people/PeopleActiveWorldState.h"
#include "mproj/h/mproj.h"
#include "storage/h/subject.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

const std::uint32_t kMissionMagic = 0x3148534du;  // MSH1
const std::uint32_t kMissionLegacyVersion = 1u;
const std::uint32_t kMissionRouteGeometryVersion = 2u;
const std::uint32_t kMissionArtefactVersion = 3u;
const std::uint32_t kMissionProjectRosterVersion = 4u;
const std::uint32_t kMissionVersion = kMissionProjectRosterVersion;
const std::size_t kMaximumMissions = 6;
const std::size_t kMaximumProjects = 200;
const int kMaximumProjectTreeNode = 1024 * 1024;
const std::size_t kMaximumReferences = KR_SetOfID::MAX_ID_CNT;
const std::size_t kMaximumSymbolic = MAX_SYMBOLIC_LENGHT - 1;
const char kProbeName[] = "Active World Mission Probe";

enum ReferenceKind {
  kReferenceTombstone = 0,
  kReferenceSymbolic = 1
};

struct StableReference {
  ReferenceKind kind;
  std::string name;

  StableReference() : kind(kReferenceTombstone) {}
};

struct StableReached {
  StableReference object;
  CFVector2 position;
  double radius;

  StableReached() : position(0.0, 0.0), radius(0.0) {}
};

struct StableMission {
  int successFirst;
  int status;
  int giveArtefact;
  StableReference project;
  StableReference commander;
  int hasSummary;
  std::string summaryName;
  std::string summaryText;
  double widthStart;
  double widthEnd;
  std::uint32_t color;
  StableReference route;
  unsigned long long routeGeometryFingerprint;
  std::vector<StableReference> successKill;
  std::vector<StableReference> successLive;
  std::vector<StableReached> successReached;
  std::vector<StableReference> failureKill;
  std::vector<StableReference> failureLive;
  std::vector<StableReached> failureReached;

  StableMission()
      : successFirst(1), status(MISSION_NONE), giveArtefact(0), hasSummary(0),
        widthStart(0.0), widthEnd(0.0), color(0),
        routeGeometryFingerprint(0) {}
};

struct StableProject {
  std::string name;
  int treeNode;
  int permanent;

  StableProject() : treeNode(-1), permanent(0) {}
};

struct StableState {
  std::string vehicle;
  int totalMissionCount;
  std::vector<StableMission> missions;
  std::vector<StableProject> projects;
  bool projectRosterPresent;

  StableState() : totalMissionCount(0), projectRosterPresent(false) {}
};

struct ResolvedMission {
  KR_ObjectID project;
  KR_ObjectID commander;
  KR_ObjectID route;
  std::vector<KR_ObjectID> successKill;
  std::vector<KR_ObjectID> successLive;
  std::vector<KR_ObjectID> successReached;
  std::vector<KR_ObjectID> failureKill;
  std::vector<KR_ObjectID> failureLive;
  std::vector<KR_ObjectID> failureReached;
};

struct Writer {
  std::vector<unsigned char> *bytes;

  void U32(std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
      bytes->push_back(static_cast<unsigned char>(value >> shift));
  }
  void I32(int value) { U32(static_cast<std::uint32_t>(value)); }
  void Double(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 64; shift += 8)
      bytes->push_back(static_cast<unsigned char>(bits >> shift));
  }
  bool String(const std::string &value, std::size_t maximum) {
    if (value.size() > maximum || value.find('\0') != std::string::npos)
      return false;
    U32(static_cast<std::uint32_t>(value.size()));
    bytes->insert(bytes->end(), value.begin(), value.end());
    return true;
  }
};

struct Reader {
  const std::vector<unsigned char> &bytes;
  std::size_t offset;

  explicit Reader(const std::vector<unsigned char> &source)
      : bytes(source), offset(0) {}
  bool U32(std::uint32_t *value) {
    if (value == NULL || offset > bytes.size() || bytes.size() - offset < 4)
      return false;
    *value = 0;
    for (int shift = 0; shift < 32; shift += 8)
      *value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    return true;
  }
  bool I32(int *value) {
    std::uint32_t encoded = 0;
    if (value == NULL || !U32(&encoded))
      return false;
    *value = static_cast<int>(encoded);
    return true;
  }
  bool Double(double *value) {
    if (value == NULL || offset > bytes.size() || bytes.size() - offset < 8)
      return false;
    std::uint64_t bits = 0;
    for (int shift = 0; shift < 64; shift += 8)
      bits |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
  }
  bool String(std::string *value, std::size_t maximum) {
    std::uint32_t size = 0;
    if (value == NULL || !U32(&size) || size > maximum ||
        offset > bytes.size() || bytes.size() - offset < size)
      return false;
    value->assign(reinterpret_cast<const char *>(&bytes[offset]), size);
    offset += size;
    return value->find('\0') == std::string::npos;
  }
};

std::string g_lastFailure;

bool Fail(const std::string &message) {
  g_lastFailure = message;
  return false;
}

bool IsNul(const KR_ObjectID &object) {
  KR_ObjectID copy = object;
  return copy.isNUL() != 0;
}

bool Finite(double value) { return std::isfinite(value) != 0; }

Vehicle *ResolveVehicle(SimulationContext *context,
                        const std::string &name = std::string()) {
  if (context == NULL)
    return NULL;
  KR_ObjectID object = KR_ObjectID::NUL();
  if (!name.empty() && context->isExist(name.c_str()))
    object = context->searchObject(name.c_str());
  else if (name.empty() && g_vehicle != NULL &&
           g_vehicle->getContext() == context &&
           context->isExist(g_vehicle->getObjectID()))
    object = g_vehicle->getObjectID();
  if (IsNul(object))
    return NULL;
  return static_cast<Vehicle *>(context->queryInterface(object, IVehicleIID));
}

std::string ObjectName(SimulationContext *context,
                       const KR_ObjectID &object) {
  const char *name = context == NULL || IsNul(object) ||
                             !context->isExist(object)
                         ? NULL
                         : context->searchObject(object);
  return name == NULL ? std::string() : std::string(name);
}

bool CaptureReference(SimulationContext *context, const KR_ObjectID &object,
                      StableReference *reference) {
  if (context == NULL || reference == NULL)
    return false;
  reference->name.clear();
  if (IsNul(object) || !context->isExist(object)) {
    reference->kind = kReferenceTombstone;
    return true;
  }
  reference->name = ObjectName(context, object);
  if (reference->name.empty() ||
      context->searchObject(reference->name.c_str()) != object)
    return false;
  reference->kind = kReferenceSymbolic;
  return true;
}

unsigned long long RouteGeometryFingerprint(
    SimulationContext *context, const KR_ObjectID &object) {
  if (context == NULL || IsNul(object) || !context->isExist(object))
    return 0;
  IRouteObject *route = static_cast<IRouteObject *>(
      context->queryInterface(object, IRouteObjectIID));
  if (route == NULL || route->GetNodeCnt() < 2)
    return 0;
  std::vector<double> coordinates;
  coordinates.reserve(static_cast<std::size_t>(route->GetNodeCnt()) * 3);
  for (int index = 0; index < route->GetNodeCnt(); ++index) {
    const CFVector3 node = route->GetNode(index);
    coordinates.push_back(node.x);
    coordinates.push_back(node.y);
    coordinates.push_back(node.z);
  }
  return PeopleActiveWorldState_RouteGeometryFingerprint(
      coordinates.data(), coordinates.size());
}

bool ResolveReference(SimulationContext *context,
                      const StableReference &reference,
                      KR_ObjectID *object) {
  if (context == NULL || object == NULL)
    return false;
  if (reference.kind == kReferenceTombstone) {
    *object = KR_ObjectID::NUL();
    return true;
  }
  if (reference.name.empty() || !context->isExist(reference.name.c_str()))
    return false;
  *object = context->searchObject(reference.name.c_str());
  return ObjectName(context, *object) == reference.name;
}

template <std::size_t N>
bool FixedString(const char (&value)[N], std::string *result) {
  const char *end = static_cast<const char *>(std::memchr(value, 0, N));
  if (result == NULL || end == NULL)
    return false;
  result->assign(value, static_cast<std::size_t>(end - value));
  return true;
}

bool CaptureReferenceSet(SimulationContext *context, const KR_SetOfID &set,
                         std::vector<StableReference> *references) {
  if (references == NULL || set.getCount() < 0 ||
      set.getCount() > KR_SetOfID::MAX_ID_CNT)
    return false;
  references->clear();
  for (int index = 0; index < set.getCount(); ++index) {
    StableReference reference;
    if (!CaptureReference(context, set[index], &reference))
      return false;
    references->push_back(reference);
  }
  return true;
}

bool CaptureReachedSet(SimulationContext *context, const KR_SetOfID &set,
                       const CFVector2 *positions, const double *radii,
                       std::vector<StableReached> *references) {
  if (references == NULL || positions == NULL || radii == NULL ||
      set.getCount() < 0 || set.getCount() > KR_SetOfID::MAX_ID_CNT)
    return false;
  references->clear();
  for (int index = 0; index < set.getCount(); ++index) {
    StableReached reached;
    if (!CaptureReference(context, set[index], &reached.object) ||
        !Finite(positions[index].x) || !Finite(positions[index].y) ||
        !Finite(radii[index]) || radii[index] < 0.0)
      return false;
    reached.position = positions[index];
    reached.radius = radii[index];
    references->push_back(reached);
  }
  return true;
}

struct ProjectCollector {
  SimulationContext *context;
  std::vector<StableProject> *projects;
  bool valid;

  ProjectCollector(SimulationContext *value,
                   std::vector<StableProject> *destination)
      : context(value), projects(destination), valid(true) {}
};

bool CollectProject(KR_ObjectID object, void *user) {
  ProjectCollector *collector = static_cast<ProjectCollector *>(user);
  if (collector == NULL || collector->context == NULL ||
      collector->projects == NULL) {
    return false;
  }
  mp_Project *project = projectTable.searchProject(object);
  const std::string name = ObjectName(collector->context, object);
  if (project == NULL || name.empty() ||
      collector->projects->size() >= kMaximumProjects) {
    collector->valid = false;
    return false;
  }
  StableProject saved;
  saved.name = name;
  saved.treeNode = project->m_treeNode;
  saved.permanent = project->m_permanent;
  collector->projects->push_back(saved);
  return true;
}

bool CaptureProjectRoster(SimulationContext *context,
                          std::vector<StableProject> *projects) {
  if (context == NULL || projects == NULL)
    return false;
  projects->clear();
  ProjectCollector collector(context, projects);
  projectTable.userFind(CollectProject, &collector);
  if (!collector.valid)
    return false;
  std::sort(projects->begin(), projects->end(),
            [](const StableProject &left, const StableProject &right) {
              return left.name < right.name;
            });
  for (std::size_t index = 1; index < projects->size(); ++index)
    if ((*projects)[index - 1].name == (*projects)[index].name)
      return false;
  return true;
}

bool CaptureState(SimulationContext *context, StableState *state) {
  if (context == NULL || state == NULL)
    return false;
  *state = StableState();
  if (!CaptureProjectRoster(context, &state->projects))
    return Fail("MSH1 ProjectTable roster is invalid");
  state->projectRosterPresent = true;
  Vehicle *vehicle = ResolveVehicle(context);
  if (vehicle == NULL)
    return true;
  state->vehicle = ObjectName(context, vehicle->getObjectID());
  if (state->vehicle.empty() || g_vehicle != vehicle)
    return Fail("MSH1 playable Vehicle identity is unresolved");
  Player &player = static_cast<Player &>(vehicle->player());
  if (player.m_missCnt < 0 || player.m_missCnt > 6 ||
      player.m_total_misCount < player.m_missCnt)
    return Fail("MSH1 Player mission counters are invalid");
  state->totalMissionCount = player.m_total_misCount;
  for (int index = 0; index < player.m_missCnt; ++index) {
    const PlayerMission &source = player.m_mission[index];
    StableMission mission;
    mission.successFirst = source.success_filed;
    mission.status = static_cast<int>(source.m_status);
    mission.giveArtefact = source.m_giveArtefact;
    mission.hasSummary = source.m_missionInfoExist;
    if (!CaptureReference(context, source.mID, &mission.project) ||
        !CaptureReference(context, source.comID, &mission.commander) ||
        !CaptureReferenceSet(context, source.success_needKill,
                             &mission.successKill) ||
        !CaptureReferenceSet(context, source.success_needLive,
                             &mission.successLive) ||
        !CaptureReachedSet(context, source.success_needReached,
                           source.success_reachedPos,
                           source.success_reachedRadius,
                           &mission.successReached) ||
        !CaptureReferenceSet(context, source.filed_needKill,
                             &mission.failureKill) ||
        !CaptureReferenceSet(context, source.filed_needLive,
                             &mission.failureLive) ||
        !CaptureReachedSet(context, source.filed_needReached,
                           source.filed_reachedPos,
                           source.filed_reachedRadius,
                           &mission.failureReached))
      return Fail("MSH1 mission condition reference is invalid");
    if (mission.hasSummary) {
      if (!FixedString(source.m_missionName, &mission.summaryName) ||
          !FixedString(source.m_missionText, &mission.summaryText) ||
          !CaptureReference(context, source.m_missionRouteID,
                            &mission.route))
        return Fail("MSH1 mission summary is not bounded");
      if (mission.route.kind == kReferenceSymbolic) {
        mission.routeGeometryFingerprint = RouteGeometryFingerprint(
            context, source.m_missionRouteID);
        if (mission.routeGeometryFingerprint == 0)
          return Fail("MSH1 mission Route geometry is unresolved");
      }
      mission.widthStart = source.m_missionsw;
      mission.widthEnd = source.m_missionew;
      mission.color = static_cast<std::uint32_t>(source.m_missionrgb);
    }
    state->missions.push_back(mission);
  }
  return true;
}

bool EncodeReference(Writer *writer, const StableReference &reference) {
  if (writer == NULL)
    return false;
  writer->U32(static_cast<std::uint32_t>(reference.kind));
  return reference.kind == kReferenceTombstone ||
         writer->String(reference.name, kMaximumSymbolic);
}

bool EncodeReferenceSet(Writer *writer,
                        const std::vector<StableReference> &references) {
  if (writer == NULL || references.size() > kMaximumReferences)
    return false;
  writer->U32(static_cast<std::uint32_t>(references.size()));
  for (std::size_t index = 0; index < references.size(); ++index)
    if (!EncodeReference(writer, references[index]))
      return false;
  return true;
}

bool EncodeReachedSet(Writer *writer,
                      const std::vector<StableReached> &references) {
  if (writer == NULL || references.size() > kMaximumReferences)
    return false;
  writer->U32(static_cast<std::uint32_t>(references.size()));
  for (std::size_t index = 0; index < references.size(); ++index) {
    if (!EncodeReference(writer, references[index].object))
      return false;
    writer->Double(references[index].position.x);
    writer->Double(references[index].position.y);
    writer->Double(references[index].radius);
  }
  return true;
}

bool EncodeStateVersion(const StableState &state, std::uint32_t version,
                        std::vector<unsigned char> *bytes) {
  if (bytes == NULL || version < kMissionLegacyVersion ||
      version > kMissionVersion)
    return false;
  bytes->clear();
  Writer writer = {bytes};
  writer.U32(kMissionMagic);
  writer.U32(version);
  if (!writer.String(state.vehicle, kMaximumSymbolic))
    return false;
  writer.I32(state.totalMissionCount);
  writer.U32(static_cast<std::uint32_t>(state.missions.size()));
  for (std::size_t index = 0; index < state.missions.size(); ++index) {
    const StableMission &mission = state.missions[index];
    writer.I32(mission.successFirst);
    writer.I32(mission.status);
    if (version >= kMissionArtefactVersion)
      writer.I32(mission.giveArtefact);
    if (!EncodeReference(&writer, mission.project) ||
        !EncodeReference(&writer, mission.commander))
      return false;
    writer.I32(mission.hasSummary);
    if (mission.hasSummary) {
      if (!writer.String(mission.summaryName, 79) ||
          !writer.String(mission.summaryText, MAX_TEXT_LEN - 1))
        return false;
      writer.Double(mission.widthStart);
      writer.Double(mission.widthEnd);
      writer.U32(mission.color);
      if (!EncodeReference(&writer, mission.route))
        return false;
      if (version >= kMissionRouteGeometryVersion) {
        writer.U32(static_cast<std::uint32_t>(
            mission.routeGeometryFingerprint));
        writer.U32(static_cast<std::uint32_t>(
            mission.routeGeometryFingerprint >> 32));
      }
    }
    if (!EncodeReferenceSet(&writer, mission.successKill) ||
        !EncodeReferenceSet(&writer, mission.successLive) ||
        !EncodeReachedSet(&writer, mission.successReached) ||
        !EncodeReferenceSet(&writer, mission.failureKill) ||
        !EncodeReferenceSet(&writer, mission.failureLive) ||
        !EncodeReachedSet(&writer, mission.failureReached))
      return false;
  }
  if (version >= kMissionProjectRosterVersion) {
    if (!state.projectRosterPresent || state.projects.size() > kMaximumProjects)
      return false;
    writer.U32(static_cast<std::uint32_t>(state.projects.size()));
    for (std::size_t index = 0; index < state.projects.size(); ++index) {
      const StableProject &project = state.projects[index];
      if (!writer.String(project.name, kMaximumSymbolic))
        return false;
      writer.I32(project.treeNode);
      writer.I32(project.permanent);
    }
  }
  return true;
}

bool EncodeState(const StableState &state, std::vector<unsigned char> *bytes) {
  return EncodeStateVersion(state, kMissionVersion, bytes);
}

bool DecodeReference(Reader *reader, StableReference *reference) {
  std::uint32_t kind = 0;
  if (reader == NULL || reference == NULL || !reader->U32(&kind) ||
      kind > kReferenceSymbolic)
    return false;
  reference->kind = static_cast<ReferenceKind>(kind);
  reference->name.clear();
  return reference->kind == kReferenceTombstone ||
         reader->String(&reference->name, kMaximumSymbolic);
}

bool DecodeReferenceSet(Reader *reader,
                        std::vector<StableReference> *references) {
  std::uint32_t count = 0;
  if (reader == NULL || references == NULL || !reader->U32(&count) ||
      count > kMaximumReferences)
    return false;
  references->resize(count);
  for (std::size_t index = 0; index < references->size(); ++index)
    if (!DecodeReference(reader, &(*references)[index]))
      return false;
  return true;
}

bool DecodeReachedSet(Reader *reader,
                      std::vector<StableReached> *references) {
  std::uint32_t count = 0;
  if (reader == NULL || references == NULL || !reader->U32(&count) ||
      count > kMaximumReferences)
    return false;
  references->resize(count);
  for (std::size_t index = 0; index < references->size(); ++index)
    if (!DecodeReference(reader, &(*references)[index].object) ||
        !reader->Double(&(*references)[index].position.x) ||
        !reader->Double(&(*references)[index].position.y) ||
        !reader->Double(&(*references)[index].radius))
      return false;
  return true;
}

bool ValidateReference(const StableReference &reference) {
  return (reference.kind == kReferenceTombstone && reference.name.empty()) ||
         (reference.kind == kReferenceSymbolic && !reference.name.empty() &&
          reference.name.size() <= kMaximumSymbolic);
}

bool ValidateReferenceSet(
    const std::vector<StableReference> &references) {
  if (references.size() > kMaximumReferences)
    return false;
  for (std::size_t index = 0; index < references.size(); ++index)
    if (!ValidateReference(references[index]))
      return false;
  return true;
}

bool ValidateReachedSet(const std::vector<StableReached> &references) {
  if (references.size() > kMaximumReferences)
    return false;
  for (std::size_t index = 0; index < references.size(); ++index)
    if (!ValidateReference(references[index].object) ||
        !Finite(references[index].position.x) ||
        !Finite(references[index].position.y) ||
        !Finite(references[index].radius) || references[index].radius < 0.0)
      return false;
  return true;
}

bool ValidateState(const StableState &state) {
  if (state.vehicle.size() > kMaximumSymbolic ||
      state.totalMissionCount < 0 ||
      state.missions.size() > kMaximumMissions ||
      state.totalMissionCount < static_cast<int>(state.missions.size()) ||
      (state.vehicle.empty() &&
       (!state.missions.empty() || state.totalMissionCount != 0)) ||
      (state.projectRosterPresent &&
       state.projects.size() > kMaximumProjects))
    return false;
  if (state.projectRosterPresent) {
    for (std::size_t index = 0; index < state.projects.size(); ++index) {
      const StableProject &project = state.projects[index];
      if (project.name.empty() || project.name.size() > kMaximumSymbolic ||
          project.treeNode < -1 ||
          project.treeNode > kMaximumProjectTreeNode ||
          (project.permanent != 0 && project.permanent != 1) ||
          (index > 0 && state.projects[index - 1].name >= project.name))
        return false;
    }
  }
  for (std::size_t index = 0; index < state.missions.size(); ++index) {
    const StableMission &mission = state.missions[index];
    if ((mission.successFirst != 0 && mission.successFirst != 1) ||
        mission.status < MISSION_NONE || mission.status > MISSION_SURRENDER ||
        (mission.giveArtefact != 0 && mission.giveArtefact != 1) ||
        !ValidateReference(mission.project) ||
        !ValidateReference(mission.commander) ||
        (mission.hasSummary != 0 && mission.hasSummary != 1) ||
        !ValidateReferenceSet(mission.successKill) ||
        !ValidateReferenceSet(mission.successLive) ||
        !ValidateReachedSet(mission.successReached) ||
        !ValidateReferenceSet(mission.failureKill) ||
        !ValidateReferenceSet(mission.failureLive) ||
        !ValidateReachedSet(mission.failureReached))
      return false;
    if (mission.hasSummary) {
      if (mission.summaryName.size() > 79 ||
          mission.summaryText.size() > MAX_TEXT_LEN - 1 ||
          !Finite(mission.widthStart) || !Finite(mission.widthEnd) ||
          !ValidateReference(mission.route) ||
          (mission.route.kind != kReferenceSymbolic &&
           mission.route.kind != kReferenceTombstone) ||
          (mission.route.kind == kReferenceTombstone &&
           mission.routeGeometryFingerprint != 0))
        return false;
    } else if (!mission.summaryName.empty() || !mission.summaryText.empty() ||
               mission.widthStart != 0.0 || mission.widthEnd != 0.0 ||
               mission.color != 0 ||
               mission.routeGeometryFingerprint != 0 ||
               mission.route.kind != kReferenceTombstone ||
               !mission.route.name.empty())
      return false;
  }
  return true;
}

bool DecodeState(const std::vector<unsigned char> &bytes,
                 StableState *state, std::uint32_t *decodedVersion = NULL) {
  if (state == NULL)
    return false;
  Reader reader(bytes);
  std::uint32_t magic = 0, version = 0, count = 0;
  if (!reader.U32(&magic) || !reader.U32(&version) ||
      magic != kMissionMagic ||
      (version < kMissionLegacyVersion || version > kMissionVersion) ||
      !reader.String(&state->vehicle, kMaximumSymbolic) ||
      !reader.I32(&state->totalMissionCount) || !reader.U32(&count) ||
      count > kMaximumMissions)
    return false;
  state->missions.resize(count);
  for (std::size_t index = 0; index < state->missions.size(); ++index) {
    StableMission &mission = state->missions[index];
    if (!reader.I32(&mission.successFirst) || !reader.I32(&mission.status) ||
        (version >= kMissionArtefactVersion &&
         !reader.I32(&mission.giveArtefact)) ||
        !DecodeReference(&reader, &mission.project) ||
        !DecodeReference(&reader, &mission.commander) ||
        !reader.I32(&mission.hasSummary))
      return false;
    if (mission.hasSummary) {
      if (!reader.String(&mission.summaryName, 79) ||
          !reader.String(&mission.summaryText, MAX_TEXT_LEN - 1) ||
          !reader.Double(&mission.widthStart) ||
          !reader.Double(&mission.widthEnd) || !reader.U32(&mission.color) ||
          !DecodeReference(&reader, &mission.route))
        return false;
      if (version >= kMissionRouteGeometryVersion) {
        std::uint32_t low = 0, high = 0;
        if (!reader.U32(&low) || !reader.U32(&high))
          return false;
        mission.routeGeometryFingerprint =
            static_cast<unsigned long long>(low) |
            (static_cast<unsigned long long>(high) << 32);
      }
    }
    if (!DecodeReferenceSet(&reader, &mission.successKill) ||
        !DecodeReferenceSet(&reader, &mission.successLive) ||
        !DecodeReachedSet(&reader, &mission.successReached) ||
        !DecodeReferenceSet(&reader, &mission.failureKill) ||
        !DecodeReferenceSet(&reader, &mission.failureLive) ||
        !DecodeReachedSet(&reader, &mission.failureReached))
      return false;
  }
  if (version >= kMissionProjectRosterVersion) {
    if (!reader.U32(&count) || count > kMaximumProjects)
      return false;
    state->projectRosterPresent = true;
    state->projects.resize(count);
    for (std::size_t index = 0; index < state->projects.size(); ++index) {
      StableProject &project = state->projects[index];
      if (!reader.String(&project.name, kMaximumSymbolic) ||
          !reader.I32(&project.treeNode) ||
          !reader.I32(&project.permanent))
        return false;
    }
  }
  if (reader.offset != bytes.size() || !ValidateState(*state))
    return false;
  if (version >= kMissionRouteGeometryVersion) {
    for (std::size_t index = 0; index < state->missions.size(); ++index) {
      const StableMission &mission = state->missions[index];
      if ((mission.route.kind == kReferenceSymbolic) !=
          (mission.routeGeometryFingerprint != 0))
        return false;
    }
  }
  if (decodedVersion != NULL)
    *decodedVersion = version;
  return true;
}

bool ResolveReferenceSet(SimulationContext *context,
                         const std::vector<StableReference> &saved,
                         std::vector<KR_ObjectID> *resolved) {
  if (resolved == NULL)
    return false;
  resolved->resize(saved.size());
  for (std::size_t index = 0; index < saved.size(); ++index)
    if (!ResolveReference(context, saved[index], &(*resolved)[index]))
      return false;
  return true;
}

bool ResolveReachedSet(SimulationContext *context,
                       const std::vector<StableReached> &saved,
                       std::vector<KR_ObjectID> *resolved) {
  if (resolved == NULL)
    return false;
  resolved->resize(saved.size());
  for (std::size_t index = 0; index < saved.size(); ++index)
    if (!ResolveReference(context, saved[index].object,
                          &(*resolved)[index]))
      return false;
  return true;
}

bool ResolveMission(SimulationContext *context, const StableMission &saved,
                    ResolvedMission *resolved) {
  if (resolved == NULL ||
      !ResolveReference(context, saved.project, &resolved->project) ||
      !ResolveReference(context, saved.commander, &resolved->commander) ||
      !ResolveReference(context, saved.route, &resolved->route) ||
      !ResolveReferenceSet(context, saved.successKill,
                           &resolved->successKill) ||
      !ResolveReferenceSet(context, saved.successLive,
                           &resolved->successLive) ||
      !ResolveReachedSet(context, saved.successReached,
                         &resolved->successReached) ||
      !ResolveReferenceSet(context, saved.failureKill,
                           &resolved->failureKill) ||
      !ResolveReferenceSet(context, saved.failureLive,
                           &resolved->failureLive) ||
      !ResolveReachedSet(context, saved.failureReached,
                         &resolved->failureReached))
    return false;
  return !saved.hasSummary || IsNul(resolved->route) ||
         context->queryInterface(resolved->route, IRouteObjectIID) != NULL;
}

template <std::size_t N>
void CopyFixed(char (&destination)[N], const std::string &source) {
  const std::size_t size = source.size() < N - 1 ? source.size() : N - 1;
  std::memcpy(destination, source.data(), size);
  destination[size] = 0;
}

bool AddReferenceSet(KR_SetOfID *destination,
                     const std::vector<KR_ObjectID> &objects) {
  if (destination == NULL)
    return false;
  destination->clr();
  for (std::size_t index = 0; index < objects.size(); ++index)
    if (!destination->add(objects[index]))
      return false;
  return true;
}

bool ApplyState(SimulationContext *context, const StableState &state) {
  if (state.projectRosterPresent) {
    std::vector<StableProject> current;
    if (!CaptureProjectRoster(context, &current) ||
        current.size() != state.projects.size())
      return Fail("MSH1 ProjectTable roster is incompatible");
    for (std::size_t index = 0; index < state.projects.size(); ++index) {
      if (current[index].name != state.projects[index].name ||
          !context->isExist(state.projects[index].name.c_str()))
        return Fail("MSH1 ProjectTable symbolic roster is incompatible");
      const KR_ObjectID object =
          context->searchObject(state.projects[index].name.c_str());
      mp_Project *project = projectTable.searchProject(object);
      if (project == NULL)
        return Fail("MSH1 ProjectTable object is unresolved");
      project->m_treeNode = state.projects[index].treeNode;
      project->m_permanent = state.projects[index].permanent;
    }
  }
  if (state.vehicle.empty())
    return ResolveVehicle(context) == NULL;
  Vehicle *vehicle = ResolveVehicle(context, state.vehicle);
  if (vehicle == NULL || vehicle != g_vehicle)
    return Fail("MSH1 target Vehicle is unresolved");
  std::vector<ResolvedMission> resolved(state.missions.size());
  for (std::size_t index = 0; index < state.missions.size(); ++index)
    if (!ResolveMission(context, state.missions[index], &resolved[index]))
      return Fail("MSH1 symbolic mission dependency is unresolved");

  Player &player = static_cast<Player &>(vehicle->player());
  player.m_missCnt = 0;
  player.m_total_misCount = state.totalMissionCount;
  for (std::size_t index = 0; index < state.missions.size(); ++index) {
    const StableMission &saved = state.missions[index];
    const ResolvedMission &links = resolved[index];
    PlayerMission &mission = player.m_mission[index];
    mission.startInitialize(links.project, links.commander);
    mission.success_filed = saved.successFirst;
    mission.m_status = static_cast<MISSION_STATUS>(saved.status);
    mission.m_giveArtefact = saved.giveArtefact;
    mission.m_TMissionId = -1;
    mission.m_missionInfoExist = saved.hasSummary;
    mission.m_missionName[0] = 0;
    mission.m_missionText[0] = 0;
    mission.m_missionsw = 0.0;
    mission.m_missionew = 0.0;
    mission.m_missionrgb = 0;
    mission.m_missionRouteID = KR_ObjectID::NUL();
    if (saved.hasSummary) {
      CopyFixed(mission.m_missionName, saved.summaryName);
      CopyFixed(mission.m_missionText, saved.summaryText);
      mission.m_missionsw = saved.widthStart;
      mission.m_missionew = saved.widthEnd;
      mission.m_missionrgb = static_cast<int>(saved.color);
      mission.m_missionRouteID = links.route;
    }
    if (!AddReferenceSet(&mission.success_needKill, links.successKill) ||
        !AddReferenceSet(&mission.success_needLive, links.successLive) ||
        !AddReferenceSet(&mission.success_needReached,
                         links.successReached) ||
        !AddReferenceSet(&mission.filed_needKill, links.failureKill) ||
        !AddReferenceSet(&mission.filed_needLive, links.failureLive) ||
        !AddReferenceSet(&mission.filed_needReached,
                         links.failureReached))
      return Fail("MSH1 condition set overflowed during apply");
    for (std::size_t reached = 0; reached < saved.successReached.size();
         ++reached) {
      mission.success_reachedPos[reached] =
          saved.successReached[reached].position;
      mission.success_reachedRadius[reached] =
          saved.successReached[reached].radius;
    }
    for (std::size_t reached = 0; reached < saved.failureReached.size();
         ++reached) {
      mission.filed_reachedPos[reached] =
          saved.failureReached[reached].position;
      mission.filed_reachedRadius[reached] =
          saved.failureReached[reached].radius;
    }
    ++player.m_missCnt;
  }
  player.loadNotify();
  return true;
}

struct FirstObject {
  SimulationContext *context;
  KR_ObjectID object;
  bool requireRoute;

  FirstObject(SimulationContext *value, bool route)
      : context(value), object(KR_ObjectID::NUL()), requireRoute(route) {}
};

bool CollectFirstObject(KR_ObjectID object, void *user) {
  FirstObject *first = static_cast<FirstObject *>(user);
  if (first == NULL || first->context == NULL ||
      ObjectName(first->context, object).empty())
    return true;
  if (first->requireRoute) {
    IRouteObject *route = static_cast<IRouteObject *>(
        first->context->queryInterface(object, IRouteObjectIID));
    if (route == NULL || route->GetNodeCnt() <= 0)
      return true;
  }
  first->object = object;
  return false;
}

KR_ObjectID FirstSeanceObject(SimulationContext *context,
                              const char *tableName, bool requireRoute) {
  const ct_ClassTableID table = g_arena.searchSeanceClassTable(tableName);
  if (table == ct_NULLID)
    return KR_ObjectID::NUL();
  FirstObject first(context, requireRoute);
  g_arena.userFind(table, CollectFirstObject, &first);
  return first.object;
}

KR_ObjectID FirstProject(SimulationContext *context) {
  FirstObject first(context, false);
  projectTable.userFind(CollectFirstObject, &first);
  return first.object;
}

void RemoveCreated(SimulationContext *context,
                   std::vector<KR_ObjectID> *created) {
  if (context == NULL || created == NULL)
    return;
  for (std::vector<KR_ObjectID>::reverse_iterator object = created->rbegin();
       object != created->rend(); ++object)
    if (context->isExist(*object))
      context->removeObject(*object);
  created->clear();
}

int ConditionReferenceCount(const StableState &state) {
  int count = 0;
  for (std::size_t index = 0; index < state.missions.size(); ++index) {
    const StableMission &mission = state.missions[index];
    count += static_cast<int>(mission.successKill.size() +
                              mission.successLive.size() +
                              mission.successReached.size() +
                              mission.failureKill.size() +
                              mission.failureLive.size() +
                              mission.failureReached.size());
  }
  return count;
}

}  // namespace

bool MissionActiveWorldState_CaptureStable(
    SimulationContext *context, std::vector<unsigned char> *bytes) {
  StableState state;
  return CaptureState(context, &state) && ValidateState(state) &&
         EncodeState(state, bytes);
}

bool MissionActiveWorldState_ValidateStable(
    const std::vector<unsigned char> &bytes) {
  StableState state;
  return DecodeState(bytes, &state);
}

bool MissionActiveWorldState_ProbeLegacyVersionCompatibility(
    SimulationContext *context) {
  StableState state;
  std::vector<unsigned char> version1;
  std::vector<unsigned char> version2;
  std::vector<unsigned char> version3;
  return CaptureState(context, &state) && ValidateState(state) &&
         EncodeStateVersion(state, kMissionLegacyVersion, &version1) &&
         EncodeStateVersion(state, kMissionRouteGeometryVersion, &version2) &&
         EncodeStateVersion(state, kMissionArtefactVersion, &version3) &&
         MissionActiveWorldState_ValidateStable(version1) &&
         MissionActiveWorldState_ValidateStable(version2) &&
         MissionActiveWorldState_ValidateStable(version3) &&
         MissionActiveWorldState_MatchesStable(context, version1) &&
         MissionActiveWorldState_MatchesStable(context, version2) &&
         MissionActiveWorldState_MatchesStable(context, version3);
}

bool MissionActiveWorldState_RouteRequirements(
    const std::vector<unsigned char> &bytes,
    std::vector<SMissionRouteRequirement> *requirements) {
  StableState state;
  if (requirements == NULL || !DecodeState(bytes, &state))
    return false;
  requirements->clear();
  for (std::size_t index = 0; index < state.missions.size(); ++index) {
    const StableMission &mission = state.missions[index];
    if (!mission.hasSummary ||
        mission.route.kind != kReferenceSymbolic)
      continue;
    SMissionRouteRequirement requirement;
    requirement.name = mission.route.name;
    requirement.geometryFingerprint =
        mission.routeGeometryFingerprint;
    requirements->push_back(requirement);
  }
  std::sort(requirements->begin(), requirements->end(),
            [](const SMissionRouteRequirement &left,
               const SMissionRouteRequirement &right) {
              if (left.name != right.name)
                return left.name < right.name;
              return left.geometryFingerprint < right.geometryFingerprint;
            });
  requirements->erase(
      std::unique(requirements->begin(), requirements->end(),
                  [](const SMissionRouteRequirement &left,
                     const SMissionRouteRequirement &right) {
                    return left.name == right.name &&
                        left.geometryFingerprint ==
                            right.geometryFingerprint;
                  }),
      requirements->end());
  return true;
}

bool MissionActiveWorldState_MatchesStable(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  StableState expected;
  StableState current;
  std::uint32_t version = 0;
  std::vector<unsigned char> currentBytes;
  return DecodeState(bytes, &expected, &version) &&
         CaptureState(context, &current) && ValidateState(current) &&
         EncodeStateVersion(current, version, &currentBytes) &&
         currentBytes == bytes;
}

bool MissionActiveWorldState_CreateStableOwners(
    SimulationContext *context, const std::vector<unsigned char> &bytes,
    std::vector<KR_ObjectID> *created) {
  StableState state;
  if (context == NULL || created == NULL || !DecodeState(bytes, &state))
    return false;
  for (std::size_t index = 0; index < state.missions.size(); ++index) {
    const StableReference &route = state.missions[index].route;
    if (state.missions[index].hasSummary &&
        state.missions[index].route.kind == kReferenceSymbolic &&
        !context->isExist(route.name.c_str()))
      return Fail("MSH1 mission Route dependency is unresolved");
    if (state.missions[index].hasSummary &&
        state.missions[index].route.kind == kReferenceSymbolic) {
      const KR_ObjectID object = context->searchObject(route.name.c_str());
      if (context->queryInterface(object, IRouteObjectIID) == NULL ||
          (state.missions[index].routeGeometryFingerprint != 0 &&
           RouteGeometryFingerprint(context, object) !=
               state.missions[index].routeGeometryFingerprint))
        return Fail("MSH1 mission Route geometry is incompatible");
    }
  }
  if (!state.vehicle.empty() && ResolveVehicle(context, state.vehicle) == NULL) {
    RemoveCreated(context, created);
    return Fail("MSH1 Vehicle dependency is unresolved");
  }
  return true;
}

bool MissionActiveWorldState_ApplyStableReferences(
    SimulationContext *context, const std::vector<unsigned char> &bytes) {
  StableState state;
  return DecodeState(bytes, &state) && ApplyState(context, state) &&
         MissionActiveWorldState_MatchesStable(context, bytes);
}

void MissionActiveWorldState_RemoveStableOwners(
    SimulationContext *context, std::vector<KR_ObjectID> *created) {
  RemoveCreated(context, created);
}

bool MissionActiveWorldState_StageProbe(
    SimulationContext *context, double timeStamp, bool *staged) {
  if (context == NULL || staged == NULL)
    return false;
  *staged = false;
  const char *levelDirectory = RecoveredLevelRuntime_Directory();
  if (levelDirectory == NULL || levelDirectory[0] == '\0')
    return true;
  Vehicle *vehicle = ResolveVehicle(context);
  if (vehicle == NULL || vehicle != g_vehicle)
    return true;
  Player &player = static_cast<Player &>(vehicle->player());
  if (!MissionActiveWorldState_ClearProbe(context) ||
      player.m_missCnt < 0 || player.m_missCnt >= 6 ||
      context->eventFreeCount() < 1)
    return false;
  RecruitCenterMissionProbeSummary authored = {};
  bool authoredStaged = false;
  if (!RecruitCenterSubjectState_StageMissionProbe(
          context, timeStamp, &authoredStaged, &authored)) {
    return Fail(RecruitCenterSubjectState_LastError());
  }
  if (authoredStaged) {
    *staged = true;
    return true;
  }
  const KR_ObjectID project = FirstProject(context);
  const KR_ObjectID recruit =
      FirstSeanceObject(context, "RecruitCenter", false);
  const KR_ObjectID route = FirstSeanceObject(context, "Route", true);
  KR_ObjectID commander = KR_ObjectID::NUL();
  for (int index = 0; index < player.m_sideQnty && IsNul(commander); ++index)
    if (context->isExist(player.m_playerStatus[index].m_masterID))
      commander = player.m_playerStatus[index].m_masterID;
  // Hermetic source-only tests omit the retail RecruitCenter scripts. Keep a
  // durable symbolic destination there so MSH1 still exercises the exact
  // rc_CHECK_MISSION label/payload; installed Levels take the authored path
  // above and never reach this fallback.
  const KR_ObjectID destination = IsNul(recruit)
      ? (IsNul(commander) ? vehicle->getObjectID() : commander)
      : recruit;

  const int missionIndex = player.m_missCnt;
  PlayerMission &mission = player.m_mission[missionIndex];
  mission.startInitialize(project, commander);
  mission.success_filed = 1;
  mission.m_status = MISSION_INPROCESS;
  // Use an already published retail Route when one exists.  This keeps the
  // probe inside the original PlayerMission -> Player::loadNotify -> DebugMap
  // path and gives MSH1 a real symbolic dependency without inventing a route
  // owner or consuming the legacy fixed route-node arena.
  mission.m_missionInfoExist = 1;
  CopyFixed(mission.m_missionName, std::string(kProbeName));
  CopyFixed(mission.m_missionText,
            std::string("PlayerMission \xD2\xE5\xF1\xF2$save/load route proof"));
  mission.m_missionsw = 0.25;
  mission.m_missionew = 0.75;
  mission.m_missionrgb = 0x20a0f0;
  mission.m_missionRouteID = route;
  const KR_ObjectID vehicleID = vehicle->getObjectID();
  const CFVector3 position = vehicle->Pos();
  mission.success_needKill.add(KR_ObjectID::NUL());
  mission.success_needLive.add(vehicleID);
  mission.success_needReached.add(vehicleID);
  mission.success_reachedPos[0] = CFVector2(position.x, position.z);
  mission.success_reachedRadius[0] = 64.0;
  mission.filed_needKill.add(vehicleID);
  mission.filed_needLive.add(vehicleID);
  mission.filed_needReached.add(vehicleID);
  mission.filed_reachedPos[0] =
      CFVector2(position.x + 4096.0, position.z - 4096.0);
  mission.filed_reachedRadius[0] = 1.0;
  ++player.m_missCnt;
  ++player.m_total_misCount;
  player.loadNotify();

  KR_Event event;
  event.label = rc_CHECK_MISSION;
  event.source = vehicleID;
  event.destination = destination;
  event.timeStamp = Finite(timeStamp) && timeStamp >= 0.1 ? timeStamp : 0.1;
  event.data.open(EDO_WRITE).putInt(missionIndex).close();
  context->addEvent(event);
  *staged = true;
  return true;
}

bool MissionActiveWorldState_ClearProbe(SimulationContext *context) {
  if (context == NULL)
    return false;
  Vehicle *vehicle = ResolveVehicle(context);
  if (vehicle == NULL)
    return true;
  const KR_ObjectID vehicleID = vehicle->getObjectID();
  const int queuedCount =
      context->copyEvents(rc_CHECK_MISSION, vehicleID, NULL, 0);
  std::vector<KR_Event> queued(
      queuedCount > 0 ? static_cast<std::size_t>(queuedCount) : 0);
  if (queuedCount < 0 ||
      context->copyEvents(rc_CHECK_MISSION, vehicleID,
                          queued.empty() ? NULL : &queued[0], queuedCount) !=
          queuedCount)
    return false;
  std::vector<int> probeIndices;
  for (int index = 0; index < queuedCount; ++index) {
    s_EventData &data = queued[index].data.open(EDO_READ);
    int missionIndex = -1;
    if (data.remaining() == static_cast<int>(sizeof(int)))
      data.getInt(missionIndex);
    data.close();
    if (missionIndex >= 0 && missionIndex < 6)
      probeIndices.push_back(missionIndex);
  }
  while (context->removeEvent(rc_CHECK_MISSION, vehicleID) == 1) {}
  std::sort(probeIndices.begin(), probeIndices.end());
  probeIndices.erase(std::unique(probeIndices.begin(), probeIndices.end()),
                     probeIndices.end());
  Player &player = static_cast<Player &>(vehicle->player());
  bool removed = false;
  for (std::vector<int>::reverse_iterator probe = probeIndices.rbegin();
       probe != probeIndices.rend(); ++probe) {
    const int index = *probe;
    if (index < 0 || index >= player.m_missCnt)
      continue;
    for (int move = index; move + 1 < player.m_missCnt; ++move)
      player.m_mission[move] = player.m_mission[move + 1];
    --player.m_missCnt;
    if (player.m_total_misCount > 0)
      --player.m_total_misCount;
    removed = true;
  }
  for (int index = 0; index < player.m_missCnt;) {
    std::string name;
    if (!FixedString(player.m_mission[index].m_missionName, &name))
      return false;
    if (name != kProbeName) {
      ++index;
      continue;
    }
    for (int move = index; move + 1 < player.m_missCnt; ++move)
      player.m_mission[move] = player.m_mission[move + 1];
    --player.m_missCnt;
    if (player.m_total_misCount > 0)
      --player.m_total_misCount;
    removed = true;
  }
  if (removed)
    player.loadNotify();
  return true;
}

bool MissionActiveWorldState_MissionIndexValid(
    SimulationContext *context, int index) {
  Vehicle *vehicle = ResolveVehicle(context);
  if (vehicle == NULL || vehicle != g_vehicle)
    return false;
  const Player &player = static_cast<const Player &>(vehicle->player());
  return index >= 0 && index < player.m_missCnt;
}

bool MissionActiveWorldState_ProbeCounts(
    const std::vector<unsigned char> &bytes, int *missions,
    int *conditionReferences, int *routeReferences) {
  StableState state;
  if (missions == NULL || conditionReferences == NULL ||
      routeReferences == NULL || !DecodeState(bytes, &state))
    return false;
  *missions = static_cast<int>(state.missions.size());
  *conditionReferences = ConditionReferenceCount(state);
  *routeReferences = 0;
  for (std::size_t index = 0; index < state.missions.size(); ++index)
    if (state.missions[index].hasSummary &&
        state.missions[index].route.kind == kReferenceSymbolic)
      ++*routeReferences;
  return true;
}

const char *MissionActiveWorldState_LastFailure() {
  return g_lastFailure.c_str();
}
