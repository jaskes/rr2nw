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
#include "h/vehicle.h"
#include "i/dynobj.i"
#include "i/player.i"
#include "i/route.i"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "message/recrcenmsg.h"
#include "message/unitmsg.h"
#include "mproj/h/mproj.h"
#include "storage/h/subject.h"
#include "../output/defs.h"

namespace {

const int kMaximumCapacity = 16;
const int kMaximumProjectNodes = 1024;
const int kMaximumProjectPayload = 10240;
const double kRadius = 4.0;
const unsigned long long kHashOffset = 14695981039346656037ull;
const unsigned long long kHashPrime = 1099511628211ull;

char g_lastError[256] = {};

void SetError(const char *message)
{
    std::snprintf(g_lastError, sizeof(g_lastError), "%s",
                  message == NULL ? "unknown RecruitCenter failure"
                                  : message);
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
                     bool *eligible)
{
    *matches = true;
    *eligible = true;
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
    if (!ProjectMetadata(object, search->commander,
                         search->player->m_total_misCount,
                         &matches, &eligible))
    {
        search->valid = false;
        return false;
    }
    if (matches && eligible && !HasMission(*search->player, object))
    {
        search->result = object;
        return false;
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

bool ConsumeDeferredCommand(int command, ProjectDataReader *data)
{
    std::string first;
    std::string second;
    std::string third;
    std::string fourth;
    double value = 0.0;
    int integer = 0;
    switch (command)
    {
    case COM_CREATE_UNITS:
        return data->readString(&first, 40) &&
               data->readString(&second, 80) && data->readDouble(&value) &&
               value >= 0.0 && data->readString(&third, 40) &&
               data->readString(&fourth, 40);
    case COM_PLAY_BRIEFING:
    case COM_RUN_SCRIPT:
        return data->readString(&first, 260);
    case COM_SKIP_WAY:
        return data->readDouble(&value) && data->readDouble(&value) &&
               data->readDouble(&value) && data->readDouble(&value);
    case COM_PLAY_BRIEFING_MSG:
        return data->readString(&first, 260) && data->readInt(&integer);
    case 33:
        if (!data->readString(&first, 80)) return false;
        for (int index = 0; index < 5; ++index)
            if (!data->readDouble(&value)) return false;
        return data->readString(&second, 260);
    case 34:
        return data->readDouble(&value) && data->readDouble(&value) &&
               data->readDouble(&value) && data->readInt(&integer) &&
               data->readDouble(&value) && data->readString(&first, 260);
    case 35:
        return true;
    default:
        return false;
    }
}

bool DecodeMission(SimulationContext *context, KR_ObjectID project,
                   KR_ObjectID commander, PlayerMission *mission,
                   std::string *routeName, int *deferredCommands)
{
    InitializeMission(mission, project, commander);
    routeName->clear();
    *deferredCommands = 0;
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
            if (!ConsumeDeferredCommand(command, &data) || !data.finish())
                return false;
            ++*deferredCommands;
            break;
        }
        case COM_BRIEFING_OVER:
            ++*deferredCommands;
            break;
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
            const bool success = EvaluateConditions(&mission, context, true);
            const bool failure = EvaluateConditions(&mission, context, false);
            if (mission.success_filed ? success : failure)
                mission.m_status = MISSION_SUCCESS;
            else if (mission.success_filed ? failure : success)
                mission.m_status = MISSION_FAILED;
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
    CFMatrix3x4 m_direction;
};

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
    if (player.m_missCnt < 0 || player.m_missCnt >= 6 ||
        context->eventFreeCount() < 1)
    {
        SetError("RecruitCenter mission probe has no transactional capacity");
        return false;
    }
    const KR_ObjectID vehicleID = g_vehicle->getObjectID();
    if (context->copyEvents(rc_CHECK_MISSION, vehicleID, NULL, 0) != 0)
    {
        SetError("RecruitCenter mission probe requires a clean check queue");
        return false;
    }

    for (ct_Subject *subject = g_recruitCenterTable.findFirstSubject();
         subject != NULL;
         subject = g_recruitCenterTable.findNextSubject(subject))
    {
        RecruitCenter *center = static_cast<RecruitCenter *>(subject);
        CandidateSearch search = {&player, center->commander(),
                                  KR_ObjectID::NUL(), true};
        projectTable.userFind(FindCandidate, &search);
        if (!search.valid)
        {
            SetError("RecruitCenter project eligibility graph is malformed");
            return false;
        }
        if (search.result.isNUL()) continue;

        PlayerMission mission;
        std::string routeName;
        int deferredCommands = 0;
        if (!DecodeMission(context, search.result, center->commanderID(),
                           &mission, &routeName, &deferredCommands))
        {
            SetError("RecruitCenter authored mission decode failed");
            return false;
        }
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
                SetError("RecruitCenter mission Route allocation failed");
                return false;
            }
            if (routeObject->GetNodeCnt() <= 0)
                routeObject->Load(routeName.c_str());
            if (routeObject->GetNodeCnt() <= 0)
            {
                if (createdRoute && context->isExist(route))
                    context->removeObject(route);
                SetError("RecruitCenter mission Route did not load");
                return false;
            }
            mission.m_missionRouteID = route;
        }

        const int missionIndex = player.addMission(search.result,
                                                   center->commanderID());
        if (missionIndex < 0)
        {
            if (createdRoute && context->isExist(route))
                context->removeObject(route);
            SetError("RecruitCenter Player mission pool is full");
            return false;
        }
        player.m_mission[missionIndex] = mission;
        player.loadNotify();

        KR_Event event(rc_CHECK_MISSION, timeStamp,
                       vehicleID, center->getObjectID());
        event.data.open(EDO_WRITE).putInt(missionIndex).close();
        context->addEvent(event);
        const int queued = context->copyEvents(
            rc_CHECK_MISSION, vehicleID, NULL, 0);
        if (queued != 1)
        {
            while (context->removeEvent(rc_CHECK_MISSION, vehicleID) == 1) {}
            for (int move = missionIndex; move + 1 < player.m_missCnt; ++move)
                player.m_mission[move] = player.m_mission[move + 1];
            --player.m_missCnt;
            if (player.m_total_misCount > 0) --player.m_total_misCount;
            player.loadNotify();
            if (createdRoute && context->isExist(route))
                context->removeObject(route);
            SetError("RecruitCenter mission check event was not queued");
            return false;
        }

        summary->stagedMissions = 1;
        summary->conditionReferences = ConditionCount(mission);
        summary->routeReferences = mission.m_missionRouteID.isNUL() ? 0 : 1;
        summary->deferredCommands = deferredCommands;
        *staged = true;
        return true;
    }
    return true;
}

const char *RecruitCenterSubjectState_LastError()
{
    return g_lastError;
}
