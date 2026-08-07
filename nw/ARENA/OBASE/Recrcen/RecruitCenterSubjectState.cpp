#include "RecruitCenterSubjectState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <vector>

class CGRPanel;
#define LAST_H__SCENE
#include "game.h"
#include "h/vehicle.h"
#include "h/olevel.h"
#include "hardware.h"
#include "briefing.h"
#include "dmap.h"
#include "i/dynobj.i"
#include "i/player.i"
#include "i/route.i"
#include "i/unit.i"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/artfmsg.h"
#include "message/hardmsg.h"
#include "message/recrcenmsg.h"
#include "message/unitmsg.h"
#include "message/vehiclemsg.h"
#include "mproj/h/mproj.h"
#include "storage/h/subject.h"
#include "i/carrier.i"
#include "obase/artefact/ArtefactActiveWorldState.h"
#include "obase/artefact/ArtefactAttributeState.h"
#include "RecoveredLegacyScriptHost.h"
#include "RecoveredLegacyScriptRunner.h"
#include "RecoveredModRuntime.h"
#include "../output/defs.h"

namespace {

const int kMaximumCapacity = 16;
const int kMaximumProjectNodes = 1024;
const int kMaximumProjectPayload = 10240;
const int kSetGiveArtefactCommand = 35;
const double kRadius = 4.0;
const double kCollisionDebounceSeconds = 0.25;
const long kMaximumMissionScriptBytes = 1024L * 1024L;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

char g_lastError[256] = {};
RecruitCenterMissionProbeSummary g_lastMissionSummary = {};
bool g_hasLastMissionSummary = false;
int g_missionStatusPresentations = 0;
int g_missionResultPresentations = 0;

struct DeferredMissionCommand
{
    int command;
    std::string first;
    std::string second;
    std::string third;
    std::string fourth;
    double values[6];
    int integer;

    DeferredMissionCommand() : command(0), integer(0)
    {
        for (int index = 0; index < 6; ++index) values[index] = 0.0;
    }
};

struct PreparedMissionFile
{
    std::string authoredPath;
    std::string resolvedPath;
    std::string source;
};

struct CenterEncounterPresentation
{
    int attempts;
    int centerFlicks;
    int hostilityBriefings;
    int failures;

    CenterEncounterPresentation()
        : attempts(0), centerFlicks(0), hostilityBriefings(0), failures(0) {}
};

void SetError(const char *message)
{
    std::snprintf(g_lastError, sizeof(g_lastError), "%s",
                  message == NULL ? "unknown RecruitCenter failure"
                                  : message);
}

bool IsSafeMissionPath(const std::string &path)
{
    if (path.empty() || path.size() >= 260 || path[0] == '/' ||
        path[0] == '\\' || path.find(':') != std::string::npos)
        return false;
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (normalized == ".." || normalized.find("../") == 0 ||
        normalized.find("/../") != std::string::npos)
        return false;
    return normalized.size() < 3 ||
           normalized.substr(normalized.size() - 3) != "/..";
}

bool PrepareMissionFile(const std::string &path, bool readSource,
                        PreparedMissionFile *prepared)
{
    if (prepared == NULL || !IsSafeMissionPath(path)) return false;
    char resolved[32768] = {};
    if (!RecoveredModRuntime_ResolveReadPath(path.c_str(), resolved,
                                             sizeof(resolved)))
        return false;
    long length = 0;
    FILE *file = RecoveredModRuntime_OpenRead(path.c_str(), &length);
    if (file == NULL || length < 0 || length > kMaximumMissionScriptBytes)
    {
        if (file != NULL) std::fclose(file);
        return false;
    }
    std::string source;
    if (readSource)
    {
        try
        {
            source.resize(static_cast<std::size_t>(length));
        }
        catch (...)
        {
            std::fclose(file);
            return false;
        }
        if (length > 0 &&
            std::fread(&source[0], 1, static_cast<std::size_t>(length), file) !=
                static_cast<std::size_t>(length))
        {
            std::fclose(file);
            return false;
        }
        if (source.empty() || source.find('\0') != std::string::npos)
        {
            std::fclose(file);
            return false;
        }
    }
    std::fclose(file);
    prepared->authoredPath = path;
    prepared->resolvedPath = resolved;
    prepared->source.swap(source);
    return true;
}

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

void HashBytes(unsigned long long &hash, const void *bytes,
               unsigned int count)
{
    const unsigned char *data = static_cast<const unsigned char *>(bytes);
    for (unsigned int index = 0; index < count; ++index)
    {
        hash ^= data[index];
        hash *= kHashPrime;
    }
}

void HashString(unsigned long long &hash, const char *value)
{
    HashBytes(hash, value, static_cast<unsigned int>(std::strlen(value) + 1));
}

bool CopyString(char *destination, int capacity, const std::string &source)
{
    if (destination == NULL || capacity <= 0 ||
        source.size() >= static_cast<std::size_t>(capacity))
        return false;
    std::memcpy(destination, source.c_str(), source.size() + 1);
    return true;
}

class ProjectDataReader
{
 public:
    explicit ProjectDataReader(mp_NodeNum node)
        : m_node(node), m_open(true), m_valid(true)
    {
        mp_OpenData(projectTable, m_node, EDO_READ);
    }

    ~ProjectDataReader()
    {
        if (m_open) mp_CloseData(projectTable, m_node);
    }

    bool readInt(int *value)
    {
        if (!m_valid || value == NULL) return false;
        mp_ReadInt(projectTable, m_node, *value);
        return true;
    }

    bool readDouble(double *value)
    {
        if (!m_valid || value == NULL) return false;
        mp_ReadFloat(projectTable, m_node, *value);
        m_valid = std::isfinite(*value);
        return m_valid;
    }

    bool readString(std::string *value, std::size_t maximum)
    {
        if (!m_valid || value == NULL || maximum == 0 ||
            maximum > static_cast<std::size_t>(kMaximumProjectPayload))
            return false;
        // ProjectTable admission has already constrained this compiler-written
        // heap to 10,240 bytes and proved terminated EDI_STR payloads. The
        // legacy reader has no capacity argument, so keep its destination one
        // byte larger than that complete admitted heap before applying the
        // field-specific limit below.
        char buffer[kMaximumProjectPayload + 1] = {};
        mp_ReadStr(projectTable, m_node, buffer);
        const std::size_t length = std::strlen(buffer);
        if (length >= maximum)
        {
            m_valid = false;
            return false;
        }
        value->assign(buffer, length);
        return true;
    }

    bool finish()
    {
        if (m_open)
        {
            mp_CloseData(projectTable, m_node);
            m_open = false;
        }
        return m_valid;
    }

 private:
    mp_NodeNum m_node;
    bool m_open;
    bool m_valid;
};

void InitializeMission(PlayerMission *mission, KR_ObjectID project,
                       KR_ObjectID commander)
{
    mission->startInitialize(project, commander);
    mission->m_TMissionId = -1;
    mission->m_missionInfoExist = 0;
    mission->m_missionText[0] = 0;
    mission->m_missionName[0] = 0;
    mission->m_missionsw = 0.0;
    mission->m_missionew = 1.0;
    mission->m_missionrgb = 0xffffff;
    mission->m_missionRouteID = KR_ObjectID::NUL();
    for (int index = 0; index < 10; ++index)
    {
        mission->success_reachedPos[index] = CFVector2(0.0, 0.0);
        mission->success_reachedRadius[index] = 0.0;
        mission->filed_reachedPos[index] = CFVector2(0.0, 0.0);
        mission->filed_reachedRadius[index] = 0.0;
    }
    mission->m_status = MISSION_INPROCESS;
}

int ConditionCount(const PlayerMission &mission)
{
    return mission.success_needKill.getCount() +
           mission.success_needLive.getCount() +
           mission.success_needReached.getCount() +
           mission.filed_needKill.getCount() +
           mission.filed_needLive.getCount() +
           mission.filed_needReached.getCount();
}

bool HasMission(const Player &player, KR_ObjectID project)
{
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].mID == project) return true;
    return false;
}

bool ProjectMetadata(KR_ObjectID project, const char *commander,
                     int totalMissionCount, bool *matches,
                     bool *eligible, int *requiredMissionCount)
{
    *matches = true;
    *eligible = true;
    *requiredMissionCount = 0;
    mp_Project *projectState = projectTable.searchProject(project);
    if (projectState == NULL) return false;
    if (projectState->m_treeNode < 0)
    {
        // Completed non-permanent projects stay as serialized tombstones so
        // a pre-result LCN1 checkpoint can restore their authored tree node.
        *matches = false;
        *eligible = false;
        return true;
    }
    std::set<int> visited;
    for (mp_NodeNum node = projectTable.getProjectRoot(project);
         node != mp_NodeNULL(); node = projectTable.getRight(node))
    {
        const int decoded = mp_Code2Int(node);
        if (decoded < 0 || decoded >= kMaximumProjectNodes ||
            !visited.insert(decoded).second)
            return false;
        const int command = projectTable.getCommand(node);
        if (command == COM_0COMMANDER)
        {
            std::string value;
            ProjectDataReader data(node);
            if (!data.readString(&value, 50) || !data.finish()) return false;
            *matches = value == commander;
        }
        else if (command == COM_MISSIONINFO)
        {
            int required = 0;
            ProjectDataReader data(node);
            if (!data.readInt(&required) || !data.finish() || required < 0)
                return false;
            if (required > *requiredMissionCount)
                *requiredMissionCount = required;
            if (required > totalMissionCount) *eligible = false;
        }
    }
    return true;
}

struct CandidateSearch
{
    Player *player;
    const char *commander;
    KR_ObjectID result;
    int bestRequiredMissionCount;
    bool valid;
};

bool FindCandidate(KR_ObjectID object, void *parameter)
{
    CandidateSearch *search = static_cast<CandidateSearch *>(parameter);
    if (search == NULL || search->player == NULL ||
        search->commander == NULL)
        return false;
    bool matches = false;
    bool eligible = false;
    int requiredMissionCount = 0;
    if (!ProjectMetadata(object, search->commander,
                         search->player->m_total_misCount,
                         &matches, &eligible, &requiredMissionCount))
    {
        search->valid = false;
        return false;
    }
    if (matches && eligible && !HasMission(*search->player, object) &&
        requiredMissionCount > search->bestRequiredMissionCount)
    {
        search->result = object;
        search->bestRequiredMissionCount = requiredMissionCount;
    }
    return true;
}

bool ReadReached(ProjectDataReader *data, std::string *objectName,
                 CFVector2 *position, double *radius)
{
    return data->readString(objectName, 80) &&
           data->readDouble(&position->x) &&
           data->readDouble(&position->y) && data->readDouble(radius) &&
           *radius >= 0.0 && *radius <= 1000000.0;
}

bool AddReached(KR_SetOfID *objects, CFVector2 *positions,
                double *radii, SimulationContext *context,
                const std::string &objectName,
                const CFVector2 &position, double radius)
{
    if (objects->getCount() >= KR_SetOfID::MAX_ID_CNT) return false;
    const KR_ObjectID object = context->isExist(objectName.c_str())
        ? context->searchObject(objectName.c_str()) : KR_ObjectID::NUL();
    if (!objects->add(object)) return false;
    const int index = objects->getCount() - 1;
    positions[index] = position;
    radii[index] = radius;
    return true;
}

bool AddNamedCondition(KR_SetOfID *objects, SimulationContext *context,
                       const std::string &objectName)
{
    if (objects->getCount() >= KR_SetOfID::MAX_ID_CNT) return false;
    return objects->add(context->isExist(objectName.c_str())
        ? context->searchObject(objectName.c_str()) : KR_ObjectID::NUL());
}

bool ReadDeferredCommand(int command, ProjectDataReader *data,
                         DeferredMissionCommand *deferred)
{
    if (data == NULL || deferred == NULL) return false;
    deferred->command = command;
    switch (command)
    {
    case COM_CREATE_UNITS:
        return data->readString(&deferred->first, 40) &&
               data->readString(&deferred->second, 80) &&
               data->readDouble(&deferred->values[0]) &&
               deferred->values[0] >= 0.0 &&
               data->readString(&deferred->third, 40) &&
               data->readString(&deferred->fourth, 40);
    case COM_PLAY_BRIEFING:
    case COM_RUN_SCRIPT:
        return data->readString(&deferred->first, 260);
    case COM_SKIP_WAY:
        return data->readDouble(&deferred->values[0]) &&
               data->readDouble(&deferred->values[1]) &&
               data->readDouble(&deferred->values[2]) &&
               data->readDouble(&deferred->values[3]);
    case COM_PLAY_BRIEFING_MSG:
        return data->readString(&deferred->first, 260) &&
               data->readInt(&deferred->integer);
    case 33:
        if (!data->readString(&deferred->first, 80)) return false;
        for (int index = 0; index < 5; ++index)
            if (!data->readDouble(&deferred->values[index])) return false;
        return data->readString(&deferred->second, 260);
    case 34:
        return data->readDouble(&deferred->values[0]) &&
               data->readDouble(&deferred->values[1]) &&
               data->readDouble(&deferred->values[2]) &&
               data->readInt(&deferred->integer) &&
               data->readDouble(&deferred->values[3]) &&
               data->readString(&deferred->first, 260);
    case 35:
        return true;
    default:
        return false;
    }
}

bool DecodeMission(SimulationContext *context, KR_ObjectID project,
                   KR_ObjectID commander, PlayerMission *mission,
                   std::string *routeName,
                   std::vector<DeferredMissionCommand> *deferredCommands)
{
    if (deferredCommands == NULL) return false;
    InitializeMission(mission, project, commander);
    routeName->clear();
    deferredCommands->clear();
    std::set<int> visited;
    for (mp_NodeNum node = projectTable.getProjectRoot(project);
         node != mp_NodeNULL(); node = projectTable.getRight(node))
    {
        const int decoded = mp_Code2Int(node);
        if (decoded < 0 || decoded >= kMaximumProjectNodes ||
            !visited.insert(decoded).second)
            return false;
        const int command = projectTable.getCommand(node);
        std::string objectName;
        switch (command)
        {
        case COM_0COMMANDER:
        case COM_MISSIONINFO:
            break;
        case COM_SUCCESS_KILL:
        case COM_SUCCESS_LIVE:
        case COM_FILED_KILL:
        case COM_FILED_LIVE:
        {
            ProjectDataReader data(node);
            if (!data.readString(&objectName, 80) || !data.finish())
                return false;
            KR_SetOfID *set = command == COM_SUCCESS_KILL
                ? &mission->success_needKill
                : command == COM_SUCCESS_LIVE
                    ? &mission->success_needLive
                    : command == COM_FILED_KILL
                        ? &mission->filed_needKill
                        : &mission->filed_needLive;
            if (!AddNamedCondition(set, context, objectName)) return false;
            break;
        }
        case COM_SUCCESS_REACHED:
        case COM_FILED_REACHED:
        {
            CFVector2 position;
            double radius = 0.0;
            ProjectDataReader data(node);
            if (!ReadReached(&data, &objectName, &position, &radius) ||
                !data.finish())
                return false;
            if (command == COM_SUCCESS_REACHED)
            {
                if (!AddReached(&mission->success_needReached,
                                mission->success_reachedPos,
                                mission->success_reachedRadius, context,
                                objectName, position, radius)) return false;
            }
            else if (!AddReached(&mission->filed_needReached,
                                 mission->filed_reachedPos,
                                 mission->filed_reachedRadius, context,
                                 objectName, position, radius)) return false;
            break;
        }
        case COM_SUCCESS_FILED:
            mission->success_filed = 1;
            break;
        case COM_FILED_SUCCESS:
            mission->success_filed = 0;
            break;
        case COM_SET_MISSION_SUMMARY:
        {
            std::string name;
            std::string text;
            int rgb = 0;
            double start = 0.0;
            double end = 0.0;
            ProjectDataReader data(node);
            if (!data.readString(&name, sizeof(mission->m_missionName)) ||
                !data.readString(&text, sizeof(mission->m_missionText)) ||
                !data.readString(routeName, 260) || !data.readInt(&rgb) ||
                !data.readDouble(&start) || !data.readDouble(&end) ||
                !data.finish() || start < 0.0 || start > 1000.0 ||
                end < 0.0 || end > 1000.0 ||
                !CopyString(mission->m_missionName,
                            sizeof(mission->m_missionName), name) ||
                !CopyString(mission->m_missionText,
                            sizeof(mission->m_missionText), text))
                return false;
            mission->m_missionrgb = rgb;
            mission->m_missionsw = start;
            mission->m_missionew = end;
            mission->m_missionInfoExist = 1;
            break;
        }
        case COM_CREATE_UNITS:
        case COM_PLAY_BRIEFING:
        case COM_RUN_SCRIPT:
        case COM_SKIP_WAY:
        case COM_PLAY_BRIEFING_MSG:
        case 33:
        case 34:
        case 35:
        {
            ProjectDataReader data(node);
            DeferredMissionCommand deferred;
            if (!ReadDeferredCommand(command, &data, &deferred) ||
                !data.finish())
                return false;
            deferredCommands->push_back(deferred);
            // Only the authored March/May COM_SET_GIVEARTEFACT command marks
            // a RecruitCenter result as an Artifact reward. Briefing and
            // script commands are common to ordinary no-reward missions.
            if (command == kSetGiveArtefactCommand)
                mission->m_giveArtefact = 1;
            break;
        }
        case COM_BRIEFING_OVER:
        {
            DeferredMissionCommand deferred;
            deferred.command = command;
            deferredCommands->push_back(deferred);
            break;
        }
        default:
            return false;
        }
    }
    return mission->m_missionInfoExist != 0;
}

bool EvaluateConditions(PlayerMission *mission, SimulationContext *context,
                         bool successSet)
{
    KR_SetOfID &kill = successSet ? mission->success_needKill
                                  : mission->filed_needKill;
    KR_SetOfID &live = successSet ? mission->success_needLive
                                  : mission->filed_needLive;
    KR_SetOfID &reached = successSet ? mission->success_needReached
                                     : mission->filed_needReached;
    CFVector2 *positions = successSet ? mission->success_reachedPos
                                      : mission->filed_reachedPos;
    double *radii = successSet ? mission->success_reachedRadius
                               : mission->filed_reachedRadius;
    bool defined = false;
    for (int index = 0; index < kill.getCount(); ++index)
    {
        defined = true;
        if (context->isExist(kill[index])) return false;
    }
    for (int index = 0; index < live.getCount(); ++index)
    {
        defined = true;
        if (!context->isExist(live[index])) return false;
    }
    for (int index = 0; index < reached.getCount(); ++index)
    {
        IDynamicObject *object = static_cast<IDynamicObject *>(
            context->queryInterface(reached[index], IDynamicObjectIID));
        if (object == NULL) continue;
        const CFVector3 position = object->getPos();
        const double dx = position.x - positions[index].x;
        const double dz = position.z - positions[index].y;
        if (dx * dx + dz * dz <= radii[index] * radii[index]) return true;
    }
    return defined;
}

bool MissionReferencesBound(const PlayerMission &mission)
{
    const KR_SetOfID *sets[] = {
        &mission.success_needKill, &mission.success_needLive,
        &mission.success_needReached, &mission.filed_needKill,
        &mission.filed_needLive, &mission.filed_needReached};
    for (int setIndex = 0; setIndex < 6; ++setIndex)
        for (int index = 0; index < sets[setIndex]->getCount(); ++index)
        {
            KR_ObjectID object = (*sets[setIndex])[index];
            if (object.isNUL()) return false;
        }
    return true;
}

int DeferredCommandCount(const std::vector<DeferredMissionCommand> &commands,
                         int command)
{
    int count = 0;
    for (std::size_t index = 0; index < commands.size(); ++index)
        if (commands[index].command == command) ++count;
    return count;
}

bool PrepareDeferredMissionFiles(
    const std::vector<DeferredMissionCommand> &commands,
    std::vector<PreparedMissionFile> *files)
{
    if (files == NULL)
    {
        SetError("RecruitCenter mission preflight has no output storage");
        return false;
    }
    files->clear();
    try
    {
        files->resize(commands.size());
    }
    catch (...)
    {
        SetError("RecruitCenter mission preflight allocation failed");
        return false;
    }
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        const int command = commands[index].command;
        if (command == COM_RUN_SCRIPT)
        {
            if (!PrepareMissionFile(commands[index].first, true,
                                    &(*files)[index]))
            {
                char message[256] = {};
                std::snprintf(message, sizeof(message),
                              "RecruitCenter cannot preflight script %.160s",
                              commands[index].first.c_str());
                SetError(message);
                return false;
            }
        }
        else if (command == COM_PLAY_BRIEFING ||
                 command == COM_PLAY_BRIEFING_MSG)
        {
            if (!PrepareMissionFile(commands[index].first, false,
                                    &(*files)[index]))
            {
                char message[256] = {};
                std::snprintf(message, sizeof(message),
                              "RecruitCenter cannot preflight briefing %.158s",
                              commands[index].first.c_str());
                SetError(message);
                return false;
            }
        }
        else if (command != COM_BRIEFING_OVER &&
                 command != kSetGiveArtefactCommand)
        {
            // CREATE_UNITS, SKIP_WAY and the May checkpoint command retain
            // their decoded payloads, but they do not yet have a
            // transactional modern owner. Never silently accept them.
            char message[256] = {};
            std::snprintf(message, sizeof(message),
                          "RecruitCenter deferred command %d is unsupported",
                          command);
            SetError(message);
            return false;
        }
        // COM_SET_GIVEARTEFACT (March command 35) has no admission-time
        // payload or side effect.
        // March retail projects use it as a completion/revisit reward marker,
        // so retain it in the deferred command stream without blocking the
        // briefing, script transaction, or mission creation. Reward delivery
        // remains owned by the later mission-result lifecycle.
    }
    return true;
}

bool RunDeferredMissionScripts(
    SimulationContext *context, double timeStamp,
    const std::vector<DeferredMissionCommand> &commands,
    const std::vector<PreparedMissionFile> &files,
    RecoveredLegacyScriptHost *host,
    RecruitCenterMissionProbeSummary *summary)
{
    if (context == NULL || host == NULL || summary == NULL ||
        commands.size() != files.size())
        return false;
    SRecoveredLegacyScriptProfile profile =
        RecoveredLegacyScript_RetailFragmentProfile();
    profile.compilerWordBufferSize = 64 * 1024;
    profile.compilerStringBufferSize = 128 * 1024;
    profile.compilerNameCount = 4096;
    profile.compilerTreeBufferSize = 256 * 1024;
    profile.compilerCodeStreamSize = 256 * 1024;
    profile.compilerLinkInfoSize = 64 * 1024;
    profile.processStorageStackSize = 4096;
    profile.processStackSize = 4096;
    profile.processQuants = 32768;
    profile.maximumVmSlices = 8192;

    host->BeginObjectTransaction();
    Player &player = static_cast<Player &>(g_vehicle->player());
    KR_ObjectID preservedRoutes[6];
    int preservedRouteCount = 0;
    for (int index = 0; index < player.m_missCnt && index < 6; ++index)
        if (!player.m_mission[index].m_missionRouteID.isNUL())
            preservedRoutes[preservedRouteCount++] =
                player.m_mission[index].m_missionRouteID;
    const int reclaimed = host->ReclaimUnreferencedRoutes(
        preservedRoutes, preservedRouteCount);
    if (reclaimed < 0)
    {
        const bool rolledBack = host->RollbackObjectTransaction();
        summary->scriptRollbacks += rolledBack ? 1 : 0;
        SetError("RecruitCenter Route capacity reclamation failed");
        return false;
    }
    summary->reclaimedRouteObjects = reclaimed;
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        if (commands[index].command != COM_RUN_SCRIPT) continue;
        SRecoveredLegacyScriptRunResult result = {};
        if (!RecoveredLegacyScript_RunMemory(
                files[index].source.c_str(),
                commands[index].first.c_str(), profile, context, timeStamp,
                host, &result))
        {
            const bool rolledBack = host->RollbackObjectTransaction();
            summary->scriptRollbacks += rolledBack ? 1 : 0;
            char message[256] = {};
            std::snprintf(message, sizeof(message),
                          "RecruitCenter mission script %.96s failed: %.120s",
                          commands[index].first.c_str(), result.error);
            SetError(message);
            return false;
        }
        ++summary->executedScripts;
    }
    summary->createdMissionObjects = host->TransactionCreatedObjectCount();
    return true;
}

void PresentDeferredBriefings(
    SimulationContext *context,
    const std::vector<DeferredMissionCommand> &commands,
    const std::vector<PreparedMissionFile> &files,
    RecruitCenterMissionProbeSummary *summary)
{
    if (context == NULL || summary == NULL || commands.size() != files.size() ||
        g_vehicle == NULL || !context->isExist("Briefing"))
        return;
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        if (commands[index].command != COM_PLAY_BRIEFING &&
            commands[index].command != COM_PLAY_BRIEFING_MSG)
            continue;
        const bool played = g_vehicle->m_playedBrief;
        g_vehicle->m_playedBrief = true;
        g_briefing.PlayBriefing(files[index].resolvedPath.c_str());
        g_vehicle->m_playedBrief = played;
        ++summary->presentedBriefings;
        if (commands[index].command == COM_PLAY_BRIEFING_MSG &&
            context->isExist("Publisher") && commands[index].integer > 0)
        {
            KR_Event event(commands[index].integer, Session::m_moment,
                           g_vehicle->getObjectID(),
                           context->searchObject("Publisher"));
            context->sendEventNow(event);
        }
    }
}

class RecruitCenter;

struct MissionVisitResult
{
    bool found;
    bool blocksNewMission;
    bool success;
    bool failure;
    bool repaired;
    bool refilled;
    bool rewardCreated;

    MissionVisitResult()
        : found(false), blocksNewMission(false), success(false),
          failure(false), repaired(false), refilled(false),
          rewardCreated(false) {}
};

bool ProcessMissionVisit(SimulationContext *context, double timeStamp,
                         RecruitCenter *center, Player *player,
                         MissionVisitResult *result);

bool StageMissionForCenter(SimulationContext *context, double timeStamp,
                           RecruitCenter *center, bool executeDeferred,
                           bool presentBriefing,
                           const char *requestedProject,
                           bool *staged,
                           RecruitCenterMissionProbeSummary *summary);

class RecruitCenter : public ct_Subject, public IDynamicObject
{
 public:
    RecruitCenter() { reset(); }

    int receiveEvent(KR_Event &event) override
    {
        if (event.label == t_EV_SET_ATTR_POS)
        {
            CFVector3 position;
            char commander[50] = {};
            char legacyBriefing[80] = {};
            event.data.open(EDO_READ)
                .getDouble(position.x).getDouble(position.y)
                .getDouble(position.z).getStr(commander, sizeof(commander));
            if (event.data.remaining() > 0)
                event.data.getStr(legacyBriefing, sizeof(legacyBriefing));
            const bool valid = event.data.remaining() == 0 &&
                               FiniteVector(position) && commander[0] != 0 &&
                               context->isExist(commander);
            event.data.close();
            if (!valid) return 0;
            m_position = position;
            std::snprintf(m_commander, sizeof(m_commander), "%s", commander);
            m_commanderID = context->searchObject(commander);
            if (legacyBriefing[0] != 0)
            {
                std::snprintf(m_defaultBriefing,
                              sizeof(m_defaultBriefing), "%s",
                              legacyBriefing);
                m_videoConfigured = true;
            }
            setPosition(position);
            m_direction.LoadIdentity().TranslateL(position);
            m_configured = true;
            return 1;
        }
        if (event.label == rc_SET_EJECT)
        {
            CFVector3 eject;
            event.data.open(EDO_READ).getDouble(eject.x).getDouble(eject.y)
                .getDouble(eject.z);
            const bool valid = event.data.remaining() == 0 &&
                               FiniteVector(eject);
            event.data.close();
            if (!valid) return 0;
            m_eject = eject;
            return 1;
        }
        if (event.label == rc_SET_VIDEO)
        {
            char briefing[80] = {};
            char flick[80] = {};
            event.data.open(EDO_READ).getStr(briefing, sizeof(briefing))
                .getStr(flick, sizeof(flick));
            const bool valid = event.data.remaining() == 0;
            event.data.close();
            if (!valid) return 0;
            std::snprintf(m_defaultBriefing, sizeof(m_defaultBriefing), "%s",
                          briefing);
            std::snprintf(m_defaultFlick, sizeof(m_defaultFlick), "%s", flick);
            m_videoConfigured = true;
            return 1;
        }
        if (event.label == rc_SET_DEFTAXI)
            return readSingleString(event, m_defaultTaxi,
                                    sizeof(m_defaultTaxi),
                                    &m_defaultTaxiConfigured);
        if (event.label == rc_SET_DICTIONARY)
            return readSingleString(event, m_dictionary,
                                     sizeof(m_dictionary),
                                     &m_dictionaryConfigured);
        if (event.label == t_EV_ONCOLLISION)
        {
            if (!m_configured ||
                event.data.remaining() !=
                    static_cast<int>(sizeof(KR_ObjectID)))
                return 0;
            KR_ObjectID collided;
            event.data.open(EDO_READ).getObjectID(collided).close();
            if (g_vehicle == NULL || g_vehicle->getContext() != context ||
                collided != g_vehicle->getObjectID())
            {
                ++m_rejectedCollisions;
                return 1;
            }
            ++m_playerCollisions;
            if (m_previousVisitTime >= 0.0 &&
                event.timeStamp >= m_previousVisitTime &&
                event.timeStamp - m_previousVisitTime <
                    kCollisionDebounceSeconds)
                return 1;
            Player &player = static_cast<Player &>(g_vehicle->player());
            CenterEncounterPresentation presentation;
            if (!m_probeAdmission)
                presentation = presentEncounter(player);
            return admit(event.timeStamp, &presentation);
        }
        if (event.label == rc_NEW_MISSION)
        {
            if (!m_configured || event.data.remaining() != 0) return 0;
            return admit(event.timeStamp, NULL);
        }
        if (event.label == rc_CHECK_MISSION)
        {
            int missionIndex = -1;
            event.data.open(EDO_READ).getInt(missionIndex);
            const bool valid = event.data.remaining() == 0;
            event.data.close();
            if (!valid || g_vehicle == NULL ||
                g_vehicle->getContext() != context)
                return 0;
            Player &player = static_cast<Player &>(g_vehicle->player());
            if (missionIndex < 0 || missionIndex >= player.m_missCnt)
                return 1;
            PlayerMission &mission = player.m_mission[missionIndex];
            if (mission.m_status != MISSION_INPROCESS) return 1;
            // Deferred COM_CREATE_UNITS and script owners can leave an
            // authored condition symbol unresolved during this recovery
            // milestone. NUL historically meant "already dead" to the old
            // predicate; treating it that way before creation would complete
            // a newly accepted mission falsely. Keep the real check event
            // alive until the missing side-effect owner can bind the ID.
            if (!MissionReferencesBound(mission))
            {
                event.timeStamp += 10.0;
                issueEvent(event);
                return 1;
            }
            const bool success = EvaluateConditions(&mission, context, true);
            const bool failure = EvaluateConditions(&mission, context, false);
            if (mission.success_filed ? success : failure)
            {
                mission.m_status = MISSION_SUCCESS;
                if (g_GameConsole.MessagesReady())
                {
                    g_GameConsole.PrintUrgent("Mission complete", 20,
                                              GameConsole::CENTER);
                    ++g_missionStatusPresentations;
                }
            }
            else if (mission.success_filed ? failure : success)
            {
                mission.m_status = MISSION_FAILED;
                if (g_GameConsole.MessagesReady())
                {
                    g_GameConsole.PrintUrgent("Mission failed", 20,
                                              GameConsole::CENTER);
                    ++g_missionStatusPresentations;
                }
            }
            else
            {
                event.timeStamp += 10.0;
                issueEvent(event);
            }
            return 1;
        }
        return 0;
    }

    void addNotify() override { ct_Subject::addNotify(); reset(); }
    void removeNotify() override { ct_Subject::removeNotify(); reset(); }

    void *queryInterface(int iid) override
    {
        if (iid == IUnknownIID) return static_cast<KR_Object *>(this);
        if (iid == IDynamicObjectIID)
            return static_cast<IDynamicObject *>(this);
        return NULL;
    }

    bool shouldDump() override { return true; }
    CFVector3 realPosition() override { return m_position; }
    CFVector3 getPos() override { return m_position; }
    double getHAngle() override { return 0.0; }
    CFVector3 getUpVector() override { return CFVector3(0.0, 1.0, 0.0); }
    CFVector3 getCenter() override { return CFVector3(0.0, 0.0, 0.0); }
    double getRadius() override { return kRadius; }
    double getRadius0() override { return kRadius; }
    CFVector3 getMoveDir() override { return CFVector3(0.0, 0.0, 1.0); }
    double getMoveSpeed() override { return 0.0; }
    void getMatrix(CFMatrix3x4 &matrix) override { matrix = m_direction; }
    double getMass() override { return 1.0; }
    TCCFMatrix3x4 &GetDir() override { return m_direction; }
    void SetDir(TCSFMatrix3x4 &direction) override
    {
        m_direction = direction;
        m_position = direction.Offset();
        setPosition(m_position);
    }

    bool configured() const { return m_configured; }
    bool videoConfigured() const { return m_videoConfigured; }
    bool taxiConfigured() const { return m_defaultTaxiConfigured; }
    bool dictionaryConfigured() const { return m_dictionaryConfigured; }
    const char *commander() const { return m_commander; }
    KR_ObjectID commanderID() const { return m_commanderID; }
    CFVector3 ejectPosition() const
    {
        CFVector3 target = m_position + m_eject;
        if (g_vehicle == NULL) return target;
        const double radius = g_vehicle->getRadius();
        if (!std::isfinite(radius) || radius < 0.0) return target;
        const double minimum = kRadius + radius + 1.0;
        const double horizontal = std::sqrt(
            m_eject.x * m_eject.x + m_eject.z * m_eject.z);
        if (horizontal >= minimum) return target;
        if (horizontal > 1.0e-6)
        {
            const double scale = minimum / horizontal;
            target.x = m_position.x + m_eject.x * scale;
            target.z = m_position.z + m_eject.z * scale;
        }
        else
            target.z = m_position.z + minimum;
        return target;
    }
    double previousVisitTime() const { return m_previousVisitTime; }
    void restorePreviousVisitTime(double value) { m_previousVisitTime = value; }
    int rejectedCollisions() const { return m_rejectedCollisions; }
    int playerCollisions() const { return m_playerCollisions; }
    int admissions() const { return m_admissions; }
    int stagedMissions() const { return m_stagedMissions; }
    int existingMissionVisits() const { return m_existingMissionVisits; }
    int noProjectVisits() const { return m_noProjectVisits; }
    int ejections() const { return m_ejections; }
    int admissionFailures() const { return m_admissionFailures; }
    int completedMissions() const { return m_completedMissions; }
    int failedMissions() const { return m_failedMissions; }
    int rewardsCreated() const { return m_rewardsCreated; }
    int centerPresentationAttempts() const
    {
        return m_centerPresentationAttempts;
    }
    int presentedCenterFlicks() const { return m_presentedCenterFlicks; }
    int presentedHostilityBriefings() const
    {
        return m_presentedHostilityBriefings;
    }
    int centerPresentationFailures() const
    {
        return m_centerPresentationFailures;
    }
    void setProbeAdmission(bool value) { m_probeAdmission = value; }
    const RecruitCenterMissionProbeSummary &lastMissionSummary() const
    {
        return m_lastMissionSummary;
    }

    void hash(unsigned long long &value) const
    {
        HashBytes(value, &m_position.x, sizeof(m_position.x));
        HashBytes(value, &m_position.y, sizeof(m_position.y));
        HashBytes(value, &m_position.z, sizeof(m_position.z));
        HashBytes(value, &m_eject.x, sizeof(m_eject.x));
        HashBytes(value, &m_eject.y, sizeof(m_eject.y));
        HashBytes(value, &m_eject.z, sizeof(m_eject.z));
        HashString(value, m_commander);
        HashString(value, m_defaultBriefing);
        HashString(value, m_defaultFlick);
        HashString(value, m_defaultTaxi);
        HashString(value, m_dictionary);
    }

 private:
    CenterEncounterPresentation presentEncounter(Player &player)
    {
        CenterEncounterPresentation result;
        ++result.attempts;
        ++m_centerPresentationAttempts;

        const bool renegade = player.isRenegat(m_commanderID) != 0;
        const char *authoredPath = renegade ? m_defaultBriefing
                                            : m_defaultFlick;
        PreparedMissionFile prepared;
        if (!m_videoConfigured || authoredPath[0] == 0 ||
            !context->isExist("Briefing") ||
            !PrepareMissionFile(authoredPath, false, &prepared))
        {
            ++result.failures;
            ++m_centerPresentationFailures;
        }
        else
        {
            const bool previous = g_vehicle->m_playedBrief;
            g_vehicle->m_playedBrief = true;
            g_briefing.PlayBriefing(prepared.resolvedPath.c_str());
            g_vehicle->m_playedBrief = previous;
            if (renegade)
            {
                ++result.hostilityBriefings;
                ++m_presentedHostilityBriefings;
            }
            else
            {
                ++result.centerFlicks;
                ++m_presentedCenterFlicks;
            }
        }

        // January already performed these relationship transitions before
        // falling through to rc_NEW_MISSION. March split the configured
        // presentation into the hostile default briefing and the ordinary
        // character flick; presentation failure must not skip relationship
        // ownership or mission admission.
        if (renegade) player.BetrayFor(m_commanderID);
        player.SetHostility(m_commanderID);
        return result;
    }

    int admit(double timeStamp,
              const CenterEncounterPresentation *presentation)
    {
        ++m_admissions;
        if (m_working)
        {
            ++m_existingMissionVisits;
            return 1;
        }
        if (!std::isfinite(timeStamp) || timeStamp < 0.0 ||
            g_vehicle == NULL || g_vehicle->getContext() != context)
        {
            ++m_admissionFailures;
            return 0;
        }
        m_working = true;
        const CFVector3 target = ejectPosition();
        if (!FiniteVector(target))
        {
            ++m_admissionFailures;
            m_working = false;
            return 0;
        }
        Player &player = static_cast<Player &>(g_vehicle->player());
        bool staged = false;
        RecruitCenterMissionProbeSummary summary = {};
        bool admitted = true;
        MissionVisitResult visit;
        if (!ProcessMissionVisit(context, timeStamp, this, &player, &visit))
            admitted = false;
        else if (visit.blocksNewMission)
            ++m_existingMissionVisits;
        else if (admitted)
        {
            if (visit.success) ++m_completedMissions;
            if (visit.failure) ++m_failedMissions;
            if (visit.rewardCreated) ++m_rewardsCreated;
            admitted = StageMissionForCenter(context, timeStamp, this,
                                             !m_probeAdmission,
                                             !m_probeAdmission,
                                             NULL,
                                             &staged, &summary);
            if (admitted && staged)
            {
                if (presentation != NULL)
                {
                    summary.centerPresentationAttempts =
                        presentation->attempts;
                    summary.presentedCenterFlicks =
                        presentation->centerFlicks;
                    summary.presentedHostilityBriefings =
                        presentation->hostilityBriefings;
                    summary.centerPresentationFailures =
                        presentation->failures;
                }
                ++m_stagedMissions;
                m_lastMissionSummary = summary;
                g_lastMissionSummary = summary;
                g_hasLastMissionSummary = true;
            }
            else if (admitted)
                ++m_noProjectVisits;
        }
        if (!admitted)
        {
            ++m_admissionFailures;
            m_working = false;
            return 0;
        }

        // Retail called Restart/SetPos/Stop here. Restart also repairs and
        // rebuilds unrelated vehicle state, which is not transactional yet.
        // Preserve the proven Teleport ownership boundary instead: move both
        // the vessel and ct_Subject caches, then stop at the authored eject.
        g_vehicle->SetPos(target);
        g_vehicle->setPosition(target);
        g_vehicle->Stop();
        ++m_ejections;
        m_previousVisitTime = timeStamp;
        m_working = false;
        return 1;
    }

    int readSingleString(KR_Event &event, char *destination, int capacity,
                         bool *configured)
    {
        char value[80] = {};
        event.data.open(EDO_READ).getStr(value, sizeof(value));
        const bool valid = event.data.remaining() == 0 &&
                           std::strlen(value) <
                               static_cast<std::size_t>(capacity);
        event.data.close();
        if (!valid) return 0;
        std::snprintf(destination, capacity, "%s", value);
        *configured = true;
        return 1;
    }

    void reset()
    {
        m_position = CFVector3(0.0, 0.0, 0.0);
        m_eject = CFVector3(0.0, 0.0, 0.0);
        m_commander[0] = 0;
        m_defaultBriefing[0] = 0;
        m_defaultFlick[0] = 0;
        m_defaultTaxi[0] = 0;
        m_dictionary[0] = 0;
        m_commanderID = KR_ObjectID::NUL();
        m_configured = false;
        m_videoConfigured = false;
        m_defaultTaxiConfigured = false;
        m_dictionaryConfigured = false;
        m_working = false;
        m_probeAdmission = false;
        m_previousVisitTime = -1.0;
        m_rejectedCollisions = 0;
        m_playerCollisions = 0;
        m_admissions = 0;
        m_stagedMissions = 0;
        m_existingMissionVisits = 0;
        m_noProjectVisits = 0;
        m_ejections = 0;
        m_admissionFailures = 0;
        m_completedMissions = 0;
        m_failedMissions = 0;
        m_rewardsCreated = 0;
        m_centerPresentationAttempts = 0;
        m_presentedCenterFlicks = 0;
        m_presentedHostilityBriefings = 0;
        m_centerPresentationFailures = 0;
        std::memset(&m_lastMissionSummary, 0,
                    sizeof(m_lastMissionSummary));
        m_direction.LoadIdentity();
    }

    CFVector3 m_position;
    CFVector3 m_eject;
    char m_commander[50];
    char m_defaultBriefing[80];
    char m_defaultFlick[80];
    char m_defaultTaxi[80];
    char m_dictionary[80];
    KR_ObjectID m_commanderID;
    bool m_configured;
    bool m_videoConfigured;
    bool m_defaultTaxiConfigured;
    bool m_dictionaryConfigured;
    bool m_working;
    bool m_probeAdmission;
    double m_previousVisitTime;
    int m_rejectedCollisions;
    int m_playerCollisions;
    int m_admissions;
    int m_stagedMissions;
    int m_existingMissionVisits;
    int m_noProjectVisits;
    int m_ejections;
    int m_admissionFailures;
    int m_completedMissions;
    int m_failedMissions;
    int m_rewardsCreated;
    int m_centerPresentationAttempts;
    int m_presentedCenterFlicks;
    int m_presentedHostilityBriefings;
    int m_centerPresentationFailures;
    RecruitCenterMissionProbeSummary m_lastMissionSummary;
    CFMatrix3x4 m_direction;
};

bool FindCenterCandidate(RecruitCenter *center, Player *player,
                         KR_ObjectID *candidate)
{
    if (center == NULL || player == NULL || candidate == NULL) return false;
    CandidateSearch search = {player, center->commander(),
                              KR_ObjectID::NUL(), -1, true};
    projectTable.userFind(FindCandidate, &search);
    if (!search.valid) return false;
    *candidate = search.result;
    return true;
}

bool StageMissionForCenter(SimulationContext *context, double timeStamp,
                           RecruitCenter *center, bool executeDeferred,
                           bool presentBriefing,
                           const char *requestedProject,
                           bool *staged,
                           RecruitCenterMissionProbeSummary *summary)
{
    if (staged == NULL || summary == NULL) return false;
    *staged = false;
    std::memset(summary, 0, sizeof(*summary));
    if (context == NULL || center == NULL || g_vehicle == NULL ||
        g_vehicle->getContext() != context || !std::isfinite(timeStamp) ||
        timeStamp < 0.0)
    {
        SetError("RecruitCenter admission has no live Player vehicle");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    if (player.m_missCnt < 0 || player.m_missCnt >= 6 ||
        context->eventFreeCount() < 1)
    {
        SetError("RecruitCenter admission has no transactional capacity");
        return false;
    }
    if (context->copyEventsTo(rc_CHECK_MISSION,
                              center->getObjectID(), NULL, 0) != 0)
    {
        SetError("RecruitCenter admission found a stale check event");
        return false;
    }

    KR_ObjectID candidate = KR_ObjectID::NUL();
    if (requestedProject != NULL && requestedProject[0] != 0)
    {
        if (!context->isExist(requestedProject))
        {
            SetError("RecruitCenter requested project does not exist");
            return false;
        }
        candidate = context->searchObject(requestedProject);
        bool matches = false;
        bool eligible = false;
        int requiredMissionCount = 0;
        if (!ProjectMetadata(candidate, center->commander(),
                             player.m_total_misCount, &matches, &eligible,
                             &requiredMissionCount) || !matches || !eligible ||
            HasMission(player, candidate))
        {
            SetError("RecruitCenter requested project is not eligible");
            return false;
        }
    }
    else if (!FindCenterCandidate(center, &player, &candidate))
    {
        SetError("RecruitCenter project eligibility graph is malformed");
        return false;
    }
    if (candidate.isNUL()) return true;
    const char *centerName = context->searchObject(center->getObjectID());
    const char *projectName = context->searchObject(candidate);
    if (centerName == NULL || projectName == NULL)
    {
        SetError("RecruitCenter mission selection lost a symbolic name");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->projectName, sizeof(summary->projectName), "%s",
                  projectName);

    PlayerMission mission;
    std::string routeName;
    std::vector<DeferredMissionCommand> deferredCommands;
    if (!DecodeMission(context, candidate, center->commanderID(),
                       &mission, &routeName, &deferredCommands))
    {
        SetError("RecruitCenter authored mission decode failed");
        return false;
    }

    std::vector<PreparedMissionFile> preparedFiles;
    RecoveredLegacyScriptHost scriptHost(&g_arena);
    bool scriptTransaction = false;
    if (executeDeferred)
    {
        if (!PrepareDeferredMissionFiles(deferredCommands, &preparedFiles))
            return false;
        if (!RunDeferredMissionScripts(context, timeStamp, deferredCommands,
                                       preparedFiles, &scriptHost, summary))
            return false;
        scriptTransaction = true;

        // Project nodes run from the final root towards the original first
        // node. Retail mission scripts therefore create their symbolic units
        // before the later condition nodes search those names. Decode again
        // only after the script side effects have committed to the Context.
        PlayerMission reboundMission;
        std::string reboundRoute;
        std::vector<DeferredMissionCommand> reboundDeferred;
        if (!DecodeMission(context, candidate, center->commanderID(),
                           &reboundMission, &reboundRoute,
                           &reboundDeferred) ||
            reboundDeferred.size() != deferredCommands.size() ||
            reboundRoute != routeName)
        {
            if (scriptHost.RollbackObjectTransaction())
                ++summary->scriptRollbacks;
            SetError("RecruitCenter mission changed during script execution");
            return false;
        }
        const int scriptCount = DeferredCommandCount(
            deferredCommands, COM_RUN_SCRIPT);
        if (scriptCount > 0 && ConditionCount(reboundMission) > 0 &&
            !MissionReferencesBound(reboundMission))
        {
            if (scriptHost.RollbackObjectTransaction())
                ++summary->scriptRollbacks;
            SetError("RecruitCenter mission script left an unresolved target");
            return false;
        }
        mission = reboundMission;
        routeName = reboundRoute;
        summary->reboundConditionReferences =
            scriptCount > 0 ? ConditionCount(mission) : 0;
    }

    const struct RollbackScript
    {
        static void Run(RecoveredLegacyScriptHost *host, bool active,
                        RecruitCenterMissionProbeSummary *summary)
        {
            if (active && host->RollbackObjectTransaction())
                ++summary->scriptRollbacks;
        }
    } rollbackScript = {};

    KR_ObjectID route = KR_ObjectID::NUL();
    bool createdRoute = false;
    if (!routeName.empty())
    {
        createdRoute = !context->isExist(routeName.c_str());
        route = createdRoute ? g_arena.newObject("Route", routeName.c_str())
                             : context->searchObject(routeName.c_str());
        IRouteObject *routeObject = static_cast<IRouteObject *>(
            context->queryInterface(route, IRouteObjectIID));
        if (route.isNUL() || routeObject == NULL)
        {
            if (createdRoute && !route.isNUL() && context->isExist(route))
                context->removeObject(route);
            rollbackScript.Run(&scriptHost, scriptTransaction, summary);
            SetError("RecruitCenter mission Route allocation failed");
            return false;
        }
        if (routeObject->GetNodeCnt() <= 0)
            routeObject->Load(routeName.c_str());
        if (routeObject->GetNodeCnt() <= 0)
        {
            if (createdRoute && context->isExist(route))
                context->removeObject(route);
            rollbackScript.Run(&scriptHost, scriptTransaction, summary);
            SetError("RecruitCenter mission Route did not load");
            return false;
        }
        mission.m_missionRouteID = route;
    }

    const int missionIndex = player.addMission(candidate,
                                               center->commanderID());
    if (missionIndex < 0)
    {
        if (createdRoute && context->isExist(route))
            context->removeObject(route);
        rollbackScript.Run(&scriptHost, scriptTransaction, summary);
        SetError("RecruitCenter Player mission pool is full");
        return false;
    }
    player.m_mission[missionIndex] = mission;
    player.loadNotify();

    KR_Event check(rc_CHECK_MISSION, timeStamp + 20.0,
                   g_vehicle->getObjectID(), center->getObjectID());
    check.data.open(EDO_WRITE).putInt(missionIndex).close();
    context->addEvent(check);
    if (context->copyEventsTo(rc_CHECK_MISSION,
                              center->getObjectID(), NULL, 0) != 1)
    {
        context->removeEventsTo(rc_CHECK_MISSION, center->getObjectID());
        for (int move = missionIndex; move + 1 < player.m_missCnt; ++move)
            player.m_mission[move] = player.m_mission[move + 1];
        --player.m_missCnt;
        if (player.m_total_misCount > 0) --player.m_total_misCount;
        player.loadNotify();
        if (createdRoute && context->isExist(route))
            context->removeObject(route);
        rollbackScript.Run(&scriptHost, scriptTransaction, summary);
        SetError("RecruitCenter mission check event was not queued");
        return false;
    }

    summary->stagedMissions = 1;
    summary->conditionReferences = ConditionCount(mission);
    summary->routeReferences = mission.m_missionRouteID.isNUL() ? 0 : 1;
    summary->deferredCommands = static_cast<int>(deferredCommands.size());
    summary->briefingCommands =
        DeferredCommandCount(deferredCommands, COM_PLAY_BRIEFING) +
        DeferredCommandCount(deferredCommands, COM_PLAY_BRIEFING_MSG);
    summary->scriptCommands =
        DeferredCommandCount(deferredCommands, COM_RUN_SCRIPT);
    summary->deferredArtefactRewards = DeferredCommandCount(
        deferredCommands, kSetGiveArtefactCommand);
    if (scriptTransaction) scriptHost.CommitObjectTransaction();
    if (presentBriefing)
        PresentDeferredBriefings(context, deferredCommands, preparedFiles,
                                 summary);
    g_lastMissionSummary = *summary;
    g_hasLastMissionSummary = true;
    *staged = true;
    return true;
}

class RecruitCenterTable : public ct_SubjectTable
{
 public:
    RecruitCenterTable() : m_table(NULL) { registerClass("RecruitCenter"); }
    ~RecruitCenterTable() { delete [] m_table; }

    void allocObjects(int count) override
    {
        m_table = count <= 0 ? NULL :
            new (std::nothrow) RecruitCenter[count];
        if (m_table == NULL) m_maxObjectQnty = 0;
    }

    void freeObjects() override
    {
        delete [] m_table;
        m_table = NULL;
        m_maxObjectQnty = 0;
    }

    ct_Object *getObjectPTR(int index) override
    {
        s_ASSERT(index >= 0 && index < m_maxObjectQnty,
                 "RecruitCenterTable::getObjectPTR");
        return &m_table[index];
    }

    bool isRendering() override { return false; }
    bool isAudible() override { return false; }
    int capacity() const { return m_maxObjectQnty; }

    int liveCount() const
    {
        int count = 0;
        for (ct_Subject *subject = findFirstSubject(); subject != NULL;
             subject = findNextSubject(subject)) ++count;
        return count;
    }

 private:
    RecruitCenter *m_table;
};

RecruitCenterTable g_recruitCenterTable;

RecruitCenter *FindRecruitCenter(SimulationContext *context,
                                 const char *centerName)
{
    if (context == NULL || centerName == NULL || centerName[0] == 0)
        return NULL;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        const char *name = context->searchObject(subject->getObjectID());
        if (name != NULL && std::strcmp(name, centerName) == 0)
            return static_cast<RecruitCenter *>(subject);
    }
    return NULL;
}

bool MissionCheckGraphExact(SimulationContext *context, Player *player)
{
    if (context == NULL || player == NULL) return false;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        RecruitCenter *center = static_cast<RecruitCenter *>(subject);
        KR_Event events[6];
        const int count = context->copyEventsTo(
            rc_CHECK_MISSION, center->getObjectID(), events, 6);
        if (count < 0 || count > 6) return false;
        for (int eventIndex = 0; eventIndex < count; ++eventIndex)
        {
            int missionIndex = -1;
            events[eventIndex].data.open(EDO_READ).getInt(missionIndex);
            const bool exact = events[eventIndex].data.remaining() == 0 &&
                missionIndex >= 0 && missionIndex < player->m_missCnt &&
                player->m_mission[missionIndex].comID ==
                    center->commanderID();
            events[eventIndex].data.close();
            if (!exact) return false;
        }
    }
    return true;
}

bool RewriteMissionCheckEvents(SimulationContext *context, int removedIndex,
                               KR_ObjectID removedCenter)
{
    struct CenterEvents
    {
        KR_ObjectID center;
        std::vector<KR_Event> events;
        bool changed;
    };
    std::vector<CenterEvents> all;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        CenterEvents entry;
        entry.center = subject->getObjectID();
        entry.changed = false;
        const int count = context->copyEventsTo(
            rc_CHECK_MISSION, entry.center, NULL, 0);
        if (count < 0 || count > 6) return false;
        entry.events.resize(static_cast<std::size_t>(count));
        if (count > 0 && context->copyEventsTo(
                rc_CHECK_MISSION, entry.center, &entry.events[0], count) !=
                count)
            return false;
        std::vector<KR_Event> retained;
        for (int eventIndex = 0; eventIndex < count; ++eventIndex)
        {
            KR_Event event = entry.events[eventIndex];
            int missionIndex = -1;
            event.data.open(EDO_READ).getInt(missionIndex);
            const bool valid = event.data.remaining() == 0;
            event.data.close();
            if (!valid || missionIndex < 0 || missionIndex >= 6)
                return false;
            if (missionIndex == removedIndex && entry.center == removedCenter)
            {
                entry.changed = true;
                continue;
            }
            if (missionIndex > removedIndex)
            {
                event.data.open(EDO_WRITE).putInt(missionIndex - 1).close();
                entry.changed = true;
            }
            retained.push_back(event);
        }
        entry.events.swap(retained);
        all.push_back(entry);
    }
    for (std::size_t center = 0; center < all.size(); ++center)
    {
        if (!all[center].changed) continue;
        context->removeEventsTo(rc_CHECK_MISSION, all[center].center);
        for (std::size_t event = 0; event < all[center].events.size(); ++event)
            context->addEvent(all[center].events[event]);
    }
    return true;
}

void RemoveMissionReward(SimulationContext *context, KR_ObjectID object)
{
    if (context == NULL || object.isNUL()) return;
    if (context->isExist(object)) context->removeObject(object);
    // Artefact::removeNotify() drops an attached artefact and may schedule a
    // fresh private move event.  Failed reward publication must not leak it.
    while (context->removeEvent(ARTEFACT_MOVE, object) == 1) {}
    while (context->removeEvent(ARTEFACT_CHANGEDIR, object) == 1) {}
}

bool CreateMissionReward(SimulationContext *context, double timeStamp,
                         RecruitCenter *center, KR_ObjectID *created)
{
    *created = KR_ObjectID::NUL();
    if (context->isExist("Artifact"))
    {
        SetError("RecruitCenter reward name Artifact is already occupied");
        return false;
    }
    if (!context->isExist("Artefact.Attr.0"))
    {
        SetError("RecruitCenter reward attribute Artefact.Attr.0 is absent");
        return false;
    }
    const KR_ObjectID attribute = context->searchObject("Artefact.Attr.0");
    if (!ArtefactAttributeState_IsKnown(attribute))
    {
        SetError("RecruitCenter reward attribute has the wrong class");
        return false;
    }
    if (!ArtefactAttributeState_ResolveReferences(context, timeStamp))
    {
        SetError("RecruitCenter reward Artefact references are unresolved");
        return false;
    }
    if (g_arena.searchSeanceClassTable("Artefact") == ct_NULLID)
    {
        SetError("RecruitCenter reward Artefact subject table is absent");
        return false;
    }
    KR_ObjectID object = g_arena.newObject("Artefact", "Artifact");
    if (object.isNUL())
    {
        SetError("RecruitCenter reward Artefact subject table is full");
        return false;
    }
    IArtefact *artefact = static_cast<IArtefact *>(
        context->queryInterface(object, IArtefactIID));
    if (artefact == NULL)
    {
        RemoveMissionReward(context, object);
        SetError("RecruitCenter reward object has no IArtefact interface");
        return false;
    }
    KR_Event setAttribute(KR_SET_ATTR, timeStamp,
                          center->getObjectID(), object);
    setAttribute.data.open(EDO_WRITE).putObjectID(attribute).close();
    context->sendEventNow(setAttribute);
    if (!ArtefactActiveWorldState_IsReady(context, object))
    {
        RemoveMissionReward(context, object);
        SetError("RecruitCenter reward attribute publication failed");
        return false;
    }
    IUnit *unit = static_cast<IUnit *>(
        context->queryInterface(object, IUnitIID));
    if (unit == NULL)
    {
        RemoveMissionReward(context, object);
        SetError("RecruitCenter reward object has no IUnit interface");
        return false;
    }
    unit->setCommander(KR_ObjectID::NUL());
    artefact->artefactMove(CFVector3(0.0, 0.0, 0.0));
    // May retail uses the fixed Artifact identity and offsets the center's
    // first two position components by +50/+30 before publishing the pickup.
    const CFVector3 position = center->getPos() + CFVector3(50.0, 30.0, 0.0);
    if (!FiniteVector(position))
    {
        RemoveMissionReward(context, object);
        SetError("RecruitCenter reward position is invalid");
        return false;
    }
    CFMatrix3x4 orientation;
    orientation.LoadIdentity().TranslateL(position);
    artefact->moveTo(orientation);
    *created = object;
    return true;
}

bool ProcessMissionVisit(SimulationContext *context, double timeStamp,
                         RecruitCenter *center, Player *player,
                         MissionVisitResult *result)
{
    if (context == NULL || center == NULL || player == NULL ||
        result == NULL || !std::isfinite(timeStamp)) return false;
    *result = MissionVisitResult();
    int missionIndex = -1;
    for (int index = 0; index < player->m_missCnt; ++index)
        if (player->m_mission[index].comID == center->commanderID())
        {
            missionIndex = index;
            break;
        }
    if (missionIndex < 0) return true;
    result->found = true;
    PlayerMission &mission = player->m_mission[missionIndex];
    if (mission.m_status == MISSION_INPROCESS ||
        mission.m_status == MISSION_NONE)
    {
        if (!std::isfinite(g_levelAttr.m_minDamage) ||
            g_levelAttr.m_minDamage < 0.0 ||
            g_levelAttr.m_minDamage > 1.0 ||
            g_levelAttr.m_minSecBulletCnt < 0)
        {
            SetError("RecruitCenter service thresholds are invalid");
            return false;
        }
        if (g_vehicle->m_secBulletCnt < g_levelAttr.m_minSecBulletCnt)
        {
            g_vehicle->m_secBulletCnt = g_levelAttr.m_minSecBulletCnt;
            result->refilled = true;
        }
        if (g_vehicle->m_damage < g_levelAttr.m_minDamage)
        {
            g_vehicle->m_damage = g_levelAttr.m_minDamage;
            result->repaired = true;
        }
        result->blocksNewMission = true;
        return true;
    }

    const bool success = mission.m_status == MISSION_SUCCESS;
    const bool failure = mission.m_status == MISSION_FAILED ||
                         mission.m_status == MISSION_SURRENDER;
    if (!success && !failure)
    {
        SetError("RecruitCenter mission has an unsupported result state");
        return false;
    }
    if (success && (!std::isfinite(g_levelAttr.m_minDamage) ||
                    g_levelAttr.m_maxSecBulletCnt < 0))
    {
        SetError("RecruitCenter success service thresholds are invalid");
        return false;
    }
    const KR_ObjectID completedProject = mission.mID;
    mp_Project *project = projectTable.searchProject(completedProject);
    if (project == NULL)
    {
        SetError("RecruitCenter completed project is missing");
        return false;
    }
    const bool retireCompletedProject = project->m_permanent == 0;
    KR_ObjectID reward = KR_ObjectID::NUL();
    if (success && mission.m_giveArtefact &&
        !CreateMissionReward(context, timeStamp, center, &reward))
        return false;
    if (!RewriteMissionCheckEvents(context, missionIndex,
                                   center->getObjectID()))
    {
        RemoveMissionReward(context, reward);
        SetError("RecruitCenter mission check reindex failed");
        return false;
    }
    if (success)
    {
        g_vehicle->m_damage = 1.0;
        if (g_vehicle->m_secBulletCnt < g_levelAttr.m_maxSecBulletCnt)
            g_vehicle->m_secBulletCnt = g_levelAttr.m_maxSecBulletCnt;
    }
    for (int move = missionIndex; move + 1 < player->m_missCnt; ++move)
        player->m_mission[move] = player->m_mission[move + 1];
    --player->m_missCnt;
    player->loadNotify();
    // Retail Player::CleanupMissionPool removes non-permanent authored
    // projects with their terminal mission. Keep an equivalent serialized
    // tombstone here: it is excluded by ProjectMetadata, while CTJ1 can still
    // restore an earlier checkpoint without recreating a vanished object.
    if (retireCompletedProject)
        project->m_treeNode = -1;
    if (g_GameConsole.MessagesReady())
    {
        const bool renegade = player->isRenegat(center->commanderID()) != 0;
        const char *message = success
            ? (renegade
                   ? "You use nasty methods but do your job well"
                   : "Well done, great job")
            : (renegade
                   ? "You did your best but failed! We're disappointed"
                   : "You're nuts! Go fight and proove your loyalty");
        g_GameConsole.PrintUrgent(message, 12, GameConsole::CENTER);
        ++g_missionResultPresentations;
    }
    result->success = success;
    result->failure = failure;
    result->rewardCreated = !reward.isNUL();
    return true;
}

bool SameVector(const CFVector3 &left, const CFVector3 &right)
{
    const double epsilon = 1.0e-7;
    return std::fabs(left.x - right.x) <= epsilon &&
           std::fabs(left.y - right.y) <= epsilon &&
           std::fabs(left.z - right.z) <= epsilon;
}

bool RewardCarrierState(SimulationContext *context, bool expectAttached)
{
    if (context == NULL || g_vehicle == NULL ||
        g_vehicle->getContext() != context || !context->isExist("Artifact"))
        return false;
    const KR_ObjectID artefactID = context->searchObject("Artifact");
    IArtefact *artefact = static_cast<IArtefact *>(
        context->queryInterface(artefactID, IArtefactIID));
    if (artefact == NULL) return false;
    if (!expectAttached)
        return g_vehicle->m_artefact == NULL &&
               g_vehicle->m_artefactID.isNUL() &&
               artefact->m_carrier == NULL && artefact->m_carrierID.isNUL();
    return g_vehicle->m_artefact == artefact &&
           g_vehicle->m_artefactID == artefactID &&
           artefact->m_carrier == static_cast<ICarrier *>(g_vehicle) &&
           artefact->m_carrierID == g_vehicle->getObjectID();
}

}  // namespace

void RecruitCenterSubjectState_Link()
{
}

bool RecruitCenterSubjectState_TableReady(SimulationContext *context)
{
    return context != NULL && g_arena.getContext() == context &&
           g_recruitCenterTable.capacity() > 0 &&
           g_recruitCenterTable.capacity() <= kMaximumCapacity &&
           g_arena.searchSeanceClassTable("RecruitCenter") ==
               g_recruitCenterTable.getClassTableID() &&
           RecruitCenterSubjectState_ConfiguredCount() ==
               g_recruitCenterTable.liveCount();
}

int RecruitCenterSubjectState_Capacity()
{
    return g_recruitCenterTable.capacity();
}

int RecruitCenterSubjectState_LiveCount()
{
    return g_recruitCenterTable.liveCount();
}

int RecruitCenterSubjectState_ConfiguredCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        if (static_cast<RecruitCenter *>(subject)->configured()) ++count;
    return count;
}

int RecruitCenterSubjectState_VideoCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        if (static_cast<RecruitCenter *>(subject)->videoConfigured()) ++count;
    return count;
}

int RecruitCenterSubjectState_DefaultTaxiCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        if (static_cast<RecruitCenter *>(subject)->taxiConfigured()) ++count;
    return count;
}

int RecruitCenterSubjectState_DictionaryCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        if (static_cast<RecruitCenter *>(subject)->dictionaryConfigured())
            ++count;
    return count;
}

int RecruitCenterSubjectState_RejectedCollisionCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->rejectedCollisions();
    return count;
}

int RecruitCenterSubjectState_PlayerCollisionCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->playerCollisions();
    return count;
}

int RecruitCenterSubjectState_AdmissionCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->admissions();
    return count;
}

int RecruitCenterSubjectState_StagedMissionCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->stagedMissions();
    return count;
}

int RecruitCenterSubjectState_ExistingMissionVisitCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->existingMissionVisits();
    return count;
}

int RecruitCenterSubjectState_NoProjectVisitCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->noProjectVisits();
    return count;
}

int RecruitCenterSubjectState_EjectionCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->ejections();
    return count;
}

int RecruitCenterSubjectState_AdmissionFailureCount()
{
    int count = 0;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
        count += static_cast<RecruitCenter *>(subject)->admissionFailures();
    return count;
}

unsigned long long RecruitCenterSubjectState_Fingerprint(
    SimulationContext *context)
{
    if (!RecruitCenterSubjectState_TableReady(context)) return 0;
    unsigned long long hash = kHashOffset;
    const int capacity = g_recruitCenterTable.capacity();
    HashBytes(hash, &capacity, sizeof(capacity));
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        const char *name = context->searchObject(subject->getObjectID());
        if (name == NULL) return 0;
        HashString(hash, name);
        static_cast<RecruitCenter *>(subject)->hash(hash);
    }
    return hash;
}

bool RecruitCenterSubjectState_StageMissionProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (staged == NULL || summary == NULL) return false;
    *staged = false;
    std::memset(summary, 0, sizeof(*summary));
    if (!RecruitCenterSubjectState_TableReady(context)) return true;
    if (g_vehicle == NULL || g_vehicle->getContext() != context ||
        !std::isfinite(timeStamp) || timeStamp < 0.0)
    {
        SetError("RecruitCenter mission probe has no live Player vehicle");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    const CFVector3 originalPosition = g_vehicle->Pos();
    const CFVector3 originalSubjectPosition = g_vehicle->getPosition();
    const CFVector3 originalSpeed = g_vehicle->Speed();
    const CFMatrix3x4 originalDirection = g_vehicle->GetDir();
    const double speedSquared = originalSpeed.x * originalSpeed.x +
        originalSpeed.y * originalSpeed.y +
        originalSpeed.z * originalSpeed.z;
    if (player.m_missCnt < 0 || player.m_missCnt >= 6 ||
        context->eventFreeCount() < 1 || !FiniteVector(originalPosition) ||
        !FiniteVector(originalSubjectPosition) ||
        !FiniteVector(originalSpeed) || speedSquared > 1.0e-8)
    {
        SetError("RecruitCenter mission probe needs a stationary boundary");
        return false;
    }
    const KR_ObjectID vehicleID = g_vehicle->getObjectID();

    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        RecruitCenter *center = static_cast<RecruitCenter *>(subject);
        KR_ObjectID candidate = KR_ObjectID::NUL();
        if (!FindCenterCandidate(center, &player, &candidate))
        {
            SetError("RecruitCenter project eligibility graph is malformed");
            return false;
        }
        if (candidate.isNUL()) continue;

        const int missionCount = player.m_missCnt;
        const int rejected = center->rejectedCollisions();
        const int collisions = center->playerCollisions();
        const int admissions = center->admissions();
        const int existing = center->existingMissionVisits();
        const int ejections = center->ejections();
        const double previousVisitTime = center->previousVisitTime();

        KR_Event nonPlayer(t_EV_ONCOLLISION, timeStamp,
                           g_arena.getObjectID(), center->getObjectID());
        nonPlayer.data.open(EDO_WRITE)
            .putObjectID(center->getObjectID()).close();
        context->sendEventNow(nonPlayer);
        if (center->rejectedCollisions() != rejected + 1 ||
            player.m_missCnt != missionCount)
        {
            SetError("RecruitCenter accepted a non-Player collision");
            return false;
        }

        KR_Event collision(t_EV_ONCOLLISION, timeStamp + 0.25,
                           vehicleID, center->getObjectID());
        collision.data.open(EDO_WRITE).putObjectID(vehicleID).close();
        center->setProbeAdmission(true);
        context->sendEventNow(collision);

        KR_Event repeat(rc_NEW_MISSION, timeStamp + 0.5,
                        vehicleID, center->getObjectID());
        context->sendEventNow(repeat);
        center->setProbeAdmission(false);

        const CFVector3 target = center->ejectPosition();
        const CFVector3 actual = g_vehicle->Pos();
        const CFVector3 subjectPosition = g_vehicle->getPosition();
        const double dx = actual.x - target.x;
        const double dy = actual.y - target.y;
        const double dz = actual.z - target.z;
        const double sdx = subjectPosition.x - target.x;
        const double sdy = subjectPosition.y - target.y;
        const double sdz = subjectPosition.z - target.z;
        const bool proven =
            center->playerCollisions() == collisions + 1 &&
            center->admissions() == admissions + 2 &&
            center->existingMissionVisits() == existing + 1 &&
            center->ejections() == ejections + 2 &&
            player.m_missCnt == missionCount + 1 &&
            context->copyEventsTo(rc_CHECK_MISSION,
                                  center->getObjectID(), NULL, 0) == 1 &&
            dx * dx + dy * dy + dz * dz <= 1.0e-8 &&
            sdx * sdx + sdy * sdy + sdz * sdz <= 1.0e-8;

        g_vehicle->SetDir(originalDirection);
        g_vehicle->SetPos(originalPosition);
        g_vehicle->setPosition(originalSubjectPosition);
        g_vehicle->Stop();
        center->restorePreviousVisitTime(previousVisitTime);
        if (!proven)
        {
            SetError("RecruitCenter public admission proof did not commit");
            return false;
        }

        *summary = center->lastMissionSummary();
        summary->rejectedNonPlayerCollisions = 1;
        summary->acceptedPlayerCollisions = 1;
        summary->admissionEvents = 2;
        summary->ejections = 2;
        *staged = true;
        return true;
    }
    return true;
}

bool RecruitCenterSubjectState_CompleteMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionResultProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (summary == NULL || context == NULL || centerName == NULL ||
        centerName[0] == 0 || !std::isfinite(timeStamp) ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter result probe arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = NULL;
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        const char *name = context->searchObject(subject->getObjectID());
        if (name != NULL && std::strcmp(name, centerName) == 0)
        {
            center = static_cast<RecruitCenter *>(subject);
            break;
        }
    }
    if (center == NULL)
    {
        SetError("RecruitCenter result probe cannot find the requested center");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    int missionIndex = -1;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID() &&
            player.m_mission[index].m_status == MISSION_INPROCESS)
        {
            missionIndex = index;
            break;
        }
    if (missionIndex < 0)
    {
        SetError("RecruitCenter result probe has no active center mission");
        return false;
    }
    PlayerMission &mission = player.m_mission[missionIndex];
    const char *projectName = context->searchObject(mission.mID);
    if (projectName == NULL || !mission.m_giveArtefact ||
        mission.success_needKill.getCount() <= 0)
    {
        SetError("RecruitCenter result probe needs a kill/reward mission");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->completedProjectName,
                  sizeof(summary->completedProjectName), "%s", projectName);
    summary->missionsBefore = player.m_missCnt;
    summary->totalMissionsBefore = player.m_total_misCount;

    std::vector<KR_ObjectID> targets;
    for (int index = 0; index < mission.success_needKill.getCount(); ++index)
        if (context->isExist(mission.success_needKill[index]))
            targets.push_back(mission.success_needKill[index]);
    context->removeEventsTo(rc_CHECK_MISSION, center->getObjectID());
    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        if (context->isExist(targets[index]))
        {
            context->removeObject(targets[index]);
            ++summary->conditionsRemoved;
        }
    }
    KR_Event check(rc_CHECK_MISSION, timeStamp,
                   g_vehicle->getObjectID(), center->getObjectID());
    check.data.open(EDO_WRITE).putInt(missionIndex).close();
    context->sendEventNow(check);
    if (player.m_mission[missionIndex].m_status != MISSION_SUCCESS)
    {
        SetError("RecruitCenter real condition check did not succeed");
        return false;
    }
    summary->statusTransitions = 1;
    g_vehicle->m_damage = 0.1;
    g_vehicle->m_secBulletCnt = 0;
    MissionVisitResult result;
    if (!ProcessMissionVisit(context, timeStamp + 0.1, center, &player,
                             &result) || !result.success ||
        !result.rewardCreated)
    {
        if (g_lastError[0] == 0)
            SetError("RecruitCenter successful revisit did not commit");
        return false;
    }
    summary->completedMissions = 1;
    summary->rewardsCreated = 1;
    summary->repaired = g_vehicle->m_damage == 1.0 ? 1 : 0;
    summary->refilled =
        g_vehicle->m_secBulletCnt >= g_levelAttr.m_maxSecBulletCnt ? 1 : 0;
    summary->missionsAfter = player.m_missCnt;
    summary->totalMissionsAfter = player.m_total_misCount;
    summary->rewardInterfaceReady = context->isExist("Artifact") &&
        context->queryInterface(context->searchObject("Artifact"),
                                IArtefactIID) != NULL ? 1 : 0;
    const KR_ObjectID reward = context->searchObject("Artifact");
    summary->pickupAccepted = g_vehicle->carrierOnCollision(
        g_vehicle->getObjectID(), reward) ? 1 : 0;
    summary->bidirectionalAttachment =
        RewardCarrierState(context, true) ? 1 : 0;
    KR_Event privateEvents[2];
    summary->carryEventsCancelled =
        context->copyEvents(ARTEFACT_MOVE, reward, privateEvents, 2) == 0 &&
        context->copyEvents(ARTEFACT_CHANGEDIR, reward,
                            privateEvents, 2) == 0 ? 1 : 0;
    g_vehicle->carrierOnMove();
    IDynamicObject *rewardDynamic = static_cast<IDynamicObject *>(
        context->queryInterface(reward, IDynamicObjectIID));
    CFMatrix3x4 expectedCarry;
    CFMatrix3x4 actualCarry;
    g_vehicle->carrierLoadMatrix(expectedCarry);
    if (rewardDynamic != NULL) rewardDynamic->getMatrix(actualCarry);
    summary->carryMoveMatched = rewardDynamic != NULL &&
        SameVector(expectedCarry.Offset(), actualCarry.Offset()) ? 1 : 0;

    KR_ObjectID next = KR_ObjectID::NUL();
    if (!FindCenterCandidate(center, &player, &next))
    {
        SetError("RecruitCenter next-project graph is malformed");
        return false;
    }
    if (!next.isNUL())
    {
        const char *nextName = context->searchObject(next);
        if (nextName == NULL) return false;
        std::snprintf(summary->nextProjectName,
                      sizeof(summary->nextProjectName), "%s", nextName);
    }
    MissionVisitResult repeat;
    const bool repeated = ProcessMissionVisit(
        context, timeStamp + 0.2, center, &player, &repeat);
    summary->repeatIdempotent = repeated && !repeat.found &&
        context->isExist("Artifact") &&
        player.m_missCnt == summary->missionsAfter ? 1 : 0;
    const bool exact = summary->conditionsRemoved ==
                           static_cast<int>(targets.size()) &&
        summary->statusTransitions == 1 &&
        summary->completedMissions == 1 && summary->rewardsCreated == 1 &&
        summary->rewardInterfaceReady == 1 && summary->repaired == 1 &&
        summary->refilled == 1 && summary->repeatIdempotent == 1 &&
        summary->pickupAccepted == 1 &&
        summary->bidirectionalAttachment == 1 &&
        summary->carryEventsCancelled == 1 &&
        summary->carryMoveMatched == 1 &&
        summary->missionsAfter == summary->missionsBefore - 1 &&
        summary->totalMissionsAfter == summary->totalMissionsBefore &&
        std::strcmp(summary->completedProjectName,
                    summary->nextProjectName) != 0;
    if (!exact)
        SetError("RecruitCenter result invariants did not hold");
    return exact;
}

bool RecruitCenterSubjectState_CompleteNoRewardMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionNoRewardResultProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (summary == NULL || context == NULL || centerName == NULL ||
        centerName[0] == 0 || !std::isfinite(timeStamp) ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter no-reward result arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL)
    {
        SetError("RecruitCenter no-reward result cannot find its center");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    int missionIndex = -1;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID() &&
            player.m_mission[index].m_status == MISSION_INPROCESS)
        {
            missionIndex = index;
            break;
        }
    if (missionIndex < 0)
    {
        SetError("RecruitCenter no-reward result has no active mission");
        return false;
    }
    PlayerMission &mission = player.m_mission[missionIndex];
    const KR_ObjectID completedProject = mission.mID;
    const char *projectName = context->searchObject(completedProject);
    if (projectName == NULL || mission.m_giveArtefact ||
        mission.success_needKill.getCount() <= 0)
    {
        SetError("RecruitCenter no-reward result needs a kill mission "
                 "without COM_SET_GIVEARTEFACT");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->completedProjectName,
                  sizeof(summary->completedProjectName), "%s", projectName);
    summary->missionsBefore = player.m_missCnt;
    summary->totalMissionsBefore = player.m_total_misCount;
    summary->scheduledChecksBefore = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);
    if (summary->scheduledChecksBefore != 1)
    {
        SetError("RecruitCenter no-reward result has no unique check event");
        return false;
    }

    std::vector<KR_ObjectID> targets;
    for (int index = 0; index < mission.success_needKill.getCount(); ++index)
        if (context->isExist(mission.success_needKill[index]))
            targets.push_back(mission.success_needKill[index]);
    if (targets.size() !=
        static_cast<std::size_t>(mission.success_needKill.getCount()))
    {
        SetError("RecruitCenter no-reward kill graph is already incomplete");
        return false;
    }
    context->removeEventsTo(rc_CHECK_MISSION, center->getObjectID());
    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        context->removeObject(targets[index]);
        ++summary->conditionsRemoved;
    }
    KR_Event check(rc_CHECK_MISSION, timeStamp,
                   g_vehicle->getObjectID(), center->getObjectID());
    check.data.open(EDO_WRITE).putInt(missionIndex).close();
    context->sendEventNow(check);
    if (player.m_mission[missionIndex].m_status != MISSION_SUCCESS)
    {
        SetError("RecruitCenter no-reward real condition check did not "
                 "succeed");
        return false;
    }
    summary->statusTransitions = 1;

    const bool artifactExisted = context->isExist("Artifact") != 0;
    const KR_ObjectID artifactBefore = artifactExisted
        ? context->searchObject("Artifact") : KR_ObjectID::NUL();
    const int presentationsBefore = g_missionResultPresentations;
    g_vehicle->m_damage = 0.1;
    g_vehicle->m_secBulletCnt = 0;
    MissionVisitResult result;
    if (!ProcessMissionVisit(context, timeStamp + 0.1, center, &player,
                             &result) || !result.success ||
        result.rewardCreated)
    {
        if (g_lastError[0] == 0)
            SetError("RecruitCenter no-reward revisit did not commit");
        return false;
    }
    summary->completedMissions = 1;
    summary->resultPresentations =
        g_missionResultPresentations - presentationsBefore;
    summary->rewardsCreated = result.rewardCreated ? 1 : 0;
    const bool artifactExistsAfter = context->isExist("Artifact") != 0;
    summary->rewardAbsent =
        artifactExistsAfter == artifactExisted &&
        (!artifactExistsAfter ||
         context->searchObject("Artifact") == artifactBefore) ? 1 : 0;
    mp_Project *completedProjectState =
        projectTable.searchProject(completedProject);
    summary->completedProjectRetired = completedProjectState != NULL &&
        completedProjectState->m_treeNode < 0 ? 1 : 0;
    summary->repaired = g_vehicle->m_damage == 1.0 ? 1 : 0;
    summary->refilled =
        g_vehicle->m_secBulletCnt >= g_levelAttr.m_maxSecBulletCnt ? 1 : 0;
    summary->missionsAfter = player.m_missCnt;
    summary->totalMissionsAfter = player.m_total_misCount;
    summary->scheduledChecksAfter = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);

    KR_ObjectID next = KR_ObjectID::NUL();
    if (!FindCenterCandidate(center, &player, &next) || next.isNUL())
    {
        SetError("RecruitCenter no-reward next project is unavailable");
        return false;
    }
    const char *nextName = context->searchObject(next);
    if (nextName == NULL)
    {
        SetError("RecruitCenter no-reward next project lost its name");
        return false;
    }
    std::snprintf(summary->nextProjectName,
                  sizeof(summary->nextProjectName), "%s", nextName);
    MissionVisitResult repeat;
    const bool repeated = ProcessMissionVisit(
        context, timeStamp + 0.2, center, &player, &repeat);
    summary->repeatIdempotent = repeated && !repeat.found &&
        player.m_missCnt == summary->missionsAfter &&
        (context->isExist("Artifact") != 0) == artifactExisted ? 1 : 0;

    const bool exact = summary->conditionsRemoved ==
                           static_cast<int>(targets.size()) &&
        summary->statusTransitions == 1 &&
        summary->completedMissions == 1 &&
        summary->resultPresentations == 1 &&
        summary->rewardsCreated == 0 && summary->rewardAbsent == 1 &&
        summary->completedProjectRetired == 1 &&
        summary->repaired == 1 && summary->refilled == 1 &&
        summary->repeatIdempotent == 1 &&
        summary->missionsAfter == summary->missionsBefore - 1 &&
        summary->totalMissionsAfter == summary->totalMissionsBefore &&
        summary->scheduledChecksAfter == 0 &&
        std::strcmp(summary->completedProjectName,
                    summary->nextProjectName) != 0;
    if (!exact)
        SetError("RecruitCenter no-reward result invariants did not hold");
    return exact;
}

bool RecruitCenterSubjectState_CompleteTerminalNoRewardMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionNoRewardResultProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (summary == NULL || context == NULL || centerName == NULL ||
        centerName[0] == 0 || !std::isfinite(timeStamp) ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter terminal no-reward arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL)
    {
        SetError("RecruitCenter terminal no-reward cannot find its center");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    int missionIndex = -1;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID() &&
            player.m_mission[index].m_status == MISSION_INPROCESS)
        {
            missionIndex = index;
            break;
        }
    if (missionIndex < 0)
    {
        SetError("RecruitCenter terminal no-reward has no active mission");
        return false;
    }

    PlayerMission &mission = player.m_mission[missionIndex];
    const KR_ObjectID completedProject = mission.mID;
    const char *projectName = context->searchObject(completedProject);
    if (projectName == NULL || mission.m_giveArtefact ||
        ConditionCount(mission) != 1 ||
        mission.success_needReached.getCount() != 1 ||
        mission.success_needReached[0] != g_vehicle->getObjectID() ||
        !std::isfinite(mission.success_reachedPos[0].x) ||
        !std::isfinite(mission.success_reachedPos[0].y) ||
        !std::isfinite(mission.success_reachedRadius[0]) ||
        mission.success_reachedRadius[0] <= 0.0)
    {
        SetError("RecruitCenter terminal no-reward needs one real Player "
                 "reached condition without COM_SET_GIVEARTEFACT");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->completedProjectName,
                  sizeof(summary->completedProjectName), "%s", projectName);
    summary->missionsBefore = player.m_missCnt;
    summary->totalMissionsBefore = player.m_total_misCount;
    summary->scheduledChecksBefore = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);
    if (summary->scheduledChecksBefore != 1)
    {
        SetError("RecruitCenter terminal no-reward has no unique check event");
        return false;
    }

    const CFVector3 originalPosition = g_vehicle->Pos();
    const double targetX = mission.success_reachedPos[0].x;
    const double targetZ = mission.success_reachedPos[0].y;
    const double radius = mission.success_reachedRadius[0];
    const double originalDx = originalPosition.x - targetX;
    const double originalDz = originalPosition.z - targetZ;
    if (!FiniteVector(originalPosition) ||
        originalDx * originalDx + originalDz * originalDz <= radius * radius)
    {
        SetError("RecruitCenter terminal reached condition is already true");
        return false;
    }
    const CFVector3 target(targetX, originalPosition.y, targetZ);
    g_vehicle->SetPos(target);
    g_vehicle->setPosition(target);
    g_vehicle->Stop();
    context->removeEventsTo(rc_CHECK_MISSION, center->getObjectID());
    KR_Event check(rc_CHECK_MISSION, timeStamp,
                   g_vehicle->getObjectID(), center->getObjectID());
    check.data.open(EDO_WRITE).putInt(missionIndex).close();
    context->sendEventNow(check);
    if (player.m_mission[missionIndex].m_status != MISSION_SUCCESS)
    {
        SetError("RecruitCenter terminal real reached check did not succeed");
        return false;
    }
    summary->reachedConditions = 1;
    summary->statusTransitions = 1;

    const bool artifactExisted = context->isExist("Artifact") != 0;
    const KR_ObjectID artifactBefore = artifactExisted
        ? context->searchObject("Artifact") : KR_ObjectID::NUL();
    const int presentationsBefore = g_missionResultPresentations;
    g_vehicle->m_damage = 0.1;
    g_vehicle->m_secBulletCnt = 0;
    MissionVisitResult result;
    if (!ProcessMissionVisit(context, timeStamp + 0.1, center, &player,
                             &result) || !result.success ||
        result.rewardCreated)
    {
        if (g_lastError[0] == 0)
            SetError("RecruitCenter terminal no-reward revisit did not commit");
        return false;
    }
    summary->completedMissions = 1;
    summary->resultPresentations =
        g_missionResultPresentations - presentationsBefore;
    summary->rewardsCreated = result.rewardCreated ? 1 : 0;
    const bool artifactExistsAfter = context->isExist("Artifact") != 0;
    summary->rewardAbsent = artifactExistsAfter == artifactExisted &&
        (!artifactExistsAfter ||
         context->searchObject("Artifact") == artifactBefore) ? 1 : 0;
    mp_Project *completedProjectState =
        projectTable.searchProject(completedProject);
    summary->completedProjectRetired = completedProjectState != NULL &&
        completedProjectState->m_treeNode < 0 ? 1 : 0;
    summary->repaired = g_vehicle->m_damage == 1.0 ? 1 : 0;
    summary->refilled =
        g_vehicle->m_secBulletCnt >= g_levelAttr.m_maxSecBulletCnt ? 1 : 0;
    summary->missionsAfter = player.m_missCnt;
    summary->totalMissionsAfter = player.m_total_misCount;
    summary->scheduledChecksAfter = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);

    KR_ObjectID next = KR_ObjectID::NUL();
    if (!FindCenterCandidate(center, &player, &next))
    {
        SetError("RecruitCenter terminal next-project graph is malformed");
        return false;
    }
    summary->noNextCandidate = next.isNUL() ? 1 : 0;
    MissionVisitResult repeat;
    const bool repeated = ProcessMissionVisit(
        context, timeStamp + 0.2, center, &player, &repeat);
    summary->repeatIdempotent = repeated && !repeat.found &&
        player.m_missCnt == summary->missionsAfter &&
        (context->isExist("Artifact") != 0) == artifactExisted ? 1 : 0;

    const bool exact = summary->reachedConditions == 1 &&
        summary->statusTransitions == 1 &&
        summary->completedMissions == 1 &&
        summary->resultPresentations == 1 &&
        summary->rewardsCreated == 0 && summary->rewardAbsent == 1 &&
        summary->completedProjectRetired == 1 &&
        summary->repaired == 1 && summary->refilled == 1 &&
        summary->noNextCandidate == 1 && summary->repeatIdempotent == 1 &&
        summary->missionsAfter == summary->missionsBefore - 1 &&
        summary->totalMissionsAfter == summary->totalMissionsBefore &&
        summary->scheduledChecksAfter == 0;
    if (!exact)
        SetError("RecruitCenter terminal no-reward invariants did not hold");
    return exact;
}

bool RecruitCenterSubjectState_TerminalNoRewardStateProbeForCenter(
    SimulationContext *context, const char *centerName,
    const char *completedProjectName,
    RecruitCenterMissionTerminalNoRewardStateSummary *summary)
{
    g_lastError[0] = 0;
    if (summary == NULL || context == NULL || centerName == NULL ||
        centerName[0] == 0 || completedProjectName == NULL ||
        completedProjectName[0] == 0 || g_vehicle == NULL ||
        g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter terminal state arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL || !context->isExist(completedProjectName))
    {
        SetError("RecruitCenter terminal state lost its authored owners");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->completedProjectName,
                  sizeof(summary->completedProjectName), "%s",
                  completedProjectName);
    Player &player = static_cast<Player &>(g_vehicle->player());
    bool missionAbsent = true;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID())
            missionAbsent = false;
    summary->missionAbsent = missionAbsent ? 1 : 0;
    const KR_ObjectID completedProject =
        context->searchObject(completedProjectName);
    mp_Project *project = projectTable.searchProject(completedProject);
    summary->projectRetired = project != NULL && project->m_treeNode < 0 ? 1 : 0;
    KR_ObjectID next = KR_ObjectID::NUL();
    if (!FindCenterCandidate(center, &player, &next))
    {
        SetError("RecruitCenter terminal state project graph is malformed");
        return false;
    }
    summary->noNextCandidate = next.isNUL() ? 1 : 0;
    summary->scheduledChecks = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);
    summary->rewardDetached = g_vehicle->m_artefact == NULL &&
        g_vehicle->m_artefactID.isNUL() ? 1 : 0;
    const bool exact = summary->missionAbsent == 1 &&
        summary->projectRetired == 1 && summary->noNextCandidate == 1 &&
        summary->scheduledChecks == 0 && summary->rewardDetached == 1;
    if (!exact)
        SetError("RecruitCenter terminal state invariants did not hold");
    return exact;
}

bool RecruitCenterSubjectState_NoRewardProgressionStateProbeForCenter(
    SimulationContext *context, const char *centerName,
    const char *completedProjectName, const char *nextProjectName,
    RecruitCenterMissionNoRewardProgressionStateSummary *summary)
{
    g_lastError[0] = 0;
    if (summary == NULL || context == NULL || centerName == NULL ||
        centerName[0] == 0 || completedProjectName == NULL ||
        completedProjectName[0] == 0 || nextProjectName == NULL ||
        nextProjectName[0] == 0 || g_vehicle == NULL ||
        g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter no-reward progression arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL || !context->isExist(completedProjectName) ||
        !context->isExist(nextProjectName))
    {
        SetError("RecruitCenter no-reward progression lost authored owners");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->completedProjectName,
                  sizeof(summary->completedProjectName), "%s",
                  completedProjectName);
    std::snprintf(summary->nextProjectName,
                  sizeof(summary->nextProjectName), "%s", nextProjectName);
    Player &player = static_cast<Player &>(g_vehicle->player());
    bool missionAbsent = true;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID())
            missionAbsent = false;
    summary->missionAbsent = missionAbsent ? 1 : 0;
    const KR_ObjectID completed = context->searchObject(completedProjectName);
    mp_Project *project = projectTable.searchProject(completed);
    summary->projectRetired =
        project != NULL && project->m_treeNode < 0 ? 1 : 0;
    KR_ObjectID next = KR_ObjectID::NUL();
    if (!FindCenterCandidate(center, &player, &next))
    {
        SetError("RecruitCenter no-reward progression graph is malformed");
        return false;
    }
    const char *candidateName = next.isNUL()
        ? NULL : context->searchObject(next);
    summary->nextCandidateExact = candidateName != NULL &&
        std::strcmp(candidateName, nextProjectName) == 0 ? 1 : 0;
    summary->scheduledChecks = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);
    summary->rewardDetached = g_vehicle->m_artefact == NULL &&
        g_vehicle->m_artefactID.isNUL() ? 1 : 0;
    const bool exact = summary->missionAbsent == 1 &&
        summary->projectRetired == 1 && summary->nextCandidateExact == 1 &&
        summary->scheduledChecks == 0 && summary->rewardDetached == 1;
    if (!exact)
        SetError("RecruitCenter no-reward progression invariants did not hold");
    return exact;
}

bool RecruitCenterSubjectState_RewardCarrierState(
    SimulationContext *context, bool expectAttached)
{
    g_lastError[0] = 0;
    const bool matches = RewardCarrierState(context, expectAttached);
    if (!matches)
        SetError(expectAttached
                     ? "RecruitCenter reward carrier link is not restored"
                     : "RecruitCenter reward carrier link did not detach");
    return matches;
}

bool RecruitCenterSubjectState_DropRewardProbe(
    SimulationContext *context, double timeStamp,
    RecruitCenterMissionResultProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (context == NULL || summary == NULL || !std::isfinite(timeStamp) ||
        g_vehicle == NULL || g_vehicle->getContext() != context ||
        !RewardCarrierState(context, true))
    {
        SetError("RecruitCenter reward drop probe has no carried Artifact");
        return false;
    }
    const KR_ObjectID reward = context->searchObject("Artifact");
    IDynamicObject *dynamic = static_cast<IDynamicObject *>(
        context->queryInterface(reward, IDynamicObjectIID));
    if (dynamic == NULL)
    {
        SetError("RecruitCenter reward drop probe has no dynamic Artifact");
        return false;
    }
    CFMatrix3x4 vehicleMatrix;
    g_vehicle->getMatrix(vehicleMatrix);
    const CFVector3 forward = -vehicleMatrix.Column(2);
    const CFVector3 expectedDirection = forward * 7.0;
    const CFVector3 expectedPosition = g_vehicle->getPosition() +
        forward * (g_vehicle->getRadius0() + 1.5);
    KR_Event drop(CTRL_BUTTONS_MSG, timeStamp,
                  g_vehicle->getObjectID(), g_vehicle->getObjectID());
    drop.data.open(EDO_WRITE)
        .putInt(DROP_ARTEFACT)
        .putDouble(1.0)
        .putInt(0)
        .putInt(0)
        .close();
    summary->dropInputAccepted = g_vehicle->receiveEvent(drop) == 1 ? 1 : 0;
    summary->bidirectionalDetach = RewardCarrierState(context, false) ? 1 : 0;
    KR_Event moveEvents[2];
    const int moveCount = context->copyEvents(
        ARTEFACT_MOVE, reward, moveEvents, 2);
    summary->dropMoveEventScheduled = moveCount == 1 &&
        moveEvents[0].source == reward && moveEvents[0].destination == reward &&
        moveEvents[0].timeStamp > timeStamp ? 1 : 0;
    summary->dropMotionMatched =
        SameVector(dynamic->getPos(), expectedPosition) &&
        SameVector(dynamic->getMoveDir(), Normal(expectedDirection)) &&
        std::fabs(dynamic->getMoveSpeed() - 7.0) <= 1.0e-7 ? 1 : 0;
    const bool exact = summary->dropInputAccepted == 1 &&
        summary->bidirectionalDetach == 1 &&
        summary->dropMoveEventScheduled == 1 &&
        summary->dropMotionMatched == 1;
    if (!exact) SetError("RecruitCenter reward drop invariants did not hold");
    return exact;
}

static bool StageMissionExecutionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    bool presentBriefing, bool *staged,
    RecruitCenterMissionProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (staged == NULL || summary == NULL) return false;
    *staged = false;
    std::memset(summary, 0, sizeof(*summary));
    if (!RecruitCenterSubjectState_TableReady(context)) return true;
    if (g_vehicle == NULL || g_vehicle->getContext() != context ||
        !std::isfinite(timeStamp) || timeStamp < 0.0)
    {
        SetError("RecruitCenter execution probe has no live Player vehicle");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    if (player.m_missCnt < 0 || player.m_missCnt >= 6 ||
        context->eventFreeCount() < 1)
    {
        SetError("RecruitCenter execution probe needs a stable mission slot");
        return false;
    }
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        RecruitCenter *center = static_cast<RecruitCenter *>(subject);
        const char *name = context->searchObject(center->getObjectID());
        if (centerName != NULL && centerName[0] != 0 &&
            (name == NULL || std::strcmp(name, centerName) != 0))
            continue;
        KR_ObjectID candidate = KR_ObjectID::NUL();
        if (!FindCenterCandidate(center, &player, &candidate))
        {
            SetError("RecruitCenter execution eligibility graph is malformed");
            return false;
        }
        if (candidate.isNUL()) continue;
        if (presentBriefing)
        {
            const int missionsBefore = player.m_missCnt;
            const int collisionsBefore = center->playerCollisions();
            const int admissionsBefore = center->admissions();
            const int stagedBefore = center->stagedMissions();
            const int ejectionsBefore = center->ejections();
            const int attemptsBefore = center->centerPresentationAttempts();
            const int flicksBefore = center->presentedCenterFlicks();
            const int hostileBefore =
                center->presentedHostilityBriefings();
            const int failuresBefore = center->centerPresentationFailures();
            const bool renegade =
                player.isRenegat(center->commanderID()) != 0;
            const KR_ObjectID vehicle = g_vehicle->getObjectID();
            KR_Event collision(t_EV_ONCOLLISION, timeStamp,
                               vehicle, center->getObjectID());
            collision.data.open(EDO_WRITE).putObjectID(vehicle).close();
            context->sendEventNow(collision);
            const RecruitCenterMissionProbeSummary &committed =
                center->lastMissionSummary();
            const bool exact = player.m_missCnt == missionsBefore + 1 &&
                center->playerCollisions() == collisionsBefore + 1 &&
                center->admissions() == admissionsBefore + 1 &&
                center->stagedMissions() == stagedBefore + 1 &&
                center->ejections() == ejectionsBefore + 1 &&
                center->centerPresentationAttempts() == attemptsBefore + 1 &&
                center->presentedCenterFlicks() ==
                    flicksBefore + (renegade ? 0 : 1) &&
                center->presentedHostilityBriefings() ==
                    hostileBefore + (renegade ? 1 : 0) &&
                center->centerPresentationFailures() == failuresBefore &&
                committed.stagedMissions == 1 &&
                committed.centerPresentationAttempts == 1 &&
                committed.presentedCenterFlicks == (renegade ? 0 : 1) &&
                committed.presentedHostilityBriefings == (renegade ? 1 : 0) &&
                committed.centerPresentationFailures == 0;
            if (!exact)
            {
                SetError("RecruitCenter collision presentation did not commit");
                return false;
            }
            *summary = committed;
            *staged = true;
            return true;
        }
        return StageMissionForCenter(context, timeStamp, center, true,
                                     presentBriefing, NULL, staged, summary);
    }
    if (centerName != NULL && centerName[0] != 0)
    {
        char message[256] = {};
        std::snprintf(message, sizeof(message),
                      "RecruitCenter execution probe cannot find %.160s",
                      centerName);
        SetError(message);
        return false;
    }
    return true;
}

bool RecruitCenterSubjectState_StageMissionExecutionProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary)
{
    return StageMissionExecutionProbeForCenter(context, timeStamp, NULL, false,
                                               staged, summary);
}

bool RecruitCenterSubjectState_StageMissionExecutionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    bool *staged, RecruitCenterMissionProbeSummary *summary)
{
    if (centerName == NULL || centerName[0] == 0)
    {
        SetError("RecruitCenter execution probe needs a center name");
        return false;
    }
    return StageMissionExecutionProbeForCenter(context, timeStamp, centerName,
                                               false, staged, summary);
}

bool RecruitCenterSubjectState_StageMissionExecutionProbeForProject(
    SimulationContext *context, double timeStamp, const char *centerName,
    const char *projectName, bool *staged,
    RecruitCenterMissionProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (context == NULL || centerName == NULL || centerName[0] == 0 ||
        projectName == NULL || projectName[0] == 0 || staged == NULL ||
        summary == NULL || !std::isfinite(timeStamp) || timeStamp < 0.0 ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter project probe arguments are invalid");
        return false;
    }
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL)
    {
        SetError("RecruitCenter project probe cannot find the requested center");
        return false;
    }
    return StageMissionForCenter(context, timeStamp, center, true, false,
                                 projectName, staged, summary);
}

bool RecruitCenterSubjectState_EjectPlayerForCenter(
    SimulationContext *context, double timeStamp, const char *centerName)
{
    g_lastError[0] = 0;
    if (context == NULL || centerName == NULL || centerName[0] == 0 ||
        !std::isfinite(timeStamp) || timeStamp < 0.0 ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter ejection probe needs a live Player vehicle");
        return false;
    }
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        RecruitCenter *center = static_cast<RecruitCenter *>(subject);
        const char *name = context->searchObject(center->getObjectID());
        if (name == NULL || std::strcmp(name, centerName) != 0)
            continue;
        const int admissions = center->admissions();
        const int existing = center->existingMissionVisits();
        const int ejections = center->ejections();
        KR_Event event(rc_NEW_MISSION, timeStamp,
                       g_vehicle->getObjectID(), center->getObjectID());
        context->sendEventNow(event);
        const CFVector3 target = center->ejectPosition();
        const CFVector3 vessel = g_vehicle->Pos();
        const CFVector3 subjectPosition = g_vehicle->getPosition();
        const bool exact = FiniteVector(target) && FiniteVector(vessel) &&
            FiniteVector(subjectPosition) &&
            Abs2(vessel - target) <= 1.0e-8 &&
            Abs2(subjectPosition - target) <= 1.0e-8;
        if (center->admissions() != admissions + 1 ||
            center->existingMissionVisits() != existing + 1 ||
            center->ejections() != ejections + 1 || !exact)
        {
            SetError("RecruitCenter ejection did not reach its authored point");
            return false;
        }
        return true;
    }
    SetError("RecruitCenter ejection cannot find the selected center");
    return false;
}

bool RecruitCenterSubjectState_StageMissionPresentationProbe(
    SimulationContext *context, double timeStamp, bool *staged,
    RecruitCenterMissionProbeSummary *summary)
{
    return StageMissionExecutionProbeForCenter(context, timeStamp, NULL, true,
                                               staged, summary);
}

bool RecruitCenterSubjectState_StageMissionPresentationProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    bool *staged, RecruitCenterMissionProbeSummary *summary)
{
    if (centerName == NULL || centerName[0] == 0)
    {
        SetError("RecruitCenter presentation probe needs a center name");
        return false;
    }
    return StageMissionExecutionProbeForCenter(context, timeStamp, centerName,
                                               true, staged, summary);
}

bool RecruitCenterSubjectState_LastMissionSummary(
    RecruitCenterMissionProbeSummary *summary)
{
    if (summary == NULL || !g_hasLastMissionSummary)
        return false;
    *summary = g_lastMissionSummary;
    return true;
}

bool RecruitCenterSubjectState_ObjectiveState(
    SimulationContext *context, RecruitCenterObjectiveStateSummary *summary)
{
    g_lastError[0] = 0;
    if (context == NULL || summary == NULL || g_vehicle == NULL ||
        g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter objective state needs a live Player vehicle");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    Player &player = static_cast<Player &>(g_vehicle->player());
    if (player.m_missCnt < 0 || player.m_missCnt > 6 ||
        player.m_total_misCount < player.m_missCnt)
    {
        SetError("RecruitCenter objective state has invalid Player counters");
        return false;
    }

    summary->missions = player.m_missCnt;
    summary->totalMissions = player.m_total_misCount;
    std::vector<KR_ObjectID> projects;
    std::vector<KR_ObjectID> commanders;
    const KR_SetOfID *sets[6];
    for (int missionIndex = 0; missionIndex < player.m_missCnt;
         ++missionIndex)
    {
        PlayerMission &mission = player.m_mission[missionIndex];
        switch (mission.m_status)
        {
        case MISSION_INPROCESS: ++summary->inProcessMissions; break;
        case MISSION_SUCCESS: ++summary->successMissions; break;
        case MISSION_FAILED: ++summary->failedMissions; break;
        case MISSION_SURRENDER: ++summary->surrenderMissions; break;
        default: break;
        }
        if (mission.m_missionInfoExist != 0) ++summary->summaryMissions;
        if (!mission.m_missionRouteID.isNUL()) ++summary->routeMissions;
        sets[0] = &mission.success_needKill;
        sets[1] = &mission.success_needLive;
        sets[2] = &mission.success_needReached;
        sets[3] = &mission.filed_needKill;
        sets[4] = &mission.filed_needLive;
        sets[5] = &mission.filed_needReached;
        for (int setIndex = 0; setIndex < 6; ++setIndex)
            for (int reference = 0; reference < sets[setIndex]->getCount();
                 ++reference)
            {
                ++summary->conditionReferences;
                KR_ObjectID object = (*sets[setIndex])[reference];
                if (!object.isNUL() && context->isExist(object))
                    ++summary->boundConditionReferences;
            }
        bool knownProject = false;
        for (std::size_t index = 0; index < projects.size(); ++index)
            if (projects[index] == mission.mID) knownProject = true;
        if (!knownProject) projects.push_back(mission.mID);
        bool knownCommander = false;
        for (std::size_t index = 0; index < commanders.size(); ++index)
            if (commanders[index] == mission.comID) knownCommander = true;
        if (!knownCommander) commanders.push_back(mission.comID);

        const char *projectName = context->searchObject(mission.mID);
        if (projectName == NULL)
        {
            SetError("RecruitCenter objective state lost a project name");
            return false;
        }
        char *destination = missionIndex == 0 ? summary->firstProjectName :
                            missionIndex == 1 ? summary->secondProjectName :
                            NULL;
        if (destination != NULL)
            std::snprintf(destination, 81, "%s", projectName);
        if (mission.m_missionInfoExist != 0 &&
            g_debugMap.MissionInUse(mission.m_TMissionId) &&
            g_debugMap.MissionHasText(mission.m_TMissionId) &&
            (mission.m_missionRouteID.isNUL() ||
             g_debugMap.MissionRouteCount(mission.m_TMissionId) > 0))
            ++summary->mapBindings;
    }
    summary->distinctProjects = static_cast<int>(projects.size());
    summary->distinctCommanders = static_cast<int>(commanders.size());
    summary->mapMissions = g_debugMap.MissionCount();
    summary->mapTexts = g_debugMap.MissionTextCount();
    summary->mapRoutes = g_debugMap.MissionRouteCount();
    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        const int checks = context->copyEventsTo(
            rc_CHECK_MISSION, subject->getObjectID(), NULL, 0);
        if (checks < 0 || checks > 6)
        {
            SetError("RecruitCenter objective state has invalid check events");
            return false;
        }
        summary->scheduledChecks += checks;
    }
    return true;
}

bool RecruitCenterSubjectState_FailMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionTerminalProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (context == NULL || centerName == NULL || centerName[0] == 0 ||
        summary == NULL || !std::isfinite(timeStamp) || timeStamp < 0.0 ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter failure probe arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL)
    {
        SetError("RecruitCenter failure probe cannot find the requested center");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    int missionIndex = -1;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID() &&
            player.m_mission[index].m_status == MISSION_INPROCESS)
        {
            missionIndex = index;
            break;
        }
    if (missionIndex < 0)
    {
        SetError("RecruitCenter failure probe has no active center mission");
        return false;
    }
    PlayerMission &mission = player.m_mission[missionIndex];
    if (mission.filed_needReached.getCount() <= 0 &&
        mission.filed_needKill.getCount() <= 0)
    {
        SetError("RecruitCenter failure probe needs an authored failure condition");
        return false;
    }
    RecruitCenterObjectiveStateSummary before = {};
    if (!RecruitCenterSubjectState_ObjectiveState(context, &before))
        return false;
    const char *projectName = context->searchObject(mission.mID);
    if (projectName == NULL)
    {
        SetError("RecruitCenter failure probe lost its project name");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->projectName, sizeof(summary->projectName), "%s",
                  projectName);
    summary->missionsBefore = before.missions;
    summary->totalMissionsBefore = before.totalMissions;
    summary->scheduledChecksBefore = before.scheduledChecks;
    summary->mapBindingsBefore = before.mapBindings;

    if (mission.filed_needReached.getCount() > 0)
    {
        IDynamicObject *target = static_cast<IDynamicObject *>(
            context->queryInterface(mission.filed_needReached[0],
                                    IDynamicObjectIID));
        if (target == NULL)
        {
            SetError("RecruitCenter failure target has no dynamic interface");
            return false;
        }
        const CFVector2 goal = mission.filed_reachedPos[0];
        const CFVector3 original = target->getPos();
        CFMatrix3x4 atFailure;
        atFailure.LoadIdentity().TranslateL(
            CFVector3(goal.x, original.y, goal.y));
        target->SetDir(atFailure);
    }
    else
    {
        std::vector<KR_ObjectID> targets;
        for (int index = 0; index < mission.filed_needKill.getCount(); ++index)
            if (context->isExist(mission.filed_needKill[index]))
                targets.push_back(mission.filed_needKill[index]);
        if (targets.empty())
        {
            SetError("RecruitCenter failure kill targets are already absent");
            return false;
        }
        for (std::size_t index = 0; index < targets.size(); ++index)
            if (context->isExist(targets[index]))
                context->removeObject(targets[index]);
    }
    context->removeEventsTo(rc_CHECK_MISSION, center->getObjectID());
    const int presentationsBefore = g_missionStatusPresentations;
    KR_Event check(rc_CHECK_MISSION, timeStamp,
                   g_vehicle->getObjectID(), center->getObjectID());
    check.data.open(EDO_WRITE).putInt(missionIndex).close();
    context->sendEventNow(check);
    summary->commandAccepted = 1;

    RecruitCenterObjectiveStateSummary after = {};
    if (!RecruitCenterSubjectState_ObjectiveState(context, &after))
        return false;
    summary->statusPresentations =
        g_missionStatusPresentations - presentationsBefore;
    summary->statusTransitions = after.failedMissions - before.failedMissions;
    summary->failedMissions = after.failedMissions;
    summary->surrenderedMissions = after.surrenderMissions;
    summary->missionsAfter = after.missions;
    summary->totalMissionsAfter = after.totalMissions;
    summary->scheduledChecksAfter = after.scheduledChecks;
    summary->mapBindingsAfter = after.mapBindings;
    summary->checkGraphExact = MissionCheckGraphExact(context, &player) ? 1 : 0;
    const bool exact = summary->commandAccepted == 1 &&
        summary->statusPresentations == 1 &&
        summary->statusTransitions == 1 &&
        after.failedMissions == before.failedMissions + 1 &&
        after.inProcessMissions == before.inProcessMissions - 1 &&
        after.surrenderMissions == before.surrenderMissions &&
        after.missions == before.missions &&
        after.totalMissions == before.totalMissions &&
        after.scheduledChecks == before.scheduledChecks - 1 &&
        after.mapBindings == before.mapBindings &&
        summary->checkGraphExact == 1;
    if (!exact)
        SetError("RecruitCenter authored failure transition diverged");
    return exact;
}

bool RecruitCenterSubjectState_SurrenderMissionProbe(
    SimulationContext *context, double timeStamp,
    RecruitCenterMissionTerminalProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (context == NULL || summary == NULL || !std::isfinite(timeStamp) ||
        timeStamp < 0.0 || g_vehicle == NULL ||
        g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter surrender probe arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenterObjectiveStateSummary before = {};
    if (!RecruitCenterSubjectState_ObjectiveState(context, &before) ||
        before.missions <= 0 || before.inProcessMissions != before.missions)
    {
        if (g_lastError[0] == 0)
            SetError("RecruitCenter surrender probe needs active missions");
        return false;
    }
    summary->missionsBefore = before.missions;
    summary->totalMissionsBefore = before.totalMissions;
    summary->scheduledChecksBefore = before.scheduledChecks;
    summary->mapBindingsBefore = before.mapBindings;
    std::snprintf(summary->projectName, sizeof(summary->projectName), "%s",
                  before.firstProjectName);
    const bool presentationReady = g_GameConsole.MessagesReady();
    KR_Event surrender(EV_VEHICLE_SURRENDER, timeStamp,
                       g_vehicle->getObjectID(), g_vehicle->getObjectID());
    summary->commandAccepted = g_vehicle->receiveEvent(surrender) == 1 ? 1 : 0;
    summary->statusPresentations =
        summary->commandAccepted && presentationReady ? 1 : 0;

    RecruitCenterObjectiveStateSummary after = {};
    if (!RecruitCenterSubjectState_ObjectiveState(context, &after))
        return false;
    summary->statusTransitions =
        after.surrenderMissions - before.surrenderMissions;
    summary->failedMissions = after.failedMissions;
    summary->surrenderedMissions = after.surrenderMissions;
    summary->missionsAfter = after.missions;
    summary->totalMissionsAfter = after.totalMissions;
    summary->scheduledChecksAfter = after.scheduledChecks;
    summary->mapBindingsAfter = after.mapBindings;
    summary->checkGraphExact = MissionCheckGraphExact(context,
                                                       &static_cast<Player &>(
                                                           g_vehicle->player()))
        ? 1 : 0;
    const bool exact = summary->commandAccepted == 1 &&
        summary->statusPresentations == 1 &&
        summary->statusTransitions == before.inProcessMissions &&
        after.inProcessMissions == 0 && after.failedMissions == 0 &&
        after.surrenderMissions == before.missions &&
        after.missions == before.missions &&
        after.totalMissions == before.totalMissions &&
        after.scheduledChecks == before.scheduledChecks &&
        after.mapBindings == before.mapBindings &&
        summary->checkGraphExact == 1;
    if (!exact) SetError("RecruitCenter surrender transition diverged");
    return exact;
}

static bool ResolveTerminalMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    int expectedStatus, RecruitCenterMissionTerminalProbeSummary *summary)
{
    g_lastError[0] = 0;
    if (context == NULL || centerName == NULL || centerName[0] == 0 ||
        summary == NULL || !std::isfinite(timeStamp) || timeStamp < 0.0 ||
        g_vehicle == NULL || g_vehicle->getContext() != context)
    {
        SetError("RecruitCenter terminal result arguments are invalid");
        return false;
    }
    std::memset(summary, 0, sizeof(*summary));
    RecruitCenter *center = FindRecruitCenter(context, centerName);
    if (center == NULL)
    {
        SetError("RecruitCenter terminal result cannot find its center");
        return false;
    }
    Player &player = static_cast<Player &>(g_vehicle->player());
    int missionIndex = -1;
    for (int index = 0; index < player.m_missCnt; ++index)
        if (player.m_mission[index].comID == center->commanderID() &&
            player.m_mission[index].m_status == expectedStatus)
        {
            missionIndex = index;
            break;
        }
    if (missionIndex < 0)
    {
        SetError("RecruitCenter terminal result has no matching mission");
        return false;
    }
    RecruitCenterObjectiveStateSummary before = {};
    if (!RecruitCenterSubjectState_ObjectiveState(context, &before))
        return false;
    const char *projectName = context->searchObject(
        player.m_mission[missionIndex].mID);
    if (projectName == NULL)
    {
        SetError("RecruitCenter terminal result lost its project name");
        return false;
    }
    std::snprintf(summary->centerName, sizeof(summary->centerName), "%s",
                  centerName);
    std::snprintf(summary->projectName, sizeof(summary->projectName), "%s",
                  projectName);
    summary->missionsBefore = before.missions;
    summary->totalMissionsBefore = before.totalMissions;
    summary->scheduledChecksBefore = before.scheduledChecks;
    summary->mapBindingsBefore = before.mapBindings;
    const int centerChecks = context->copyEventsTo(
        rc_CHECK_MISSION, center->getObjectID(), NULL, 0);
    const double damage = g_vehicle->m_damage;
    const int ammunition = g_vehicle->m_secBulletCnt;
    const bool artifactBefore = context->isExist("Artifact");
    const int presentationsBefore = g_missionResultPresentations;
    MissionVisitResult result;
    if (centerChecks < 0 ||
        !ProcessMissionVisit(context, timeStamp, center, &player, &result))
    {
        if (g_lastError[0] == 0)
            SetError("RecruitCenter terminal result did not commit");
        return false;
    }
    RecruitCenterObjectiveStateSummary after = {};
    if (!RecruitCenterSubjectState_ObjectiveState(context, &after))
        return false;
    summary->resultPresentations =
        g_missionResultPresentations - presentationsBefore;
    summary->removedMissions = before.missions - after.missions;
    summary->failedMissions = after.failedMissions;
    summary->surrenderedMissions = after.surrenderMissions;
    summary->missionsAfter = after.missions;
    summary->totalMissionsAfter = after.totalMissions;
    summary->scheduledChecksAfter = after.scheduledChecks;
    summary->mapBindingsAfter = after.mapBindings;
    summary->rewardCreated = result.rewardCreated ? 1 : 0;
    summary->damagePreserved = g_vehicle->m_damage == damage ? 1 : 0;
    summary->ammunitionPreserved =
        g_vehicle->m_secBulletCnt == ammunition ? 1 : 0;
    summary->checkGraphExact = MissionCheckGraphExact(context, &player) ? 1 : 0;
    std::snprintf(summary->survivingProjectName,
                  sizeof(summary->survivingProjectName), "%s",
                  after.firstProjectName);
    MissionVisitResult repeat;
    const bool repeated = ProcessMissionVisit(
        context, timeStamp + 0.1, center, &player, &repeat);
    summary->repeatIdempotent = repeated && !repeat.found &&
        player.m_missCnt == after.missions ? 1 : 0;
    const int expectedFailed = expectedStatus == MISSION_FAILED ? 1 : 0;
    const int expectedSurrender = expectedStatus == MISSION_SURRENDER ? 1 : 0;
    const bool exact = result.found && result.failure && !result.success &&
        !result.repaired && !result.refilled && !result.rewardCreated &&
        summary->resultPresentations == 1 &&
        summary->removedMissions == 1 &&
        before.failedMissions - after.failedMissions == expectedFailed &&
        before.surrenderMissions - after.surrenderMissions ==
            expectedSurrender &&
        after.missions == before.missions - 1 &&
        after.totalMissions == before.totalMissions &&
        after.scheduledChecks == before.scheduledChecks - centerChecks &&
        after.mapBindings == before.mapBindings - 1 &&
        (context->isExist("Artifact") != 0) == artifactBefore &&
        summary->damagePreserved == 1 &&
        summary->ammunitionPreserved == 1 &&
        summary->checkGraphExact == 1 && summary->repeatIdempotent == 1;
    if (!exact)
        SetError("RecruitCenter terminal result invariants did not hold");
    return exact;
}

bool RecruitCenterSubjectState_ResolveFailedMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionTerminalProbeSummary *summary)
{
    return ResolveTerminalMissionProbeForCenter(
        context, timeStamp, centerName, MISSION_FAILED, summary);
}

bool RecruitCenterSubjectState_ResolveSurrenderedMissionProbeForCenter(
    SimulationContext *context, double timeStamp, const char *centerName,
    RecruitCenterMissionTerminalProbeSummary *summary)
{
    return ResolveTerminalMissionProbeForCenter(
        context, timeStamp, centerName, MISSION_SURRENDER, summary);
}

const char *RecruitCenterSubjectState_LastError()
{
    return g_lastError;
}
