#include "RecoveredArenaSeanceRuntime.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <set>
#include <string>
#include <vector>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "i/unit.i"
#include "obase/artefact/ArtefactAttributeState.h"
#include "obase/bird/BirdAttributeState.h"
#include "obase/bullet/BulletAttributeState.h"
#include "obase/bullet/BulletActiveWorldState.h"
#include "obase/bullet/BulletSubjectState.h"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/corpse/CorpseActiveWorldState.h"
#include "obase/corpse/CorpseSubjectState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/explosion/ExplosionActiveWorldState.h"
#include "obase/explosion/ExplosionSubjectState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/farter/FarterSubjectState.h"
#include "obase/lamp/LampAttributeState.h"
#include "obase/orphan/OrphanAttributeState.h"
#include "obase/orphan/OrphanSubjectState.h"
#include "obase/people/PeopleActiveWorldState.h"
#include "obase/people/PeopleSubjectState.h"
#include "obase/cannon/CannonSubjectState.h"
#include "obase/comander/CommanderState.h"
#include "obase/group/TankGroupState.h"
#include "obase/howitzer/HowitzerSubjectState.h"
#include "obase/tank/TankActiveWorldState.h"
#include "obase/tank/TankSubjectState.h"
#include "obase/portal/PortalClassTableState.h"
#include "obase/recrcen/RecruitCenterSubjectState.h"
#include "obase/teleport/TeleportSubjectState.h"
#include "obase/spark/SparkAttributeState.h"
#include "obase/spark/SparkActiveWorldState.h"
#include "obase/spark/SparkSubjectState.h"
#include "obase/taxi/TaxiAttributeState.h"
#include "obase/taxi/TaxiSubjectState.h"
#include "obase/vehicle/VehicleActiveWorldState.h"
#include "obase/vehicle/VehicleAttributeState.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokeActiveWorldState.h"
#include "obase/smoke/SmokeVisualState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "obase/skin/SkinResourceState.h"
#include "storage/h/subject.h"
#include "message/groupmsg.h"
#include "message/skinmsg.h"
#include "message/unitmsg.h"
#include "mproj/h/mproj.h"
#include "sound.h"

#include "RecoveredLegacyScriptHost.h"
#include "RecoveredLegacyScriptRunner.h"
#include "ActiveWorldSave.h"
#include "ActiveWorldRuntimeProbe.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredGameplayTuningRuntime.h"
#include "RecoveredScriptEventRuntime.h"
#include "RecoveredStaticMechanismRuntime.h"
#include "RecoveredModRuntime.h"
#include "RecoveredRetailScriptManifest.h"
#include "SimulationRandom.h"
#include "RecoveredSkinResourceCatalog.h"
#include "RecoveredWavMetadataCatalog.h"
#include "ZavSceneState.h"

namespace {

constexpr double kSceneWidth = 5120.0;
constexpr double kSceneDepth = 5120.0;
constexpr int kDynSmokerCapacity = 50 + 12;
constexpr int kSmokeCapacity = 300;
constexpr int kSoundObjectCapacity = 250;
constexpr int kFarterSubjectCapacity = 25;
constexpr int kOrphanSubjectCapacity = 5;
constexpr int kCorpseSubjectCapacity = 100;
constexpr int kSparkSubjectCapacity = 40;
constexpr double kDeviceFreeSoundDistance = 300.0;
constexpr int kSourceOnlySmokerAttributeCount = 11;
constexpr const char kCommonBootstrapProgramName[] =
    "recovered_common_attribute_bootstrap";
constexpr const char kSmokeAttributeProgramName[] =
    "recovered_retail_smoke_attribute_bootstrap";
constexpr const char kExplosionAttributeProgramName[] =
    "recovered_retail_explosion_attribute_bootstrap";
constexpr const char kTaxiAttributeProgramName[] =
    "recovered_retail_taxi_attribute_bootstrap";
constexpr const char kTaxiSubjectProgramName[] =
    "recovered_retail_taxi_subject_bootstrap";
constexpr const char kVehicleAttributeProgramName[] =
    "recovered_retail_vehicle_attribute_bootstrap";
constexpr const char kCommanderProgramName[] =
    "recovered_retail_commander_bootstrap";
constexpr const char kMissionTankProgramName[] =
    "recovered_retail_mission_tank_probe";
constexpr const char kFarterAttributeProgramName[] =
    "recovered_retail_farter_attribute_bootstrap";
constexpr const char kFarterSubjectProgramName[] =
    "recovered_retail_farter_subject_bootstrap";
constexpr const char kLampAttributeProgramName[] =
    "recovered_retail_lamp_attribute_bootstrap";
constexpr const char kCorpseAttributeProgramName[] =
    "recovered_retail_corpse_attribute_bootstrap";
constexpr const char kBulletAttributeProgramName[] =
    "recovered_retail_bullet_attribute_bootstrap";
constexpr const char kSmokerAttributeProgramName[] =
    "recovered_retail_smoker_attribute_bootstrap";
constexpr const char kWavMetadataProgramName[] =
    "recovered_retail_wav_metadata_bootstrap";
constexpr const char kRouteProgramName[] =
    "recovered_retail_route_bootstrap";
constexpr const char kPeopleAttributeProgramName[] =
    "recovered_retail_people_attribute_bootstrap";
constexpr const char kPeopleSubjectProgramName[] =
    "recovered_retail_people_subject_bootstrap";
constexpr const char kTankCannonAttributeProgramName[] =
    "recovered_retail_tank_cannon_attribute_bootstrap";
constexpr const char kHowitzerProgramName[] =
    "recovered_retail_howitzer_bootstrap";
constexpr const char kSkinAnimationProgramName[] =
    "recovered_retail_skin_animation_bootstrap";
constexpr const char kMissionProjectProgramName[] =
    "recovered_retail_mission_project_bootstrap";
constexpr const char kRecruitCenterProgramName[] =
    "recovered_retail_recruit_center_bootstrap";

const char kMissionProjectBootstrapPrefix[] = R"RR2NW_SCRIPT(
func void s_CreateProjectTable(int projects, int nodes, int heap) extern;
func int s_NewPNode(int command, int left, int right) extern;
func void s_OpenProjectData(int node) extern;
func void s_CloseProjectData(int node) extern;
func void s_ProjectWriteInt(int node, int value) extern;
func void s_ProjectWriteFloat(int node, float value) extern;
func void s_ProjectWriteStr(int node, str value) extern;
func void s_ProjectNodeSetLink(int node, int left, int right) extern;
func int s_PNodeNULL() extern;
func void s_NewProjectEx(str name, int node, int permanent) extern;
func int s_SearchSeanceClassTable(str tableName) extern;
func int s_SetCommander(str objectName, str commanderName) extern;
func void s_DeferMissionHowitzer(int classTable, str attributeName,
                                 str holderName, float startTime,
                                 str objectName) extern;
func void s_DeferMissionDestroyable(str attributeName, str scriptName,
                                    str objectName) extern;

func void s_NewProject(str name, int node)
{
  s_NewProjectEx(name,node,0);
}

func int ConvertColor(int r, int g, int b)
{
  if r > 255 then r := 255; else if r < 0 then r := 0;
  if g > 255 then g := 255; else if g < 0 then g := 0;
  if b > 255 then b := 255; else if b < 0 then b := 0;
  return r*65536+g*256+b;
}

func void CreateHowitzerName(int classTable, str attributeName,
                             str holderName, float startTime,
                             str objectName)
{
  s_DeferMissionHowitzer(classTable,attributeName,holderName,startTime,
                         objectName);
}

func void CreateDestroyable(int classTable, vector position, float angle,
                            str attributeName, int mission,
                            str scriptName, str objectName)
{
  s_DeferMissionDestroyable(attributeName,scriptName,objectName);
}
)RR2NW_SCRIPT";

const char kMissionProjectBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  s_CreateProjectTable(200,1024,1024*10);
  CreateTestProject();
}
)RR2NW_SCRIPT";

// This deliberately uses the original script-facing storage and event
// protocol for the small common Bird/Portal/Orphan/Artefact/Spark/Route slice.
// Exact Vehicle tables now come from the selected retail VEHICLE.SCI. The
// complete retail LEVEL0.SC will replace this bridge as the remaining OBASE
// class-table archives are connected.
const char kCommonAttributeBootstrapScript[] = R"RR2NW_SCRIPT(const int EDO_WRITE = 1;
const int KR_SET_ATTR = 1;
const int sp_EV_SET_PHASE_COUNT extern;
const int sp_EV_SET_PHASE extern;
const int RECT2D_I extern;
const int LIGHT_COLOR_YELLOW extern;
const int s_ATTR_MSG_SET_INT extern;
const int s_ATTR_MSG_SET_DOUBLE extern;
const int s_ATTR_MSG_SET_STR extern;
const int s_GROUP_ADD_MEMBER_N extern;
const int s_COMMANDER_ADD_MEMBER_N extern;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_Descend(int event, int tag, int index) extern;
func void s_Ascend(int event) extern;
func void s_WriteInt(int event, int value) extern;
func void s_WriteFloat(int event, float value) extern;
func void s_WriteStr(int event, str value) extern;
func void s_WriteObjectID(int event, int objectID, int cachePos) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func int s_SearchSeanceClassTable(str tableName) extern;
func int s_AddClassTable(str className, int maxTableSize) extern;
func void s_New(int classTableID, str name, var int objectID, var int cachePos) extern;
func void s_NewObject(int classTableID, str name) extern;

func void SetSparkPhase(int objectID, int cachePos,
                        int phase, int x, int y, int w, int h, float time,
                        int brightness, int color, float radius)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, phase);
  s_Descend(event, RECT2D_I, 0);
  s_WriteInt(event, x-w);
  s_WriteInt(event, y-h);
  s_WriteInt(event, x+w);
  s_WriteInt(event, y+h);
  s_Ascend(event);
  s_WriteFloat(event, time);
  s_WriteInt(event, brightness);
  s_WriteInt(event, color);
  s_WriteFloat(event, radius);
  s_CloseEventData(event);
  s_SendEventNow(event, sp_EV_SET_PHASE, objectID, cachePos);
}

func void SetSpark0()
var int objectID, cachePos, event;
{
  s_SearchObjectID(objectID, cachePos, "Spark.Flash");
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event, 6);
  s_CloseEventData(event);
  s_SendEventNow(event, sp_EV_SET_PHASE_COUNT, objectID, cachePos);

  SetSparkPhase(objectID,cachePos,0, 25,45,20,40,0.04,100,LIGHT_COLOR_YELLOW, 7);
  SetSparkPhase(objectID,cachePos,1, 78,46,20,40,0.03,200,LIGHT_COLOR_YELLOW,14);
  SetSparkPhase(objectID,cachePos,2,126,47,20,40,0.03,100,LIGHT_COLOR_YELLOW,10);
  SetSparkPhase(objectID,cachePos,3,180,44,20,40,0.10, 20,LIGHT_COLOR_YELLOW, 7);
  SetSparkPhase(objectID,cachePos,4,230,44,20,40,0.10,  0,LIGHT_COLOR_YELLOW, 4);
  SetSparkPhase(objectID,cachePos,5,230,44,20,40,0.00,  0,LIGHT_COLOR_YELLOW, 0);
}

func void SetAttributeFloat(int objectID, int cachePos, str name, float value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteFloat(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_DOUBLE, objectID, cachePos);
}

func void SetAttributeInt(int objectID, int cachePos, str name, int value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteInt(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_INT, objectID, cachePos);
}

func void SetAttributeStr(int objectID, int cachePos, str name, str value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteStr(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_STR, objectID, cachePos);
}

func void ChangeObjectAttrN(str attrName, str objectName)
var int event, attrID, attrCachePos, objectID, cachePos;
{
  s_SearchObjectID(attrID, attrCachePos, attrName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteObjectID(event, attrID, attrCachePos);
  s_CloseEventData(event);
  s_SearchObjectID(objectID, cachePos, objectName);
  s_SendEventNow(event, KR_SET_ATTR, objectID, cachePos);
}

func void main()
var int sparkAttrTable, routeTable, birdAttrTable, portalTable;
var int orphanAttrTable, artefactAttrTable, objectID, cachePos;
{
  sparkAttrTable := s_AddClassTable("SparkAttr", 3);
  birdAttrTable := s_AddClassTable("BirdAttr", 1);
  s_New(birdAttrTable, "Bird.Attr.0", objectID, cachePos);
  SetAttributeFloat(objectID, cachePos, "m_speed", 2.0);
  SetAttributeStr(objectID, cachePos, "m_skinName", "sk.Bird.0");
  SetAttributeFloat(objectID, cachePos, "m_calcPosIncrement", 0.2);

  portalTable := s_AddClassTable("Portal", 2);

  s_NewObject(sparkAttrTable, "Spark.Flash");
  SetSpark0();

  orphanAttrTable := s_AddClassTable("OrphanAttr", 3);
  s_New(orphanAttrTable, "Orphan.Attr.Default", objectID, cachePos);

  artefactAttrTable := s_AddClassTable("ArtefactAttr", 2);
  s_New(artefactAttrTable, "Artefact.Attr.0", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_skinName", "sk.Artefact.0");
  SetAttributeFloat(objectID, cachePos, "m_maxCoronaR", 20.0);
  SetAttributeFloat(objectID, cachePos, "m_coronaR", 0.4);
  SetAttributeInt(objectID, cachePos, "m_coronaRGB", 16711935);
  SetAttributeInt(objectID, cachePos, "m_coronaAlpha", 150);

}
)RR2NW_SCRIPT";

const char kRetailAttributeBootstrapPrefix[] = R"RR2NW_SCRIPT(
const int EDO_WRITE = 1;
const int KR_SET_ATTR = 1;
const int NO_LAND = 0;
const int ON_LAND = 1;
const int ON_WATER = 2;
const int ON_OBJECTS = 3;
const float INFINITY_TIME = -1;
const int LIGHT_COLOR_RED = 1;
const int LIGHT_COLOR_GREEN = 2;
const int LIGHT_COLOR_YELLOW = 3;
const int LIGHT_COLOR_BLUE = 4;
const int LIGHT_COLOR_VIOLET = 5;
const int LIGHT_COLOR_CYAN = 6;
const int LIGHT_COLOR_WHITE = 7;
const int s_ATTR_MSG_SET_INT extern;
const int s_ATTR_MSG_SET_DOUBLE extern;
const int s_ATTR_MSG_SET_STR extern;
const int s_GROUP_ADD_MEMBER_N extern;
const int s_COMMANDER_ADD_MEMBER_N extern;
const int fou_EVCMD_START extern;
const int START_FARTING extern;
const int lmp_EV_START extern;
const int lmp_EV_SETENDPOS extern;
const int taxi_SET_TO_POS extern;
  const int pe_EVCMD_START extern;
  const int pe_EVCMD_START_EX extern;
  const int sk_EV_PROG extern;
const int t_EV_SET_ATTR_POS extern;
const int rc_SET_EJECT extern;
const int rc_SET_VIDEO extern;
const int rc_SET_DEFTAXI extern;
const int rc_SET_DICTIONARY extern;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_WriteInt(int event, int value) extern;
func void s_WriteFloat(int event, float value) extern;
func void s_WriteStr(int event, str value) extern;
func void s_WriteObjectID(int event, int objectID, int cachePos) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func void s_SearchObjectIDNoWarning(var int objectID, var int cachePos,
                                    str name) extern;
func void s_RemoveObject(str objectName, float from) extern;
func void s_ForceRemoveObject(str objectName) extern;
func void s_DeleteHowitzer(str holderName) extern;
func void s_IssueEvent(int event, int label, float time,
                       int objectID, int cachePos) extern;
func int s_AddClassTable(str className, int maxTableSize) extern;
func void s_New(int classTableID, str name,
                var int objectID, var int cachePos) extern;
func void s_NewObject(int classTableID, str name) extern;
func void s_NewObjectN(str className, str name) extern;
func void s_LoadRoute(int classTableID, str fileName, str routeName) extern;
func int s_SearchSeanceClassTable(str tableName) extern;
func int s_SetCommander(str objectName, str commanderName) extern;
func void s_SetHostileCommander(int commanderID, int commanderCachePos,
                                int relativeID, int relativeCachePos) extern;
func void s_SetFriendlyCommander(int commanderID, int commanderCachePos,
                                 int relativeID, int relativeCachePos) extern;

func int ConvertColor(int r, int g, int b)
{
  if r > 255 then r := 255; else if r < 0 then r := 0;
  if g > 255 then g := 255; else if g < 0 then g := 0;
  if b > 255 then b := 255; else if b < 0 then b := 0;
  return r*65536+g*256+b;
}

func void New(int classTableID, str name,
              var int objectID, var int cachePos)
{
  s_NewObject(classTableID, name);
  s_SearchObjectID(objectID, cachePos, name);
}

func void ChangeObjectAttrN(str attrName, str objectName)
var int event, attrID, attrCachePos, objectID, cachePos;
{
  s_SearchObjectID(attrID, attrCachePos, attrName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteObjectID(event, attrID, attrCachePos);
  s_CloseEventData(event);
  s_SearchObjectID(objectID, cachePos, objectName);
  s_SendEventNow(event, KR_SET_ATTR, objectID, cachePos);
}

func void SetAttribute_f(int objectID, int cachePos, str name, float value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteFloat(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_DOUBLE, objectID, cachePos);
}

func void SetAttribute_i(int objectID, int cachePos, str name, int value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteInt(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_INT, objectID, cachePos);
}

func void SetAttribute_s(int objectID, int cachePos, str name, str value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, name);
  s_WriteStr(event, value);
  s_CloseEventData(event);
  s_SendEventNow(event, s_ATTR_MSG_SET_STR, objectID, cachePos);
}

// Exact SYS.SCI helper required by populated People scripts.  The event is
// delivered to the real Skin object; this is not a parser-only compatibility
// shim.
func void SetAnimateBlock(int objectID, int cachePos,
                          int animateSet, int position, str name)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  if animateSet = 0
  then s_WriteStr(event, "set0");
  else s_WriteStr(event, "set");
  s_WriteInt(event, position);
  s_WriteStr(event, name);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_PROG, objectID, cachePos);
}

// Exact DEFINES.SCI helpers used by retail People and Tank animation setup.
func void CreateAnimSets(var int objectID, var int cachePos,
                         str skinName, int initialSet, int count)
var int event;
{
  s_SearchObjectID(objectID, cachePos, skinName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, "createAniSets");
  s_WriteInt(event, initialSet);
  s_WriteInt(event, count);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_PROG, objectID, cachePos);
}

func void CreateAnimSetsAuto(var int objectID, var int cachePos,
                             str skinName, int initialSet,
                             int count, int programLength)
var int event;
{
  s_SearchObjectID(objectID, cachePos, skinName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, "createAniSetsAuto");
  s_WriteInt(event, initialSet);
  s_WriteInt(event, count);
  s_WriteInt(event, programLength);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_PROG, objectID, cachePos);
}

func void fount_SetColors(int objectID, int cachePos,
                          int r0, int g0, int b0,
                          int r1, int g1, int b1,
                          int r2, int g2, int b2,
                          int r3, int g3, int b3)
{
  SetAttribute_i(objectID, cachePos, "m_RGB0", ConvertColor(r0,g0,b0));
  SetAttribute_i(objectID, cachePos, "m_RGB1", ConvertColor(r1,g1,b1));
  SetAttribute_i(objectID, cachePos, "m_RGB2", ConvertColor(r2,g2,b2));
  SetAttribute_i(objectID, cachePos, "m_RGB3", ConvertColor(r3,g3,b3));
}

)RR2NW_SCRIPT";

const char kTaxiSubjectBootstrapHelpers[] = R"RR2NW_SCRIPT(
func void CreateTaxi3DEx(int classTableID, vector position,
                         float horizontalAngle, str attributeName,
                         str objectName, int eventLabel)
var int objectID, cachePos, attributeID, attributeCachePos, event;
{
  s_New(classTableID, objectName, objectID, cachePos);
  s_SearchObjectID(attributeID, attributeCachePos, attributeName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteObjectID(event, attributeID, attributeCachePos);
  s_WriteFloat(event, position.x);
  s_WriteFloat(event, position.z);
  s_WriteFloat(event, -position.y);
  s_WriteFloat(event, horizontalAngle);
  s_CloseEventData(event);
  s_SendEventNow(event, eventLabel, objectID, cachePos);
}

func void CreateTaxiNameEx(int classTableID, vector position,
                           str attributeName, str objectName,
                           int eventLabel)
var int objectID, cachePos, attributeID, attributeCachePos, event;
{
  s_New(classTableID, objectName, objectID, cachePos);
  s_SearchObjectID(attributeID, attributeCachePos, attributeName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteObjectID(event, attributeID, attributeCachePos);
  s_WriteFloat(event, position.x);
  s_WriteFloat(event, -position.y);
  s_CloseEventData(event);
  s_SendEventNow(event, eventLabel, objectID, cachePos);
}

func void CreateTaxi3D(int classTableID, vector position,
                       float horizontalAngle, str attributeName)
{
  CreateTaxi3DEx(classTableID, position, horizontalAngle, attributeName,
                 "Taxi.Obj", taxi_SET_TO_POS);
}

func void CreateTaxi(int classTableID, vector position, str attributeName)
{
  CreateTaxiNameEx(classTableID, position, attributeName,
                   "Taxi.Obj", KR_SET_ATTR);
}
)RR2NW_SCRIPT";

const char kSmokeAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateSmokeAttr();
}
)RR2NW_SCRIPT";

const char kExplosionAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateExplosionAttr();
}
)RR2NW_SCRIPT";

const char kFarterAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateFarterAttrs();
}
)RR2NW_SCRIPT";

const char kTaxiAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateTaxiAttr();
}
)RR2NW_SCRIPT";

const char kTaxiSubjectBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateTaxi();
}
)RR2NW_SCRIPT";

const char kVehicleAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateVehicleAttr();
  main_CreateVehicle();
}
)RR2NW_SCRIPT";

const char kFarterSubjectBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateFarters();
}
)RR2NW_SCRIPT";

const char kLampAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateLampAttr();
}
)RR2NW_SCRIPT";

const char kCorpseAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateCorpseAttr();
}
)RR2NW_SCRIPT";

const char kBulletAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateBullets();
}
)RR2NW_SCRIPT";

const char kRouteBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_LoadRoute();
}
)RR2NW_SCRIPT";

const char kRecruitCenterBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  SetRecruitCenter();
}
)RR2NW_SCRIPT";

const char kPeopleAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreatePeopleAttr();
}
)RR2NW_SCRIPT";

const char kPeopleSubjectBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreatePeoples();
  SetAnimatePeople();
}
)RR2NW_SCRIPT";

const char kTankCannonAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateCannonsAndTankAttr();
}
)RR2NW_SCRIPT";

const char kHowitzerBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateHowitzerAttrs();
  main_CreateHowitzers();
}
)RR2NW_SCRIPT";

const char kSmokerAttributeBootstrapSuffix[] = R"RR2NW_SCRIPT(
func void main()
{
  main_CreateSmokerAttr();
}
)RR2NW_SCRIPT";

const char kWavMetadataBootstrapPrefix[] = R"RR2NW_SCRIPT(
const int EDO_WRITE = 1;
const int sk_EV_LOAD extern;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_WriteInt(int event, int value) extern;
func void s_WriteFloat(int event, float value) extern;
func void s_WriteStr(int event, str value) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func int s_AddClassTable(str className, int maxTableSize) extern;
func void s_NewObject(int classTableID, str name) extern;

func void LoadWAV(int classTableID, str objectName, str fileName,
                  float minFront, float minBack,
                  float maxFront, float maxBack, float intensity)
var int event, objectID, cachePos;
{
  s_NewObject(classTableID, objectName);
  s_SearchObjectID(objectID, cachePos, objectName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, fileName);
  s_WriteFloat(event, minFront);
  s_WriteFloat(event, minBack);
  s_WriteFloat(event, maxFront);
  s_WriteFloat(event, maxBack);
  s_WriteFloat(event, intensity);
  s_WriteInt(event, 0);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_LOAD, objectID, cachePos);
}

func void LoadWAVEx(int classTableID, str objectName, str fileName,
                    float minFront, float minBack,
                    float maxFront, float maxBack, float intensity,
                    int flags)
var int event, objectID, cachePos;
{
  s_NewObject(classTableID, objectName);
  s_SearchObjectID(objectID, cachePos, objectName);
  event := s_OpenEventData(EDO_WRITE);
  s_WriteStr(event, fileName);
  s_WriteFloat(event, minFront);
  s_WriteFloat(event, minBack);
  s_WriteFloat(event, maxFront);
  s_WriteFloat(event, maxBack);
  s_WriteFloat(event, intensity);
  s_WriteInt(event, flags);
  s_CloseEventData(event);
  s_SendEventNow(event, sk_EV_LOAD, objectID, cachePos);
}

)RR2NW_SCRIPT";

constexpr long kMaximumRetailAttributeSourceBytes = 128 * 1024;

bool ReadBoundedRetailAttributeSource(const char* relativePath,
                                      std::string* source) {
  if (relativePath == nullptr || source == nullptr) return false;

  FILE* file = RecoveredModRuntime_OpenRead(relativePath, nullptr);
  if (file == nullptr) return false;
  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return false;
  }
  const long size = std::ftell(file);
  if (size <= 0 || size > kMaximumRetailAttributeSourceBytes ||
      std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return false;
  }

  try {
    source->resize(static_cast<std::size_t>(size));
  } catch (...) {
    std::fclose(file);
    return false;
  }
  const bool read =
      std::fread(&(*source)[0], 1, static_cast<std::size_t>(size), file) ==
      static_cast<std::size_t>(size);
  std::fclose(file);
  return read && source->find('\0') == std::string::npos;
}

struct ScriptFunctionDefinition {
  std::string source;
  std::string scan;
};

bool IsScriptIdentifierCharacter(char value) {
  return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

std::string SanitizeScriptForStructure(const std::string& source) {
  std::string result(source);
  enum State { kCode, kLineComment, kBlockComment, kString } state = kCode;
  for (std::size_t i = 0; i < result.size(); ++i) {
    const char value = source[i];
    const char next = i + 1 < source.size() ? source[i + 1] : 0;
    if (state == kCode) {
      if (value == '/' && next == '/') {
        result[i] = result[i + 1] = ' ';
        ++i;
        state = kLineComment;
      } else if (value == '/' && next == '*') {
        result[i] = result[i + 1] = ' ';
        ++i;
        state = kBlockComment;
      } else if (value == '"') {
        result[i] = ' ';
        state = kString;
      }
    } else if (state == kLineComment) {
      if (value == '\r' || value == '\n') {
        state = kCode;
      } else {
        result[i] = ' ';
      }
    } else if (state == kBlockComment) {
      result[i] = ' ';
      if (value == '*' && next == '/') {
        result[i + 1] = ' ';
        ++i;
        state = kCode;
      }
    } else {
      result[i] = ' ';
      if (value == '"') state = kCode;
    }
  }
  return result;
}

void ParseScriptFunctions(
    const std::string& source,
    std::map<std::string, ScriptFunctionDefinition>* definitions,
    bool replaceExisting) {
  if (definitions == nullptr) return;
  const std::string scan = SanitizeScriptForStructure(source);
  std::size_t position = 0;
  while ((position = scan.find("func", position)) != std::string::npos) {
    const std::size_t keywordEnd = position + 4;
    if ((position > 0 && IsScriptIdentifierCharacter(scan[position - 1])) ||
        (keywordEnd < scan.size() &&
         IsScriptIdentifierCharacter(scan[keywordEnd]))) {
      position = keywordEnd;
      continue;
    }
    std::size_t cursor = keywordEnd;
    while (cursor < scan.size() &&
           std::isspace(static_cast<unsigned char>(scan[cursor])))
      ++cursor;
    while (cursor < scan.size() && IsScriptIdentifierCharacter(scan[cursor]))
      ++cursor;
    while (cursor < scan.size() &&
           std::isspace(static_cast<unsigned char>(scan[cursor])))
      ++cursor;
    const std::size_t nameBegin = cursor;
    while (cursor < scan.size() && IsScriptIdentifierCharacter(scan[cursor]))
      ++cursor;
    if (cursor == nameBegin) {
      position = keywordEnd;
      continue;
    }
    const std::string name = scan.substr(nameBegin, cursor - nameBegin);
    const std::size_t openBrace = scan.find('{', cursor);
    const std::size_t nextFunction = scan.find("func", cursor);
    const std::size_t external = scan.find("extern", cursor);
    if (openBrace == std::string::npos ||
        (nextFunction != std::string::npos && nextFunction < openBrace) ||
        (external != std::string::npos && external < openBrace)) {
      position = cursor;
      continue;
    }
    int depth = 1;
    std::size_t end = openBrace + 1;
    while (end < scan.size() && depth > 0) {
      if (scan[end] == '{') ++depth;
      if (scan[end] == '}') --depth;
      ++end;
    }
    if (depth != 0) return;
    if (replaceExisting || definitions->find(name) == definitions->end()) {
      ScriptFunctionDefinition definition = {
          source.substr(position, end - position),
          scan.substr(position, end - position)};
      (*definitions)[name] = definition;
    }
    position = end;
  }
}

void CollectScriptCalls(const std::string& scan,
                        const std::map<std::string, ScriptFunctionDefinition>&
                            available,
                        const std::set<std::string>& alreadyDefined,
                        std::set<std::string>* selected,
                        std::vector<std::string>* queue) {
  if (selected == nullptr || queue == nullptr) return;
  std::size_t position = 0;
  while (position < scan.size()) {
    if (!std::isalpha(static_cast<unsigned char>(scan[position])) &&
        scan[position] != '_') {
      ++position;
      continue;
    }
    const std::size_t begin = position++;
    while (position < scan.size() &&
           IsScriptIdentifierCharacter(scan[position]))
      ++position;
    const std::string name = scan.substr(begin, position - begin);
    std::size_t after = position;
    while (after < scan.size() &&
           std::isspace(static_cast<unsigned char>(scan[after])))
      ++after;
    if (after < scan.size() && scan[after] == '(' &&
        available.find(name) != available.end() &&
        alreadyDefined.find(name) == alreadyDefined.end() &&
        selected->insert(name).second) {
      queue->push_back(name);
    }
  }
}

bool AppendScriptFunctionClosure(
    const std::string& name,
    const std::map<std::string, ScriptFunctionDefinition>& available,
    const std::set<std::string>& alreadyDefined,
    std::set<std::string>* emitted, std::set<std::string>* visiting,
    std::string* supportSource) {
  if (emitted == nullptr || visiting == nullptr || supportSource == nullptr)
    return false;
  if (alreadyDefined.find(name) != alreadyDefined.end() ||
      emitted->find(name) != emitted->end())
    return true;
  const std::map<std::string, ScriptFunctionDefinition>::const_iterator it =
      available.find(name);
  if (it == available.end() || !visiting->insert(name).second) return false;

  std::set<std::string> selected;
  std::vector<std::string> dependencies;
  const std::size_t body = it->second.scan.find('{');
  if (body == std::string::npos) return false;
  CollectScriptCalls(it->second.scan.substr(body + 1), available,
                     alreadyDefined, &selected, &dependencies);
  for (std::size_t i = 0; i < dependencies.size(); ++i) {
    if (!AppendScriptFunctionClosure(dependencies[i], available,
                                     alreadyDefined, emitted, visiting,
                                     supportSource))
      return false;
  }
  visiting->erase(name);
  try {
    supportSource->append(it->second.source);
    supportSource->push_back('\n');
    emitted->insert(name);
  } catch (...) {
    return false;
  }
  return true;
}

bool BuildRetailSupportClosure(const std::string& rootSource,
                               const std::string& subjectSource,
                               const std::string& unitsSource,
                               const std::string& sysSource,
                               const std::string& sysfSource,
                               std::string* supportSource) {
  if (supportSource == nullptr) return false;
  std::map<std::string, ScriptFunctionDefinition> available;
  ParseScriptFunctions(unitsSource, &available, true);
  ParseScriptFunctions(sysSource, &available, false);
  ParseScriptFunctions(sysfSource, &available, false);

  std::map<std::string, ScriptFunctionDefinition> rootDefinitions;
  ParseScriptFunctions(kRetailAttributeBootstrapPrefix, &rootDefinitions,
                       true);
  ParseScriptFunctions(rootSource, &rootDefinitions, true);
  ParseScriptFunctions(subjectSource, &rootDefinitions, true);
  std::set<std::string> alreadyDefined;
  for (std::map<std::string, ScriptFunctionDefinition>::const_iterator it =
           rootDefinitions.begin();
       it != rootDefinitions.end(); ++it)
    alreadyDefined.insert(it->first);

  std::set<std::string> selected;
  std::vector<std::string> roots;
  CollectScriptCalls(SanitizeScriptForStructure(rootSource), available,
                     alreadyDefined, &selected, &roots);
  CollectScriptCalls(SanitizeScriptForStructure(subjectSource), available,
                     alreadyDefined, &selected, &roots);
  supportSource->clear();
  std::set<std::string> emitted;
  std::set<std::string> visiting;
  for (std::size_t i = 0; i < roots.size(); ++i)
    if (!AppendScriptFunctionClosure(roots[i], available, alreadyDefined,
                                     &emitted, &visiting, supportSource))
      return false;
  return true;
}

bool BuildOwnedTableScript(const std::string& source,
                           const char* entryPoint,
                           const char* ownedTable,
                           std::string* ownedSource) {
  if (entryPoint == nullptr || ownedTable == nullptr ||
      ownedSource == nullptr)
    return false;
  std::map<std::string, ScriptFunctionDefinition> definitions;
  ParseScriptFunctions(source, &definitions, true);
  const std::map<std::string, ScriptFunctionDefinition>::const_iterator entry =
      definitions.find(entryPoint);
  if (entry == definitions.end()) return false;

  const std::string& scan = entry->second.scan;
  const std::size_t owned = scan.find("s_AddClassTable");
  const std::size_t ownedName = entry->second.source.find(
      std::string("\"") + ownedTable + "\"", owned);
  if (owned == std::string::npos || ownedName == std::string::npos)
    return false;
  const std::size_t sibling = scan.find("s_AddClassTable", owned + 1u);
  if (sibling != std::string::npos && ownedName >= sibling) return false;
  if (sibling == std::string::npos) {
    *ownedSource = source;
    return true;
  }

  // Some May retail scripts append a second subsystem to a January-named
  // bootstrap (Level.06N puts DestroyableAttr after HowitzerAttr).  A recovered
  // owner must not publish that sibling table accidentally: trim complete
  // statements from the second table onward while retaining the exact helper
  // functions and the owned part of the entry point.
  std::size_t statement = sibling;
  while (statement > 0u && scan[statement - 1u] != ';' &&
         scan[statement - 1u] != '{')
    --statement;
  while (statement < sibling &&
         std::isspace(static_cast<unsigned char>(scan[statement])))
    ++statement;
  const std::size_t definitionOffset =
      source.find(entry->second.source);
  if (definitionOffset == std::string::npos) return false;
  const std::size_t definitionEnd =
      definitionOffset + entry->second.source.size();
  try {
    ownedSource->assign(source, 0u, definitionOffset + statement);
    ownedSource->append("\n}\n");
    ownedSource->append(source, definitionEnd, std::string::npos);
  } catch (...) {
    return false;
  }
  return true;
}

bool IsPeopleCreationPrimitive(const std::string& name) {
  return name == "CreateMan" || name == "CreateManEx" ||
         name == "CreateManName" || name == "CreateManNum";
}

int CountPeopleCreationsInFunction(
    const std::string& name,
    const std::map<std::string, ScriptFunctionDefinition>& definitions,
    std::set<std::string>* visiting, bool* valid) {
  if (visiting == nullptr || valid == nullptr || !*valid) return 0;
  if (IsPeopleCreationPrimitive(name)) return 1;
  const std::map<std::string, ScriptFunctionDefinition>::const_iterator it =
      definitions.find(name);
  if (it == definitions.end()) return 0;
  if (!visiting->insert(name).second) {
    *valid = false;
    return 0;
  }

  const std::string& scan = it->second.scan;
  std::size_t position = scan.find('{');
  int count = 0;
  if (position != std::string::npos) ++position;
  while (position < scan.size()) {
    if (!std::isalpha(static_cast<unsigned char>(scan[position])) &&
        scan[position] != '_') {
      ++position;
      continue;
    }
    const std::size_t begin = position++;
    while (position < scan.size() &&
           IsScriptIdentifierCharacter(scan[position]))
      ++position;
    const std::string call = scan.substr(begin, position - begin);
    std::size_t after = position;
    while (after < scan.size() &&
           std::isspace(static_cast<unsigned char>(scan[after])))
      ++after;
    if (after < scan.size() && scan[after] == '(' &&
        (IsPeopleCreationPrimitive(call) ||
         definitions.find(call) != definitions.end())) {
      count += CountPeopleCreationsInFunction(call, definitions, visiting,
                                              valid);
      if (!*valid) break;
    }
  }
  visiting->erase(name);
  return count;
}

int CountPeopleCreations(const std::string& peopleSource,
                         const std::string& subjectSource,
                         const std::string& unitsSource,
                         const std::string& sysfSource) {
  std::map<std::string, ScriptFunctionDefinition> definitions;
  ParseScriptFunctions(sysfSource, &definitions, true);
  ParseScriptFunctions(unitsSource, &definitions, true);
  ParseScriptFunctions(peopleSource, &definitions, true);
  ParseScriptFunctions(subjectSource, &definitions, true);
  bool valid = true;
  std::set<std::string> visiting;
  const int count = CountPeopleCreationsInFunction(
      "main_CreatePeoples", definitions, &visiting, &valid);
  return valid ? count : -1;
}

struct FarterSubjectScriptSummary {
  bool tablePresent;
  int objectCount;
};

int CountTextOccurrences(const std::string& text, const char* pattern) {
  int count = 0;
  std::size_t position = 0;
  const std::size_t length = std::strlen(pattern);
  while ((position = text.find(pattern, position)) != std::string::npos) {
    ++count;
    position += length;
  }
  return count;
}

int CountStandaloneCalls(const std::string& text, const char* functionName) {
  const std::string marker = std::string(functionName) + "(";
  int count = 0;
  std::size_t position = 0;
  while ((position = text.find(marker, position)) != std::string::npos) {
    const bool standalone =
        position == 0 ||
        (!std::isalnum(static_cast<unsigned char>(text[position - 1])) &&
         text[position - 1] != '_');
    if (standalone) ++count;
    position += marker.size();
  }
  return count;
}

bool CompactScriptSource(const std::string& source, std::string* compact) {
  if (compact == nullptr) return false;
  compact->clear();
  try {
    compact->reserve(source.size());
  } catch (...) {
    return false;
  }
  bool lineComment = false;
  bool blockComment = false;
  bool quoted = false;
  for (std::size_t index = 0; index < source.size(); ++index) {
    const char current = source[index];
    const char next = index + 1 < source.size() ? source[index + 1] : '\0';
    if (lineComment) {
      if (current == '\r' || current == '\n') lineComment = false;
      continue;
    }
    if (blockComment) {
      if (current == '*' && next == '/') {
        blockComment = false;
        ++index;
      }
      continue;
    }
    if (!quoted && current == '/' && next == '/') {
      lineComment = true;
      ++index;
      continue;
    }
    if (!quoted && current == '/' && next == '*') {
      blockComment = true;
      ++index;
      continue;
    }
    if (quoted && current == '\\' && next != '\0') {
      compact->push_back(current);
      compact->push_back(next);
      ++index;
      continue;
    }
    if (current == '"') quoted = !quoted;
    if (quoted || !std::isspace(static_cast<unsigned char>(current)))
      compact->push_back(current);
  }
  return !blockComment && !quoted;
}

int InspectSingleTableCapacity(const std::string& compact,
                               const char* tableName,
                               bool allowAbsent) {
  const std::string marker =
      std::string("s_AddClassTable(\"") + tableName + "\",";
  const std::size_t first = compact.find(marker);
  if (first == std::string::npos) return allowAbsent ? 0 : -1;
  if (compact.find(marker, first + marker.size()) != std::string::npos)
    return -1;
  std::size_t position = first + marker.size();
  int capacity = 0;
  int digits = 0;
  while (position < compact.size() &&
         std::isdigit(static_cast<unsigned char>(compact[position]))) {
    capacity = capacity * 10 + compact[position] - '0';
    ++position;
    ++digits;
  }
  return digits > 0 && capacity > 0 && capacity <= 512 ? capacity : -1;
}

struct PeopleScriptSummary {
  int attributeCapacity;
  int attributeCount;
  int subjectCapacity;
  int subjectCount;
};

struct TankCannonScriptSummary {
  int cannonAttributeCapacity;
  int tankAttributeCapacity;
  int tankGroupSubjectCapacity;
  int cannonSubjectCapacity;
  int tankSubjectCapacity;
};

struct HowitzerScriptSummary {
  int attributeCapacity;
  int subjectCapacity;
};

struct TeleportScriptSummary {
  int capacity;
  std::vector<TeleportDefinition> definitions;
};

struct CommanderScriptSummary {
  int capacity;
  int count;
  int hostilePairs;
  std::string functionSource;
};

struct MissionProjectSummary {
  int capacity;
  int nodeCapacity;
  int heapCapacity;
  int projectCount;
  int nodeCount;
  int dataBytes;
  int summaryCount;
  int permanentCount;
  int deferredHowitzerCount;
  int deferredDestroyableCount;
  unsigned long long fingerprint;
};

struct MissionProjectEntry {
  KR_ObjectID object;
  std::string name;
  int root;
  int permanent;
};

struct MissionProjectCollector {
  SimulationContext* context;
  std::vector<MissionProjectEntry>* entries;
  bool valid;
};

bool CollectMissionProject(KR_ObjectID object, void* parameter) {
  MissionProjectCollector* collector =
      static_cast<MissionProjectCollector*>(parameter);
  if (collector == nullptr || collector->context == nullptr ||
      collector->entries == nullptr) return false;
  mp_Project* project = projectTable.searchProject(object);
  const char* name = collector->context->searchObject(object);
  if (project == nullptr || name == nullptr || name[0] == 0) {
    collector->valid = false;
    return false;
  }
  try {
    collector->entries->push_back(
        MissionProjectEntry{object, name, project->get(),
                            project->m_permanent});
  } catch (...) {
    collector->valid = false;
    return false;
  }
  return true;
}

void HashMissionProjectBytes(unsigned long long* hash, const void* data,
                             std::size_t size) {
  if (hash == nullptr || data == nullptr) return;
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    *hash ^= bytes[index];
    *hash *= 1099511628211ull;
  }
}

void HashMissionProjectInt(unsigned long long* hash, int value) {
  HashMissionProjectBytes(hash, &value, sizeof(value));
}

bool InspectMissionProjects(SimulationContext* context,
                            const RecoveredLegacyScriptHost& host,
                            MissionProjectSummary* summary) {
  if (context == nullptr || summary == nullptr ||
      !host.ProjectTableCreated() || host.ProjectCount() < 0 ||
      host.ProjectCount() > 200 || host.ProjectNodeCount() < 0 ||
      host.ProjectNodeCount() > 1024 || host.ProjectDataBytes() < 0 ||
      host.ProjectDataBytes() > 10240)
    return false;

  std::vector<MissionProjectEntry> entries;
  MissionProjectCollector collector = {context, &entries, true};
  projectTable.userFind(CollectMissionProject, &collector);
  if (!collector.valid ||
      entries.size() != static_cast<std::size_t>(host.ProjectCount()))
    return false;
  std::sort(entries.begin(), entries.end(),
            [](const MissionProjectEntry& left,
               const MissionProjectEntry& right) {
              return left.name < right.name;
            });

  unsigned long long fingerprint = 1469598103934665603ull;
  std::set<int> allNodes;
  int summaryCount = 0;
  int permanentCount = 0;
  for (std::size_t projectIndex = 0; projectIndex < entries.size();
       ++projectIndex) {
    const MissionProjectEntry& entry = entries[projectIndex];
    HashMissionProjectBytes(&fingerprint, entry.name.c_str(),
                            entry.name.size() + 1u);
    HashMissionProjectInt(&fingerprint, entry.permanent);
    if (entry.permanent != 0) ++permanentCount;

    std::vector<int> pending(1, entry.root);
    std::set<int> projectNodes;
    while (!pending.empty()) {
      const int node = pending.back();
      pending.pop_back();
      const int decoded = mp_Code2Int(node);
      if (decoded == -1) continue;
      if (decoded < 0 || decoded >= host.ProjectNodeCount() ||
          !projectNodes.insert(decoded).second)
        return false;
      allNodes.insert(decoded);
      const int command = projectTable.getCommand(node);
      const int left = projectTable.getLeft(node);
      const int right = projectTable.getRight(node);
      const int decodedLeft = mp_Code2Int(left);
      const int decodedRight = mp_Code2Int(right);
      if (command < 0 || command > 35 || decodedLeft < -1 ||
          decodedLeft >= host.ProjectNodeCount() || decodedRight < -1 ||
          decodedRight >= host.ProjectNodeCount())
        return false;
      if (command == 30) ++summaryCount;
      HashMissionProjectInt(&fingerprint, decoded);
      HashMissionProjectInt(&fingerprint, command);
      HashMissionProjectInt(&fingerprint, decodedLeft);
      HashMissionProjectInt(&fingerprint, decodedRight);
      if (decodedRight >= 0) pending.push_back(right);
      if (decodedLeft >= 0) pending.push_back(left);
    }
  }
  if (allNodes.size() !=
      static_cast<std::size_t>(host.ProjectNodeCount()))
    return false;

  summary->capacity = 200;
  summary->nodeCapacity = 1024;
  summary->heapCapacity = 10240;
  summary->projectCount = host.ProjectCount();
  summary->nodeCount = host.ProjectNodeCount();
  summary->dataBytes = host.ProjectDataBytes();
  summary->summaryCount = summaryCount;
  summary->permanentCount = permanentCount;
  summary->deferredHowitzerCount = host.DeferredMissionHowitzerCount();
  summary->deferredDestroyableCount =
      host.DeferredMissionDestroyableCount();
  summary->fingerprint = fingerprint;
  return true;
}

bool HasCanonicalMissionProjectTableCall(const std::string& localMainSource) {
  std::string compact;
  return CompactScriptSource(localMainSource, &compact) &&
         CountTextOccurrences(
             compact,
             "s_CreateProjectTable(200,1024,1024*10);") == 1;
}

bool InspectTankCannonScripts(const std::string& attributeSource,
                              const std::string& localMainSource,
                              const std::string& setTankSource,
                              TankCannonScriptSummary* summary) {
  if (summary == nullptr) return false;
  std::string attributes;
  std::string localMain;
  std::string setTank;
  if (!CompactScriptSource(attributeSource, &attributes) ||
      !CompactScriptSource(localMainSource, &localMain) ||
      !CompactScriptSource(setTankSource, &setTank))
    return false;
  const int cannonAttributeCapacity =
      InspectSingleTableCapacity(attributes, "CannonAttr", true);
  const int tankAttributeCapacity =
      InspectSingleTableCapacity(attributes, "TankAttr", true);
  const int cannonSubjectCapacity =
      InspectSingleTableCapacity(localMain, "Cannon", false);
  const int tankGroupSubjectCapacity =
      InspectSingleTableCapacity(setTank, "TankGroup", false);
  const int tankSubjectCapacity =
      InspectSingleTableCapacity(setTank, "Tank", false);
  if (cannonAttributeCapacity < 0 || tankAttributeCapacity < 0 ||
      cannonSubjectCapacity <= 0 || tankGroupSubjectCapacity <= 0 ||
      tankSubjectCapacity <= 0)
    return false;
  summary->cannonAttributeCapacity = cannonAttributeCapacity;
  summary->tankAttributeCapacity = tankAttributeCapacity;
  summary->tankGroupSubjectCapacity = tankGroupSubjectCapacity;
  summary->cannonSubjectCapacity = cannonSubjectCapacity;
  summary->tankSubjectCapacity = tankSubjectCapacity;
  return true;
}

bool ConsumeScriptCharacter(const std::string& source, std::size_t* position,
                            char expected) {
  if (position == nullptr || *position >= source.size() ||
      source[*position] != expected)
    return false;
  ++*position;
  return true;
}

bool ConsumeScriptDouble(const std::string& source, std::size_t* position,
                         double* value) {
  if (position == nullptr || value == nullptr || *position >= source.size())
    return false;
  errno = 0;
  const char* begin = source.c_str() + *position;
  char* end = nullptr;
  const double parsed = std::strtod(begin, &end);
  if (end == begin || errno == ERANGE || !std::isfinite(parsed)) return false;
  *position += static_cast<std::size_t>(end - begin);
  *value = parsed;
  return true;
}

bool ConsumeScriptVector(const std::string& source, std::size_t* position,
                         CFVector3* value) {
  if (value == nullptr || !ConsumeScriptCharacter(source, position, '[') ||
      !ConsumeScriptDouble(source, position, &value->x) ||
      !ConsumeScriptCharacter(source, position, ',') ||
      !ConsumeScriptDouble(source, position, &value->y) ||
      !ConsumeScriptCharacter(source, position, ',') ||
      !ConsumeScriptDouble(source, position, &value->z) ||
      !ConsumeScriptCharacter(source, position, ']'))
    return false;
  return true;
}

bool InspectTeleportScript(const std::string& localMainSource,
                           TeleportScriptSummary* summary) {
  if (summary == nullptr) return false;
  summary->capacity = 0;
  summary->definitions.clear();
  std::string compact;
  if (!CompactScriptSource(localMainSource, &compact)) return false;
  const int capacity =
      InspectSingleTableCapacity(compact, "Teleport", true);
  if (capacity < 0 || capacity > 64) return false;

  const std::string marker = "CreateTeleport(";
  std::size_t search = 0;
  while ((search = compact.find(marker, search)) != std::string::npos) {
    if (search > 0 &&
        IsScriptIdentifierCharacter(compact[search - 1]))
      return false;
    std::size_t position = search + marker.size();
    TeleportDefinition definition;
    if (!ConsumeScriptVector(compact, &position, &definition.source) ||
        !ConsumeScriptCharacter(compact, &position, ',') ||
        !ConsumeScriptVector(compact, &position, &definition.destination) ||
        !ConsumeScriptCharacter(compact, &position, ',') ||
        !ConsumeScriptDouble(compact, &position, &definition.radius) ||
        !ConsumeScriptCharacter(compact, &position, ')') ||
        (position < compact.size() && compact[position] != ';') ||
        definition.radius <= 0.0 || definition.radius > 1024.0)
      return false;
    summary->definitions.push_back(definition);
    search = position;
  }
  if ((capacity == 0) != summary->definitions.empty() ||
      summary->definitions.size() > static_cast<std::size_t>(capacity))
    return false;
  summary->capacity = capacity;
  return true;
}

bool ReadTeleportScriptSummary(TeleportScriptSummary* summary) {
  std::string source;
  return summary != nullptr &&
         ReadBoundedRetailAttributeSource("SCINC\\localmain.sci", &source) &&
         InspectTeleportScript(source, summary);
}

bool InspectCommanderScript(const std::string& localMainSource,
                            CommanderScriptSummary* summary) {
  if (summary == nullptr) return false;
  std::string compact;
  if (!CompactScriptSource(localMainSource, &compact)) return false;
  const int capacity =
      InspectSingleTableCapacity(compact, "Commander", false);
  std::map<std::string, ScriptFunctionDefinition> definitions;
  ParseScriptFunctions(localMainSource, &definitions, true);
  const std::map<std::string, ScriptFunctionDefinition>::const_iterator it =
      definitions.find("local_createCommanders");
  if (capacity <= 0 || it == definitions.end()) return false;
  std::string functionCompact;
  if (!CompactScriptSource(it->second.source, &functionCompact)) return false;
  const int count = CountStandaloneCalls(functionCompact, "s_New") +
                    CountStandaloneCalls(functionCompact, "s_NewObject");
  const int hostilePairs =
      CountStandaloneCalls(functionCompact, "s_SetHostileCommander");
  if (count <= 0 || count > capacity || hostilePairs < 0) return false;
  summary->capacity = capacity;
  summary->count = count;
  summary->hostilePairs = hostilePairs;
  summary->functionSource = it->second.source;
  return true;
}

bool InspectPeopleScripts(const std::string& attributeSource,
                          const std::string& subjectSource,
                          const std::string& unitsSource,
                          const std::string& sysfSource,
                          PeopleScriptSummary* summary) {
  if (summary == nullptr) return false;
  std::string attributes;
  std::string subjects;
  if (!CompactScriptSource(attributeSource, &attributes) ||
      !CompactScriptSource(subjectSource, &subjects))
    return false;
  const int attributeCapacity =
      InspectSingleTableCapacity(attributes, "PeopleAttr", true);
  const int subjectCapacity =
      InspectSingleTableCapacity(subjects, "People", true);
  if (attributeCapacity < 0 || subjectCapacity < 0) return false;
  const int attributeCount =
      CountStandaloneCalls(attributes, "CreatePeopleAttr");
  const int subjectCount = CountPeopleCreations(
      attributeSource, subjectSource, unitsSource, sysfSource);
  if (attributeCount < 0 || subjectCount < 0 ||
      (attributeCapacity == 0 && attributeCount != 0) ||
      (subjectCapacity == 0 && subjectCount != 0) ||
      attributeCount > attributeCapacity ||
      (subjectCapacity > 0 && subjectCount >= subjectCapacity))
    return false;
  summary->attributeCapacity = attributeCapacity;
  summary->attributeCount = attributeCount;
  summary->subjectCapacity = subjectCapacity;
  summary->subjectCount = subjectCount;
  return true;
}

bool InspectHowitzerScripts(const std::string& attributeSource,
                            const std::string& subjectSource,
                            HowitzerScriptSummary* summary) {
  if (summary == nullptr) return false;
  std::string attributes;
  std::string subjects;
  if (!CompactScriptSource(attributeSource, &attributes) ||
      !CompactScriptSource(subjectSource, &subjects))
    return false;
  const int attributeCapacity =
      InspectSingleTableCapacity(attributes, "HowitzerAttr", true);
  const int subjectCapacity =
      InspectSingleTableCapacity(subjects, "Howitzer", true);
  if (attributeCapacity < 0 || subjectCapacity < 0 ||
      ((attributeCapacity == 0) != (subjectCapacity == 0)))
    return false;
  summary->attributeCapacity = attributeCapacity;
  summary->subjectCapacity = subjectCapacity;
  return true;
}

bool InspectFarterSubjectScript(const std::string& source,
                                FarterSubjectScriptSummary* summary) {
  if (summary == nullptr) return false;
  std::string compact;
  try {
    compact.reserve(source.size());
  } catch (...) {
    return false;
  }
  bool lineComment = false;
  bool blockComment = false;
  bool quoted = false;
  for (std::size_t index = 0; index < source.size(); ++index) {
    const char current = source[index];
    const char next = index + 1 < source.size() ? source[index + 1] : '\0';
    if (lineComment) {
      if (current == '\r' || current == '\n') lineComment = false;
      continue;
    }
    if (blockComment) {
      if (current == '*' && next == '/') {
        blockComment = false;
        ++index;
      }
      continue;
    }
    if (!quoted && current == '/' && next == '/') {
      lineComment = true;
      compact.push_back('\n');
      ++index;
      continue;
    }
    if (!quoted && current == '/' && next == '*') {
      blockComment = true;
      compact.push_back('\n');
      ++index;
      continue;
    }
    if (quoted && current == '\\' && next != '\0') {
      compact.push_back(current);
      compact.push_back(next);
      ++index;
      continue;
    }
    if (current == '"') quoted = !quoted;
    if (quoted || !std::isspace(static_cast<unsigned char>(current))) {
      compact.push_back(current);
    }
  }
  if (blockComment || quoted) return false;

  const int tableCount = CountTextOccurrences(
      compact, "s_AddClassTable(\"Farter\",25)");
  const int objectCount = CountTextOccurrences(compact, "CreateFarter(");
  if (tableCount > 1 ||
      (objectCount != 0 && objectCount != 23) ||
      (tableCount == 0 && objectCount != 0)) {
    return false;
  }
  summary->tablePresent = tableCount == 1;
  summary->objectCount = objectCount;
  return true;
}

struct TaxiSubjectScriptSummary {
  int capacity;
  int objectCount;
};

bool InspectTaxiSubjectScript(const std::string& source,
                              TaxiSubjectScriptSummary* summary) {
  if (summary == nullptr) return false;
  std::string compact;
  try {
    compact.reserve(source.size());
  } catch (...) {
    return false;
  }
  bool lineComment = false;
  bool blockComment = false;
  bool quoted = false;
  for (std::size_t index = 0; index < source.size(); ++index) {
    const char current = source[index];
    const char next = index + 1 < source.size() ? source[index + 1] : '\0';
    if (lineComment) {
      if (current == '\r' || current == '\n') lineComment = false;
      continue;
    }
    if (blockComment) {
      if (current == '*' && next == '/') {
        blockComment = false;
        ++index;
      }
      continue;
    }
    if (!quoted && current == '/' && next == '/') {
      lineComment = true;
      ++index;
      continue;
    }
    if (!quoted && current == '/' && next == '*') {
      blockComment = true;
      ++index;
      continue;
    }
    if (quoted && current == '\\' && next != '\0') {
      compact.push_back(current);
      compact.push_back(next);
      ++index;
      continue;
    }
    if (current == '"') quoted = !quoted;
    if (quoted || !std::isspace(static_cast<unsigned char>(current)))
      compact.push_back(current);
  }
  if (blockComment || quoted) return false;

  static const char tableMarker[] = "s_AddClassTable(\"Taxi\",";
  const std::size_t table = compact.find(tableMarker);
  if (table == std::string::npos ||
      compact.find(tableMarker, table + 1) != std::string::npos)
    return false;
  const std::size_t assignment = compact.rfind(":=", table);
  if (assignment == std::string::npos || assignment + 2u != table)
    return false;
  std::size_t variableStart = assignment;
  while (variableStart > 0) {
    const unsigned char character =
        static_cast<unsigned char>(compact[variableStart - 1]);
    if (!std::isalnum(character) && character != '_') break;
    --variableStart;
  }
  if (variableStart == assignment) return false;
  const std::string tableVariable =
      compact.substr(variableStart, assignment - variableStart);
  std::size_t position = table + sizeof(tableMarker) - 1u;
  int capacity = 0;
  int digits = 0;
  while (position < compact.size() &&
         std::isdigit(static_cast<unsigned char>(compact[position]))) {
    capacity = capacity * 10 + (compact[position] - '0');
    ++position;
    ++digits;
  }
  const std::string createTaxi =
      std::string("CreateTaxi(") + tableVariable + ',';
  const std::string createTaxi3D =
      std::string("CreateTaxi3D(") + tableVariable + ',';
  const int objectCount = CountTextOccurrences(compact, createTaxi.c_str()) +
                          CountTextOccurrences(compact,
                                               createTaxi3D.c_str());
  const bool knownShape =
      (capacity == 100 &&
       (objectCount == 20 || objectCount == 35 || objectCount == 38 ||
        objectCount == 66)) ||
      (capacity == 150 &&
       (objectCount == 28 || objectCount == 93)) ||
      (capacity == 80 && objectCount == 0) ||
      (capacity == 20 && objectCount == 1) ||
      (capacity == 4 && objectCount == 2);
  if (digits == 0 || objectCount >= capacity || !knownShape) return false;
  summary->capacity = capacity;
  summary->objectCount = objectCount;
  return true;
}

int InspectVehicleAttributeCapacity(const std::string& source) {
  static const char marker[] = "s_AddClassTable(\"VehicleAttr\",";
  const std::size_t markerPosition = source.find(marker);
  if (markerPosition == std::string::npos) return 0;
  std::size_t position = markerPosition + sizeof(marker) - 1u;
  while (position < source.size() &&
         std::isspace(static_cast<unsigned char>(source[position]))) {
    ++position;
  }
  int capacity = 0;
  int digits = 0;
  while (position < source.size() &&
         std::isdigit(static_cast<unsigned char>(source[position]))) {
    capacity = capacity * 10 + (source[position] - '0');
    ++position;
    ++digits;
  }
  return digits > 0 && capacity > 0 && capacity <= 128 ? capacity : 0;
}

struct RecoveredArenaSeanceState {
  unsigned long long issues;
  unsigned long long extendedIssues;
  bool arenaOpen;
  bool scriptCompleted;
  bool birdAttributesReady;
  bool portalReady;
  bool teleportRoutesReady;
  bool teleportTargetLevel;
  bool orphanAttributesReady;
  bool orphanReferencesReady;
  bool orphanSubjectReady;
  bool artefactAttributesReady;
  bool smokeAttributesReady;
  bool smokeSubjectReady;
  bool smokeVisualResourcesReady;
  bool explosionAttributesReady;
  bool explosionSubjectReady;
  bool explosionImpulseReady;
  bool explosionLightReady;
  bool explosionSoundReady;
  bool explosionParticlesReady;
  bool explosionSmokeReady;
  bool explosionPieceReady;
  bool explosionTraceReady;
  bool explosionActiveWorldReady;
  bool vehicleAttributesReady;
  bool vehicleReferencesReady;
  bool taxiAttributesReady;
  bool taxiReferencesReady;
  bool taxiSubjectReady;
  bool bulletAttributesReady;
  bool bulletReferencesReady;
  bool bulletSubjectRegistrationReady;
  bool bulletSubjectReady;
  bool bulletImpactEffectsReady;
  bool bulletGroundSparkReady;
  bool bulletBarrelSmokeReady;
  bool bulletActiveWorldReady;
  bool farterAttributesReady;
  bool farterReferencesReady;
  bool farterRuntimeReady;
  bool farterSubjectReady;
  bool lampAttributesReady;
  bool corpseAttributesReady;
  bool corpseReferencesReady;
  bool corpseRuntimeReady;
  bool corpseSubjectReady;
  bool corpseActiveWorldReady;
  bool smokerAttributesReady;
  bool smokerReferencesReady;
  bool smokerRuntimeReady;
  bool dynSmokerReady;
  bool wavMetadataReady;
  bool soundObjectReady;
  bool soundDistanceReady;
  bool skinResourcesReady;
  bool skinAnimationsReady;
  bool staticMechanismsReady;
  bool staticMechanismTargetLevel;
  bool staticMechanismLevelOne;
  bool staticMechanismLevelFive;
  bool sparkAttributesReady;
  bool sparkSubjectReady;
  bool sparkVisualResourcesReady;
  bool sparkActiveWorldReady;
  bool smokeActiveWorldReady;
  bool routeReady;
  bool peopleAttributesReady;
  bool peopleReferencesReady;
  bool peopleSubjectReady;
  int peopleActiveWorldReconstructedIDs;
  int peopleActiveWorldSchedulerEvents;
  int peopleActiveWorldRollbacks;
  unsigned long long peopleActiveWorldFingerprint;
  bool tankCannonAttributesReady;
  bool tankReferencesReady;
  bool tankCannonSubjectTablesReady;
  bool commanderReady;
  bool missionProjectsReady;
  bool recruitCentersReady;
  bool missionTankLifecycleReady;
  bool activeWorldPersistenceReady;
  bool vehicleReady;
  int teleportCapacity;
  int teleportRouteCount;
  int teleportProbeRejectedNonPlayer;
  int teleportProbePhysicsCollisions;
  int teleportProbeAppliedPlayer;
  int teleportProbeVehicleRollbacks;
  unsigned long long teleportFingerprint;
  int vehicleActiveWorldReconstructedIDs;
  int vehicleActiveWorldRollbacks;
  unsigned long long vehicleActiveWorldFingerprint;
  int vehicleAttributeCount;
  int vehicleAttributeCapacity;
  unsigned long long vehicleAttributeFingerprint;
  unsigned long long vehicleReferenceFingerprint;
  unsigned long long taxiReferenceFingerprint;
  unsigned long long orphanReferenceFingerprint;
  int orphanSubjectCapacity;
  unsigned long long orphanSubjectFingerprint;
  int taxiSubjectCapacity;
  int taxiSubjectCount;
  int taxiSubjectSoundCount;
  unsigned long long taxiSubjectFingerprint;
  int taxiProbeInvalidStarts;
  int taxiProbeValidStarts;
  int taxiProbeRenderReady;
  int taxiProbeSoundReady;
  int taxiProbeRollbacks;
  int bulletAttributeCount;
  int bulletAttributeCapacity;
  int bulletSubjectCapacity;
  int sparkSubjectCapacity;
  unsigned long long sparkSubjectFingerprint;
  unsigned long long sparkVisualResourceFingerprint;
  int sparkProbeInvalidStarts;
  int sparkProbeQueuedCreates;
  int sparkProbeQueueRollbacks;
  int sparkProbePhaseTransitions;
  int sparkProbeExpirations;
  int sparkActiveWorldCapturedOwners;
  int sparkActiveWorldSchedulerEvents;
  int sparkActiveWorldRollbacks;
  int sparkActiveWorldReconstructedIDs;
  int sparkActiveWorldStableRoundTrips;
  int sparkActiveWorldResumedPhases;
  unsigned long long sparkActiveWorldFingerprint;
  int smokeActiveWorldCapturedOwners;
  int smokeActiveWorldCapturedBlobs;
  int smokeActiveWorldSchedulerEvents;
  int smokeActiveWorldRollbacks;
  int smokeActiveWorldReconstructedIDs;
  int smokeActiveWorldStableRoundTrips;
  int smokeActiveWorldResumedMoves;
  unsigned long long smokeActiveWorldFingerprint;
  int corpseActiveWorldCapturedOwners;
  int corpseActiveWorldOwnedSmokers;
  int corpseActiveWorldSchedulerEvents;
  int corpseActiveWorldRollbacks;
  int corpseActiveWorldReconstructedObjects;
  int corpseActiveWorldStableRoundTrips;
  int corpseActiveWorldResumedEmissions;
  int corpseActiveWorldResumedDeaths;
  unsigned long long corpseActiveWorldFingerprint;
  unsigned long long bulletAttributeFingerprint;
  unsigned long long bulletReferenceFingerprint;
  unsigned long long bulletSubjectFingerprint;
  int explosionSubjectCapacity;
  unsigned long long explosionSubjectFingerprint;
  int explosionProbeInvalidStarts;
  int explosionProbeAllocationRollbacks;
  int explosionProbeQueuedCommands;
  int explosionProbeQueueRollbacks;
  int explosionProbeExecutedCommands;
  int explosionProbeDamageApplications;
  unsigned long long explosionSoundReferenceFingerprint;
  int explosionSoundProbeStarted;
  int explosionSoundProbeDependencySkips;
  int explosionSoundProbeRollbacks;
  unsigned long long explosionParticleVisualFingerprint;
  int explosionParticleProbeStartedBranches;
  int explosionParticleProbeSimpleParticles;
  int explosionParticleProbeSnakeParticles;
  int explosionParticleProbeRays;
  int explosionParticleProbeDependencySkips;
  int explosionParticleProbeMoveSteps;
  int explosionParticleProbeExpiredParents;
  int explosionParticleProbeRolledBackBranches;
  unsigned long long explosionSmokeVisualFingerprint;
  int explosionSmokeProbeStartedSprites;
  int explosionSmokeProbeDependencySkips;
  int explosionSmokeProbeMoveSteps;
  int explosionSmokeProbeExpiredParents;
  int explosionSmokeProbeRolledBackSprites;
  unsigned long long explosionPieceReferenceFingerprint;
  int explosionPieceProbeStartedPieces;
  int explosionPieceProbeDependencySkips;
  int explosionPieceProbeMoveSteps;
  int explosionPieceProbeExpiredParents;
  int explosionPieceProbeRolledBackPieces;
  unsigned long long explosionTraceReferenceFingerprint;
  int explosionTraceProbeStartedPieces;
  int explosionTraceProbeQuotaGateSkips;
  int explosionTraceProbePuffEvents;
  int explosionTraceProbeSmokeChildren;
  int explosionTraceProbeMoveSteps;
  int explosionTraceProbeExpiredParents;
  int explosionTraceProbeRolledBackPieces;
  int explosionActiveWorldCapturedOwners;
  int explosionActiveWorldCapturedBranches;
  int explosionActiveWorldSchedulerEvents;
  int explosionActiveWorldSoundChildren;
  int explosionActiveWorldRollbacks;
  int explosionActiveWorldReconstructedIDs;
  int explosionActiveWorldStableRoundTrips;
  int explosionActiveWorldResumedMoves;
  unsigned long long explosionActiveWorldFingerprint;
  int bulletSubjectProbeMoveCount;
  int bulletCollisionScheduledChecks;
  int bulletCollisionExecutedChecks;
  int bulletCollisionSphereCases;
  int bulletCollisionEarliestHitCases;
  int bulletCollisionWaterlineCases;
  int bulletCollisionSceneQueries;
  int bulletEffectQueuedBatches;
  int bulletEffectQueuedChildren;
  int bulletEffectSplashFirstCases;
  int bulletEffectRolledBackChildren;
  int bulletGroundSparkQueued;
  int bulletGroundSparkRolledBack;
  int bulletBarrelSmokeThresholdStarts;
  int bulletBarrelSmokeFrameGateSkips;
  int bulletBarrelSmokeAttributeGateSkips;
  int bulletBarrelSmokeRollbacks;
  int bulletActiveWorldCapturedOwners;
  int bulletActiveWorldSchedulerEvents;
  int bulletActiveWorldRollbacks;
  int bulletActiveWorldReconstructedIDs;
  int bulletActiveWorldStableRoundTrips;
  int bulletActiveWorldResumedMoves;
  int bulletActiveWorldTombstonedMasters;
  unsigned long long bulletActiveWorldFingerprint;
  int corpseSubjectCapacity;
  unsigned long long corpseSubjectFingerprint;
  int skinModelCount;
  int skinSpriteCount;
  unsigned long long skinCatalogFingerprint;
  unsigned long long skinResourceFingerprint;
  int skinAnimationEntryCallCount;
  int skinAnimatedModelCount;
  int skinAnimationCommandCount;
  unsigned long long skinAnimationSourceFingerprint;
  unsigned long long skinAnimationStateFingerprint;
  int skinAnimationPoseTemporalModelCount;
  int skinAnimationPoseChangedModelCount;
  int skinAnimationPoseSampleCount;
  int skinAnimationPoseRestoredModifierCount;
  unsigned long long skinAnimationPoseFingerprint;
  int staticMechanismBindingCount;
  int staticMechanismWaterwheelCount;
  int staticMechanismFlagCount;
  int staticMechanismRotatingCount;
  int staticMechanismDoorCount;
  int staticMechanismPol16Count;
  int staticMechanismChangedBindingCount;
  int staticMechanismPoseSampleCount;
  int staticMechanismRestoredModifierCount;
  unsigned long long staticMechanismFingerprint;
  int wavMetadataCount;
  int wavMetadataCapacity;
  unsigned long long wavCatalogFingerprint;
  unsigned long long wavResourceFingerprint;
  int soundObjectCapacity;
  unsigned long long soundObjectFingerprint;
  double previousSoundDistance;
  double previousSoundDistanceSquared;
  double soundDistance;
  double soundDistanceSquared;
  unsigned long long farterReferenceFingerprint;
  int farterSubjectCapacity;
  unsigned long long farterSubjectFingerprint;
  int farterScriptObjectCount;
  int farterLiveObjectCount;
  int farterSoundObjectCount;
  int farterNearFrameAudibleCount;
  int farterFarFrameAudibleCount;
  bool farterAudibleFrameTransition;
  unsigned long long corpseReferenceFingerprint;
  unsigned long long smokerReferenceFingerprint;
  int smokeSubjectCapacity;
  unsigned long long smokeSubjectFingerprint;
  unsigned long long smokeVisualResourceFingerprint;
  int dynSmokerCapacity;
  unsigned long long dynSmokerFingerprint;
  int peopleAttributeCapacity;
  int peopleAttributeCount;
  int peopleSubjectCapacity;
  int peopleSubjectCount;
  int peopleSubjectSoundCount;
  unsigned long long peopleAttributeFingerprint;
  unsigned long long peopleSubjectFingerprint;
  int peopleProbeScheduledMoves;
  int peopleProbeCadenceBounded;
  int peopleProbeRenderedPoseFrames;
  int peopleProbeViewBoundaryResets;
  int peopleProbeBulletDamageApplications;
  int peopleProbeDeathTransitions;
  int peopleProbeSaveStateRoundTrips;
  int peopleProbeRollbacks;
  int peopleCombatProbeAvailable;
  int peopleCombatProbeAttackerReady;
  int peopleCombatProbeTargetReady;
  int peopleCombatProbeRouteDisplacement;
  int peopleCombatProbeTargetAcquired;
  int peopleCombatProbeTargetCadence;
  int peopleCombatProbeProjectileStarted;
  int peopleCombatProbeDamageDelivered;
  int peopleCombatProbeDeathTransition;
  int peopleCombatProbeDeathEffects;
  int peopleCombatProbeRollbacks;
  int cannonAttributeCapacity;
  int cannonAttributeCount;
  int cannonSubjectCapacity;
  int cannonSubjectCount;
  int tankAttributeCapacity;
  int tankAttributeCount;
  int tankSubjectCapacity;
  int tankSubjectCount;
  unsigned long long cannonAttributeFingerprint;
  unsigned long long cannonSubjectFingerprint;
  unsigned long long tankAttributeFingerprint;
  unsigned long long tankSubjectFingerprint;
  int tankProbeAvailable;
  int tankProbeValidStarts;
  int tankProbeDynamicReady;
  int tankProbeRenderReady;
  int tankProbeCannonReady;
  int tankProbeScheduledMoves;
  int tankProbeCadenceBounded;
  int tankProbeRenderedPoseFrames;
  int tankProbeViewBoundaryResets;
  int tankProbeBulletDamageApplications;
  int tankProbeDeathTransitions;
  int tankProbeDeathEffects;
  int tankProbeSaveStateRoundTrips;
  int tankProbeRollbacks;
  int commanderCapacity;
  int commanderCount;
  int commanderHostileLinks;
  unsigned long long commanderFingerprint;
  int missionProjectCapacity;
  int missionProjectNodeCapacity;
  int missionProjectHeapCapacity;
  int missionProjectCount;
  int missionProjectNodeCount;
  int missionProjectDataBytes;
  int missionProjectSummaryCount;
  int missionProjectPermanentCount;
  int missionProjectDeferredHowitzerCount;
  int missionProjectDeferredDestroyableCount;
  unsigned long long missionProjectFingerprint;
  int recruitCenterCapacity;
  int recruitCenterCount;
  int recruitCenterVideoCount;
  int recruitCenterDefaultTaxiCount;
  int recruitCenterDictionaryCount;
  unsigned long long recruitCenterFingerprint;
  int tankGroupSubjectCapacity;
  int missionTankAvailable;
  int missionTankSpawns;
  int missionTankMembershipLinks;
  int missionTankFindEnemyCycles;
  int missionTankMovingCycles;
  int missionTankStableRoundTrips;
  int missionTankReconstructedIDs;
  int missionTankRollbacks;
  unsigned long long missionTankFingerprint;
  int activeWorldSections;
  int activeWorldEvents;
  int activeWorldOwnerPhases;
  int activeWorldReferencePhases;
  int activeWorldEventPhases;
  int activeWorldCreatedOwners;
  int activeWorldMissionRecords;
  int activeWorldMissionConditionReferences;
  int activeWorldMissionRouteReferences;
  int activeWorldMissionCheckEvents;
  int activeWorldClockRecords;
  unsigned int activeWorldRngAlgorithm;
  int activeWorldRngStateBytes;
  unsigned long long activeWorldRngDrawCount;
  int activeWorldCorruptionRejects;
  int activeWorldRollbacks;
  unsigned long long activeWorldContainerBytes;
  unsigned long long activeWorldFingerprint;
  char lastError[256];
};

RecoveredArenaSeanceState g_state = {};

void SetError(const char* message) {
  std::snprintf(g_state.lastError, sizeof(g_state.lastError), "%s",
                message == nullptr ? "unknown seance failure" : message);
}

void Report(unsigned long long issue, const char* message) {
  g_state.issues |= issue;
  SetError(message);
}

void ReportExtended(unsigned long long issue, const char* message) {
  g_state.extendedIssues |= issue;
  SetError(message);
}

void RollBackPartiallyOpenedArena(SimulationContext* context) {
  if (context == nullptr) return;
  if (g_arena.getContext() == context || context->isExist("Storage")) {
    g_state.arenaOpen = true;
    RecoveredArenaSeance_Release();
  }
}

unsigned int IssueForScriptStatus(ERecoveredLegacyScriptRunStatus status) {
  switch (status) {
    case RECOVERED_LEGACY_SCRIPT_RUN_ALLOCATION_FAILURE:
      return RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_INITIALIZATION_FAILURE:
    case RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT:
      return RECOVERED_ARENA_SEANCE_SCRIPT_INITIALIZATION_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE:
      return RECOVERED_ARENA_SEANCE_SCRIPT_COMPILE_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_TIMEOUT:
      return RECOVERED_ARENA_SEANCE_SCRIPT_TIMEOUT;
    case RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE:
      return RECOVERED_ARENA_SEANCE_SCRIPT_HOST_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_PROCESS_FAILURE:
    case RECOVERED_LEGACY_SCRIPT_RUN_EXCEPTION:
      return RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
    case RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS:
      break;
  }
  return RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE;
}

bool RunCommonAttributeBootstrap(SimulationContext* context,
                                 double startTime) {
  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  if (!RecoveredLegacyScript_RunMemory(
          kCommonAttributeBootstrapScript, kCommonBootstrapProgramName,
          profile, context, startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool RunMissionProjectBootstrap(SimulationContext* context, double startTime,
                                MissionProjectSummary* summary) {
  std::string localMainSource;
  std::string definitionsSource;
  std::string helpersSource;
  std::string projectsSource;
  if (summary == nullptr ||
      !ReadBoundedRetailAttributeSource("SCINC\\localmain.sci",
                                        &localMainSource) ||
      !ReadBoundedRetailAttributeSource("..\\DEFS.H", &definitionsSource) ||
      !ReadBoundedRetailAttributeSource("..\\PFUNC.SCI", &helpersSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\BRIEF.SCI",
                                        &projectsSource)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "could not read bounded retail mission project sources");
    return false;
  }
  if (!HasCanonicalMissionProjectTableCall(localMainSource)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "localmain.sci mission ProjectTable contract is not canonical");
    return false;
  }

  std::string program;
  try {
    program.reserve(sizeof(kMissionProjectBootstrapPrefix) +
                    definitionsSource.size() + helpersSource.size() +
                    projectsSource.size() +
                    sizeof(kMissionProjectBootstrapSuffix) + 5u);
    program.append(kMissionProjectBootstrapPrefix);
    program.append(definitionsSource);
    program.push_back('\n');
    program.append(helpersSource);
    program.push_back('\n');
    program.append(projectsSource);
    program.push_back('\n');
    program.append(kMissionProjectBootstrapSuffix);
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate bounded retail mission project bootstrap");
    return false;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
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
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kMissionProjectProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  if (!InspectMissionProjects(context, host, summary)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "retail mission project inventory is malformed");
    return false;
  }
  // Topology alone cannot distinguish equal-length authored objective text.
  // Fold the exact retail definitions/helpers/BRIEF byte streams into the
  // identity while retaining the independently validated live graph counts.
  const unsigned char separator = 0xffu;
  HashMissionProjectBytes(&summary->fingerprint, definitionsSource.data(),
                          definitionsSource.size());
  HashMissionProjectBytes(&summary->fingerprint, &separator, 1u);
  HashMissionProjectBytes(&summary->fingerprint, helpersSource.data(),
                          helpersSource.size());
  HashMissionProjectBytes(&summary->fingerprint, &separator, 1u);
  HashMissionProjectBytes(&summary->fingerprint, projectsSource.data(),
                          projectsSource.size());
  return true;
}

bool RunRetailAttributeBootstrap(SimulationContext* context,
                                 double startTime,
                                 const char* rootPath,
                                 const char* levelPath,
                                 const char* suffix,
                                 const char* programName,
                                 unsigned long long sourceIssue,
                                 const char* description,
                                 bool extendedSourceIssue = false) {
  std::string rootSource;
  std::string levelSource;
  if (!ReadBoundedRetailAttributeSource(rootPath, &rootSource) ||
      (levelPath != nullptr &&
       !ReadBoundedRetailAttributeSource(levelPath, &levelSource))) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "could not read bounded retail %s source beside selected "
                  "Level", description);
    if (extendedSourceIssue) {
      ReportExtended(sourceIssue, message);
    } else {
      Report(sourceIssue, message);
    }
    return false;
  }

  std::string program;
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    rootSource.size() + levelSource.size() +
                    std::strlen(suffix) + 3u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(rootSource);
    program.push_back('\n');
    if (!levelSource.empty()) {
      program.append(levelSource);
      program.push_back('\n');
    }
    program.append(suffix);
  } catch (...) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "could not allocate bounded retail %s bootstrap source",
                  description);
    if (extendedSourceIssue) {
      ReportExtended(sourceIssue, message);
    } else {
      Report(sourceIssue, message);
    }
    return false;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), programName, profile, context, startTime,
          &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool RunSmokeAttributeBootstrap(SimulationContext* context,
                                double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\SMOKE.SCI", nullptr,
      kSmokeAttributeBootstrapSuffix, kSmokeAttributeProgramName,
      RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "SMOKE.SCI");
}

bool RunSmokerAttributeBootstrap(SimulationContext* context,
                                 double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\SMOKE.SCI", nullptr,
      kSmokerAttributeBootstrapSuffix, kSmokerAttributeProgramName,
      RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "SMOKE.SCI SmokerAttr section");
}

bool RunExplosionAttributeBootstrap(SimulationContext* context,
                                    double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\EXPLOSION.SCI",
      "SCINC\\EXPLOSION_LOC.SCI", kExplosionAttributeBootstrapSuffix,
      kExplosionAttributeProgramName,
      RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "EXPLOSION.SCI + SCINC\\EXPLOSION_LOC.SCI");
}

bool RunTaxiAttributeBootstrap(SimulationContext* context,
                               double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "SCINC\\TAXI.SCI", nullptr,
      kTaxiAttributeBootstrapSuffix, kTaxiAttributeProgramName,
      RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "SCINC\\TAXI.SCI", true);
}

bool RunTaxiSubjectBootstrap(SimulationContext* context,
                             double startTime,
                             TaxiSubjectScriptSummary* summary) {
  std::string source;
  if (!ReadBoundedRetailAttributeSource("SCINC\\SET_TAXI.SCI", &source)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_SOURCE_UNAVAILABLE,
        "could not read bounded retail SCINC\\SET_TAXI.SCI beside selected "
        "Level");
    return false;
  }
  if (!InspectTaxiSubjectScript(source, summary)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_ROSTER_INVALID,
        "SCINC\\SET_TAXI.SCI is not an admitted retail Taxi roster");
    return false;
  }

  std::string program;
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    sizeof(kTaxiSubjectBootstrapHelpers) + source.size() +
                    sizeof(kTaxiSubjectBootstrapSuffix) + 3u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(kTaxiSubjectBootstrapHelpers);
    program.append(source);
    program.push_back('\n');
    program.append(kTaxiSubjectBootstrapSuffix);
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate bounded retail Taxi subject bootstrap");
    return false;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kTaxiSubjectProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool RunVehicleAttributeBootstrap(SimulationContext* context,
                                  double startTime) {
  std::string source;
  if (!ReadBoundedRetailAttributeSource("SCINC\\VEHICLE.SCI", &source)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_SOURCE_UNAVAILABLE,
        "could not read bounded retail SCINC\\VEHICLE.SCI beside selected "
        "Level");
    return false;
  }
  VehicleAttributeState_SetCapacity(
      InspectVehicleAttributeCapacity(source));
  return RunRetailAttributeBootstrap(
      context, startTime, "SCINC\\VEHICLE.SCI", nullptr,
      kVehicleAttributeBootstrapSuffix, kVehicleAttributeProgramName,
      RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "SCINC\\VEHICLE.SCI", true);
}

bool RunFarterAttributeBootstrap(SimulationContext* context,
                                 double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\FARTER.SCI", "SCINC\\FARTERATTR.SCI",
      kFarterAttributeBootstrapSuffix, kFarterAttributeProgramName,
      RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "FARTER.SCI + SCINC\\FARTERATTR.SCI");
}

bool RunFarterSubjectBootstrap(SimulationContext* context,
                               double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\FARTER.SCI", "SCINC\\SET_FARTER.SCI",
      kFarterSubjectBootstrapSuffix, kFarterSubjectProgramName,
      RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
      "FARTER.SCI + SCINC\\SET_FARTER.SCI subject roster");
}

bool RunLampAttributeBootstrap(SimulationContext* context,
                               double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\LAMP.SCI", nullptr,
      kLampAttributeBootstrapSuffix, kLampAttributeProgramName,
      RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "LAMP.SCI");
}

bool RunCorpseAttributeBootstrap(SimulationContext* context,
                                 double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "SCINC\\CORPSE.SCI", nullptr,
      kCorpseAttributeBootstrapSuffix, kCorpseAttributeProgramName,
      RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "SCINC\\CORPSE.SCI");
}

bool RunBulletAttributeBootstrap(SimulationContext* context,
                                 double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "..\\BULLET.SCI", "SCINC\\bullet_loc.sci",
      kBulletAttributeBootstrapSuffix, kBulletAttributeProgramName,
      RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_SOURCE_UNAVAILABLE,
      "BULLET.SCI + SCINC\\bullet_loc.sci", true);
}

bool RunRouteBootstrap(SimulationContext* context, double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "SCINC\\load_route.sci", nullptr,
      kRouteBootstrapSuffix, kRouteProgramName,
      RECOVERED_ARENA_SEANCE_EXT_ROUTE_SOURCE_UNAVAILABLE,
      "SCINC\\load_route.sci", true);
}

bool RunRecruitCenterBootstrap(SimulationContext* context,
                               double startTime, bool* published) {
  if (published == nullptr) return false;
  *published = false;
  std::string rootSource;
  std::string levelSource;
  if (!ReadBoundedRetailAttributeSource("..\\incubator.sci", &rootSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\RECRCEN.SCI",
                                        &levelSource)) {
    // Hermetic source-only fixtures intentionally copy only the already
    // recovered slices. A verified retail manifest makes these May files
    // mandatory; otherwise preserve the historical synthetic MSH1 fallback.
    if (!RecoveredRetailScriptManifest_IsReady()) return true;
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "could not read bounded retail RecruitCenter sources");
    return false;
  }
  if (!RunRetailAttributeBootstrap(
          context, startTime, "..\\incubator.sci",
          "SCINC\\RECRCEN.SCI", kRecruitCenterBootstrapSuffix,
          kRecruitCenterProgramName,
          RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
          "incubator.sci + SCINC\\RECRCEN.SCI"))
    return false;
  if (!RecruitCenterSubjectState_TableReady(context)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "retail RecruitCenter roster is incomplete");
    return false;
  }
  *published = true;
  return true;
}

bool ReadPeopleScriptSummary(PeopleScriptSummary* summary) {
  std::string attributeSource;
  std::string subjectSource;
  std::string unitsSource;
  std::string sysfSource;
  if (summary == nullptr ||
      !ReadBoundedRetailAttributeSource("SCINC\\PEOPLE.SCI",
                                        &attributeSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\set_people.sci",
                                        &subjectSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\units.sci", &unitsSource) ||
      !ReadBoundedRetailAttributeSource("..\\SYSF.SCI", &sysfSource)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_SOURCE_UNAVAILABLE |
            RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_SOURCE_UNAVAILABLE,
        "could not read Level-local PEOPLE.SCI/set_people.sci/units.sci/SYSF.SCI");
    return false;
  }
  if (!InspectPeopleScripts(attributeSource, subjectSource, unitsSource,
                            sysfSource, summary)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_ROSTER_INVALID |
            RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_ROSTER_INVALID,
        "Level-local People table/call shape is invalid");
    return false;
  }
  return true;
}

bool RunPeopleAttributeBootstrap(SimulationContext* context,
                                 double startTime) {
  std::string peopleSource;
  std::string unitsSource;
  std::string sysfSource;
  if (!ReadBoundedRetailAttributeSource("SCINC\\PEOPLE.SCI",
                                        &peopleSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\units.sci", &unitsSource) ||
      !ReadBoundedRetailAttributeSource("..\\SYSF.SCI", &sysfSource)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_SOURCE_UNAVAILABLE,
        "could not read bounded PEOPLE.SCI/units.sci/SYSF.SCI");
    return false;
  }
  std::string supportSource;
  if (!BuildRetailSupportClosure(peopleSource, std::string(), unitsSource,
                                 std::string(), sysfSource,
                                 &supportSource)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_SOURCE_UNAVAILABLE,
        "could not build bounded People attribute support closure");
    return false;
  }
  std::string program;
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    supportSource.size() + peopleSource.size() +
                    sizeof(kPeopleAttributeBootstrapSuffix) + 3u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(supportSource);
    program.append(peopleSource);
    program.push_back('\n');
    program.append(kPeopleAttributeBootstrapSuffix);
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate bounded People attribute bootstrap source");
    return false;
  }
  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kPeopleAttributeProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool ReadHowitzerScriptSummary(HowitzerScriptSummary* summary) {
  std::string attributeSource;
  std::string subjectSource;
  if (summary == nullptr ||
      !ReadBoundedRetailAttributeSource("SCINC\\HOWITZER.SCI",
                                        &attributeSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\set_howitzers.sci",
                                        &subjectSource)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "could not read bounded Howitzer scripts");
    return false;
  }
  if (!InspectHowitzerScripts(attributeSource, subjectSource, summary)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "Howitzer table declarations are malformed");
    return false;
  }
  return true;
}

bool RunHowitzerBootstrap(SimulationContext* context, double startTime,
                          const HowitzerScriptSummary& script) {
  // Level.07N reuses these filenames for its Destroyable shield roster and
  // declares neither Howitzer table.  It is not a zero-sized Howitzer owner.
  if (script.attributeCapacity == 0 && script.subjectCapacity == 0)
    return true;

  std::string attributeSource;
  std::string subjectSource;
  std::string unitsSource;
  std::string sysSource;
  std::string sysfSource;
  if (!ReadBoundedRetailAttributeSource("SCINC\\HOWITZER.SCI",
                                        &attributeSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\set_howitzers.sci",
                                        &subjectSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\units.sci", &unitsSource) ||
      !ReadBoundedRetailAttributeSource("..\\SYS.SCI", &sysSource) ||
      !ReadBoundedRetailAttributeSource("..\\SYSF.SCI", &sysfSource)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "could not read the retail Howitzer support closure");
    return false;
  }

  std::string ownedAttributeSource;
  if (!BuildOwnedTableScript(attributeSource, "main_CreateHowitzerAttrs",
                             "HowitzerAttr", &ownedAttributeSource)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "could not isolate the retail Howitzer-owned table bootstrap");
    return false;
  }

  std::string supportSource;
  if (!BuildRetailSupportClosure(ownedAttributeSource, subjectSource,
                                 unitsSource, sysSource, sysfSource,
                                 &supportSource)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "could not build the retail Howitzer support closure");
    return false;
  }

  std::string program;
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    supportSource.size() + ownedAttributeSource.size() +
                    subjectSource.size() +
                    sizeof(kHowitzerBootstrapSuffix) + 4u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(supportSource);
    program.append(ownedAttributeSource);
    program.push_back('\n');
    program.append(subjectSource);
    program.push_back('\n');
    program.append(kHowitzerBootstrapSuffix);
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate the retail Howitzer support closure");
    return false;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kHowitzerProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool RunPeopleSubjectBootstrap(SimulationContext* context,
                               double startTime) {
  std::string peopleSource;
  std::string subjectSource;
  std::string unitsSource;
  std::string sysfSource;
  if (!ReadBoundedRetailAttributeSource("SCINC\\PEOPLE.SCI", &peopleSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\set_people.sci",
                                        &subjectSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\units.sci", &unitsSource) ||
      !ReadBoundedRetailAttributeSource("..\\SYSF.SCI", &sysfSource)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_SOURCE_UNAVAILABLE,
        "could not read bounded PEOPLE.SCI/set_people.sci/units.sci/SYSF.SCI");
    return false;
  }

  std::string supportSource;
  if (!BuildRetailSupportClosure(peopleSource, subjectSource, unitsSource,
                                 std::string(), sysfSource,
                                 &supportSource)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_SOURCE_UNAVAILABLE,
        "could not build bounded People support-function closure");
    return false;
  }

  std::string program;
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    supportSource.size() + peopleSource.size() +
                    subjectSource.size() +
                    sizeof(kPeopleSubjectBootstrapSuffix) + 4u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(supportSource);
    program.append(peopleSource);
    program.push_back('\n');
    program.append(subjectSource);
    program.push_back('\n');
    program.append(kPeopleSubjectBootstrapSuffix);
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate bounded People subject bootstrap source");
    return false;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kPeopleSubjectProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool ReadTankCannonScriptSummary(TankCannonScriptSummary* summary) {
  std::string attributeSource;
  std::string localMainSource;
  std::string setTankSource;
  if (summary == nullptr ||
      !ReadBoundedRetailAttributeSource("SCINC\\TANK.SCI",
                                        &attributeSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\localmain.sci",
                                        &localMainSource) ||
      !ReadBoundedRetailAttributeSource("SCINC\\set_tank.sci",
                                        &setTankSource)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SOURCE_UNAVAILABLE,
        "could not read Level-local TANK.SCI/localmain.sci/set_tank.sci");
    return false;
  }
  if (!InspectTankCannonScripts(attributeSource, localMainSource,
                                setTankSource, summary)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_ATTRIBUTE_ROSTER_INVALID |
            RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SUBJECT_TABLE_FAILURE,
        "Level-local Tank/Cannon table shape is invalid");
    return false;
  }
  return true;
}

bool ReadCommanderScriptSummary(CommanderScriptSummary* summary) {
  std::string localMainSource;
  if (summary == nullptr ||
      !ReadBoundedRetailAttributeSource("SCINC\\localmain.sci",
                                        &localMainSource) ||
      !InspectCommanderScript(localMainSource, summary)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_COMMANDER_SOURCE_OR_ROSTER_INVALID,
        "could not inspect Level-local Commander ownership");
    return false;
  }
  return true;
}

bool RunCommanderBootstrap(SimulationContext* context, double startTime,
                           const CommanderScriptSummary& script) {
  std::string program;
  std::string commanderFunction = script.functionSource;
  std::size_t statement = 0;
  while ((statement = commanderFunction.find("s_AddClassTable", statement)) !=
         std::string::npos) {
    const std::size_t end = commanderFunction.find(';', statement);
    if (end == std::string::npos) break;
    const std::string call =
        commanderFunction.substr(statement, end - statement + 1);
    if (call.find("\"TankGroup\"") != std::string::npos ||
        call.find("\"Tank\"") != std::string::npos) {
      for (std::size_t index = statement; index <= end; ++index)
        if (commanderFunction[index] != '\r' &&
            commanderFunction[index] != '\n')
          commanderFunction[index] = ' ';
    }
    statement = end + 1;
  }
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    commanderFunction.size() + 96u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(commanderFunction);
    program.append("\nfunc void main()\n{\n"
                   "  local_createCommanders();\n"
                   "}\n");
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate Commander bootstrap source");
    return false;
  }
  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kCommanderProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool BuildMissionTankSupportSource(std::string* supportSource) {
  std::string sysSource;
  std::string sysfSource;
  if (supportSource == nullptr ||
      !ReadBoundedRetailAttributeSource("..\\SYS.SCI", &sysSource) ||
      !ReadBoundedRetailAttributeSource("..\\SYSF.SCI", &sysfSource))
    return false;
  std::map<std::string, ScriptFunctionDefinition> available;
  ParseScriptFunctions(sysSource, &available, true);
  ParseScriptFunctions(sysfSource, &available, true);
  const char* rootNames[] = {"CreateGroup", "CreateUnit"};
  const std::set<std::string> alreadyDefined = {"ChangeObjectAttrN", "New"};
  std::set<std::string> emitted;
  std::set<std::string> visiting;
  supportSource->clear();
  for (int index = 0; index < 2; ++index)
    if (!AppendScriptFunctionClosure(rootNames[index], available,
                                     alreadyDefined, &emitted, &visiting,
                                     supportSource))
      return false;
  return true;
}

bool LevelHasRetailAER00TankSpawn() {
  std::string source;
  std::string compact;
  if (!ReadBoundedRetailAttributeSource("BRIEF\\AER00.SC", &source))
    return false;
  return CompactScriptSource(source, &compact) &&
         compact.find("comName:=\"Colony\";") != std::string::npos &&
         compact.find("groupName:=\"C.Group.aer00.00\";") !=
             std::string::npos &&
         compact.find("CreateGroup(groupName,comName);") !=
             std::string::npos &&
         compact.find(
             "CreateUnit(\"C.Unit.aer00.00\",groupName,"
             "\"TankLevEnglAttr\");") != std::string::npos;
}

bool RunRetailAER00TankSpawn(SimulationContext* context, double startTime) {
  std::string supportSource;
  std::string program;
  if (!BuildMissionTankSupportSource(&supportSource)) return false;
  try {
    program.reserve(sizeof(kRetailAttributeBootstrapPrefix) +
                    supportSource.size() + 256u);
    program.append(kRetailAttributeBootstrapPrefix);
    program.append(supportSource);
    program.append(
        "\nfunc void main()\n{\n"
        "  CreateGroup(\"C.Group.aer00.00\", \"Colony\");\n"
        "  CreateUnit(\"C.Unit.aer00.00\", "
        "\"C.Group.aer00.00\", \"TankLevEnglAttr\");\n"
        "}\n");
  } catch (...) {
    return false;
  }
  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kMissionTankProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool RunTankCannonAttributeBootstrap(SimulationContext* context,
                                     double startTime) {
  return RunRetailAttributeBootstrap(
      context, startTime, "SCINC\\TANK.SCI", nullptr,
      kTankCannonAttributeBootstrapSuffix, kTankCannonAttributeProgramName,
      RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SOURCE_UNAVAILABLE,
      "SCINC\\TANK.SCI", true);
}

bool InitializeTankCannonSubjectTables(
    SimulationContext* context, const TankCannonScriptSummary& script) {
  if (context == nullptr ||
      g_arena.addClassTable("TankGroup", script.tankGroupSubjectCapacity) ==
          ct_NULLID ||
      g_arena.addClassTable("Cannon", script.cannonSubjectCapacity) ==
          ct_NULLID ||
      g_arena.addClassTable("Tank", script.tankSubjectCapacity) == ct_NULLID) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SUBJECT_TABLE_FAILURE,
        "could not create exact Level-local Cannon/Tank subject tables");
    return false;
  }
  CannonSubjectState_SetExpectedCapacities(
      script.cannonAttributeCapacity, script.cannonSubjectCapacity);
  TankSubjectState_SetExpectedCapacities(
      script.tankAttributeCapacity, script.tankSubjectCapacity);
  return true;
}

bool RunWavMetadataBootstrap(SimulationContext* context, double startTime,
                             SRecoveredWavMetadataCatalog* catalog) {
  SRecoveredWavMetadataCatalogResult catalogResult = {};
  if (!RecoveredWavMetadataCatalog_Load(".", catalog, &catalogResult)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "WAV metadata preflight failed (issues=%u): %.190s",
                  catalogResult.issues, catalogResult.error);
    const unsigned long long issue =
        (catalogResult.issues &
         (RECOVERED_WAV_CATALOG_SOURCE_UNAVAILABLE |
          RECOVERED_WAV_CATALOG_SOURCE_TOO_LARGE)) != 0
            ? RECOVERED_ARENA_SEANCE_WAV_SOURCE_UNAVAILABLE
            : RECOVERED_ARENA_SEANCE_WAV_CATALOG_INVALID;
    Report(issue, message);
    return false;
  }
  if (!RecoveredWavMetadataCatalog_IsKnown(catalog)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "WAV metadata is not a bounded retail roster "
                  "(count=%d capacity=%d fingerprint=%llu)",
                  catalog->entryCount, catalog->capacity,
                  catalog->fingerprint);
    Report(RECOVERED_ARENA_SEANCE_WAV_ROSTER_INVALID, message);
    return false;
  }

  std::string source;
  if (!ReadBoundedRetailAttributeSource("SCINC\\LOADWAV.SCI", &source)) {
    Report(RECOVERED_ARENA_SEANCE_WAV_SOURCE_UNAVAILABLE,
           "could not read bounded retail SCINC\\LOADWAV.SCI");
    return false;
  }
  char suffix[256] = {};
  std::snprintf(suffix, sizeof(suffix),
                "\nfunc void main()\n"
                " var int classTableID;\n"
                "{\n"
                " classTableID := s_AddClassTable(\"WAVObj\",%d);\n"
                " LoadAllWaves(classTableID);\n"
                "}\n",
                catalog->capacity);
  std::string program;
  try {
    program.reserve(sizeof(kWavMetadataBootstrapPrefix) + source.size() +
                    std::strlen(suffix) + 2u);
    program.append(kWavMetadataBootstrapPrefix);
    program.append(source);
    program.append(suffix);
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "could not allocate bounded WAV metadata bootstrap source");
    return false;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kWavMetadataProgramName, profile, context,
          startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
  return true;
}

bool OpenArena(SimulationContext* context) {
  try {
    g_arena.openSeance(context, kSceneWidth, kSceneDepth);
    g_state.arenaOpen = g_arena.getContext() == context &&
                        context->isExist("Storage");
  } catch (const std::bad_alloc&) {
    RollBackPartiallyOpenedArena(context);
    Report(RECOVERED_ARENA_SEANCE_OPEN_FAILURE,
           "Arena allocation failed");
    return false;
  } catch (...) {
    RollBackPartiallyOpenedArena(context);
    Report(RECOVERED_ARENA_SEANCE_OPEN_FAILURE, "Arena open failed");
    return false;
  }

  if (!g_state.arenaOpen) {
    Report(RECOVERED_ARENA_SEANCE_OPEN_FAILURE,
           "Arena did not publish Storage");
    RecoveredArenaSeance_Release();
    return false;
  }
  return true;
}

bool InitializeDeviceFreeSoundDistance() {
  g_state.previousSoundDistance = snd_distMax;
  g_state.previousSoundDistanceSquared = snd_distMax2;
  if (!SoundState_SetMaximumDistance(kDeviceFreeSoundDistance)) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "device-free audible distance initialization failed");
    return false;
  }
  g_state.soundDistance = snd_distMax;
  g_state.soundDistanceSquared = snd_distMax2;
  g_state.soundDistanceReady =
      snd_distMax == kDeviceFreeSoundDistance &&
      snd_distMax2 == kDeviceFreeSoundDistance * kDeviceFreeSoundDistance;
  if (!g_state.soundDistanceReady) {
    snd_distMax = g_state.previousSoundDistance;
    snd_distMax2 = g_state.previousSoundDistanceSquared;
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "device-free audible distance did not publish atomically");
    return false;
  }
  return true;
}

bool InitializeCorpseSubjectTable(SimulationContext* context) {
  if (!CorpseSubjectState_CreateTable(context, kCorpseSubjectCapacity) ||
      !CorpseSubjectState_TableReady(context, kCorpseSubjectCapacity) ||
      CorpseSubjectState_LiveCount() != 0) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_CORPSE_SUBJECT_TABLE_FAILURE,
                   "could not create the empty retail Corpse(100) table");
    return false;
  }
  g_state.corpseSubjectCapacity = CorpseSubjectState_Capacity();
  g_state.corpseSubjectFingerprint =
      CorpseSubjectState_Fingerprint(context);
  if (g_state.corpseSubjectFingerprint == 0) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_CORPSE_SUBJECT_TABLE_FAILURE,
                   "retail Corpse table did not publish an empty stable pool");
    return false;
  }
  g_state.corpseSubjectReady = true;
  return true;
}

bool InitializeOrphanSubjectTable(SimulationContext* context) {
  if (!OrphanSubjectState_CreateTable(context, kOrphanSubjectCapacity) ||
      !OrphanSubjectState_TableReady(context, kOrphanSubjectCapacity) ||
      OrphanSubjectState_LiveCount() != 0 ||
      !OrphanSubjectState_AllReady(context)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ORPHAN_SUBJECT_TABLE_FAILURE,
                   "could not create the empty retail Orphan(5) table");
    return false;
  }
  g_state.orphanSubjectCapacity = OrphanSubjectState_Capacity();
  g_state.orphanSubjectFingerprint =
      OrphanSubjectState_Fingerprint(context);
  if (g_state.orphanSubjectFingerprint == 0) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ORPHAN_SUBJECT_TABLE_FAILURE,
                   "retail Orphan table did not publish a stable empty pool");
    return false;
  }
  g_state.orphanSubjectReady = true;
  return true;
}

bool PublishVehicleAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_TABLE_MISSING,
                   "retail fragment did not create the VehicleAttr table");
    return false;
  }
  const int count = VehicleAttributeState_RosterSize(context);
  const int capacity = VehicleAttributeState_Capacity();
  const unsigned long long fingerprint =
      VehicleAttributeState_Fingerprint(context);
  if (!VehicleAttributeState_CachesUnresolved(context) ||
      !RecoveredGameplayTuning_AcceptsVehicleRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "VehicleAttr objects do not match a bounded unresolved "
                  "Level roster (capacity=%d count=%d fingerprint=%llu)",
                  capacity, count, fingerprint);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_VEHICLE_ATTRIBUTE_ROSTER_INVALID,
                   message);
    return false;
  }
  g_state.vehicleAttributeCount = count;
  g_state.vehicleAttributeCapacity = capacity;
  g_state.vehicleAttributeFingerprint = fingerprint;
  g_state.vehicleAttributesReady = true;
  return true;
}

bool PublishVehicle(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_VEHICLE_TABLE_MISSING,
           "script did not create the Vehicle tables");
    return false;
  }

  KR_ObjectID vehicleID = context->searchObject("Vehicle.Default");
  if (vehicleID.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_VEHICLE_OBJECT_MISSING,
           "script did not create Vehicle.Default");
    return false;
  }

  g_vehicle = static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  if (g_vehicle == nullptr) {
    Report(RECOVERED_ARENA_SEANCE_VEHICLE_INTERFACE_MISSING,
           "Vehicle.Default does not expose IVehicleIID");
    return false;
  }

  if (!g_state.vehicleReferencesReady ||
      !VehicleAttributeState_ReferencesResolved(context)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_VEHICLE_REFERENCE_INVALID,
                   "Vehicle.Default reached publication before its "
                   "attribute reference graph");
    return false;
  }

  std::vector<unsigned char> vehicleState;
  const unsigned long long vehicleFingerprint =
      VehicleActiveWorldState_Fingerprint(context);
  if (vehicleFingerprint == 0 ||
      VehicleActiveWorldState_LiveCount(context) != 1 ||
      !VehicleActiveWorldState_CaptureStable(context, &vehicleState) ||
      !VehicleActiveWorldState_ValidateStable(vehicleState)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_ENVELOPE_FAILURE,
                   "Vehicle.Default could not enter the active-world envelope");
    return false;
  }

  std::vector<KR_ObjectID> removed(1, vehicleID);
  VehicleActiveWorldState_RemoveStableOwners(context, &removed);
  if (g_vehicle != nullptr || VehicleActiveWorldState_LiveCount(context) != 0) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE,
                   "Vehicle.Default owner teardown left live runtime state");
    return false;
  }

  std::vector<KR_ObjectID> staged;
  if (!VehicleActiveWorldState_CreateStableOwners(context, vehicleState,
                                                   &staged) ||
      staged.size() != 1 || staged.front() == vehicleID) {
    VehicleActiveWorldState_RemoveStableOwners(context, &staged);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE,
                   "Vehicle.Default staging did not allocate a fresh owner");
    return false;
  }
  const KR_ObjectID stagedID = staged.front();
  VehicleActiveWorldState_RemoveStableOwners(context, &staged);
  if (g_vehicle != nullptr || VehicleActiveWorldState_LiveCount(context) != 0 ||
      context->isExist(stagedID)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE,
                   "Vehicle.Default staged-owner rollback leaked state");
    return false;
  }

  std::vector<KR_ObjectID> restored;
  if (!VehicleActiveWorldState_CreateStableOwners(context, vehicleState,
                                                   &restored) ||
      restored.size() != 1 || restored.front() == vehicleID ||
      restored.front() == stagedID ||
      !VehicleActiveWorldState_ApplyStableReferences(context, vehicleState) ||
      !VehicleActiveWorldState_MatchesStable(context, vehicleState) ||
      VehicleActiveWorldState_Fingerprint(context) != vehicleFingerprint) {
    VehicleActiveWorldState_RemoveStableOwners(context, &restored);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE,
                   "Vehicle.Default did not survive fresh owner restoration");
    return false;
  }

  vehicleID = restored.front();
  g_vehicle = static_cast<Vehicle*>(
      context->queryInterface(vehicleID, IVehicleIID));
  if (g_vehicle == nullptr || g_vehicle->getObjectID() != vehicleID) {
    VehicleActiveWorldState_RemoveStableOwners(context, &restored);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE,
                   "restored Vehicle.Default did not publish IVehicleIID");
    return false;
  }
  g_state.vehicleActiveWorldReconstructedIDs = 1;
  g_state.vehicleActiveWorldRollbacks = 1;
  g_state.vehicleActiveWorldFingerprint = vehicleFingerprint;

  const double mass = g_vehicle->VesselMass();
  const CFVector3 speedBefore = g_vehicle->Speed();
  if (!std::isfinite(mass) || mass <= 0.0 ||
      !g_vehicle->ApplyExplosionImpulse(CFVector3(0.0, 0.0, 0.0), 5.0)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_IMPULSE_BINDING_FAILURE,
        "Vehicle.Default did not expose a finite positive vessel mass");
    return false;
  }
  const CFVector3 speedAfter = g_vehicle->Speed();
  if (speedBefore.x != speedAfter.x || speedBefore.y != speedAfter.y ||
      speedBefore.z != speedAfter.z ||
      !ExplosionSubjectState_BindImpulseTarget(
          context, vehicleID, g_vehicle,
          [](void* user, const CFVector3& impulse, double factor) -> bool {
            Vehicle* vehicle = static_cast<Vehicle*>(user);
            return vehicle != nullptr &&
                   vehicle->ApplyExplosionImpulse(impulse, factor);
          }) ||
      !ExplosionSubjectState_ImpulseTargetReady(context, vehicleID)) {
    ExplosionSubjectState_UnbindImpulseTarget(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_IMPULSE_BINDING_FAILURE,
        "Vehicle.Default Explosion impulse dispatch did not bind atomically");
    return false;
  }

  g_state.explosionImpulseReady = true;
  g_state.vehicleReady = true;
  return true;
}

bool PublishBulletActiveWorld(SimulationContext* context,
                              double startTime) {
  const char* attribute = BulletAttributeState_FirstAttributeName(context);
  KR_ObjectID master = context == nullptr
      ? KR_ObjectID::NUL()
      : context->searchObject("Vehicle.Default");
  BulletActiveWorldProbeSummary summary = {};
  if (!g_state.vehicleReady || !g_state.bulletSubjectReady ||
      attribute == nullptr || master.isNUL() ||
      !BulletActiveWorldState_ProbeFlightRoundTrip(
          context, attribute, master, startTime, &summary) ||
      summary.capturedOwners != 1 || summary.schedulerEvents != 2 ||
      summary.stagedRollbacks != 1 || summary.reconstructedOwners != 1 ||
      summary.stableRoundTrips != 2 || summary.resumedMoves != 1 ||
      summary.tombstonedMasters != 1 ||
      summary.fingerprint == 0 ||
      BulletSubjectState_LiveCount() != 0) {
    char message[320] = {};
    std::snprintf(message, sizeof(message),
                  "BUL1 flight owners/events/rollback/recreated/roundtrips/"
                  "resumed/tombstones/fingerprint="
                  "%d/%d/%d/%d/%d/%d/%d/%llu: %.140s",
                  summary.capturedOwners, summary.schedulerEvents,
                  summary.stagedRollbacks, summary.reconstructedOwners,
                  summary.stableRoundTrips, summary.resumedMoves,
                  summary.tombstonedMasters,
                  summary.fingerprint,
                  BulletActiveWorldState_LastFailure());
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_BULLET_ACTIVE_WORLD_FAILURE,
                   message);
    return false;
  }
  g_state.bulletActiveWorldCapturedOwners = summary.capturedOwners;
  g_state.bulletActiveWorldSchedulerEvents = summary.schedulerEvents;
  g_state.bulletActiveWorldRollbacks = summary.stagedRollbacks;
  g_state.bulletActiveWorldReconstructedIDs =
      summary.reconstructedOwners;
  g_state.bulletActiveWorldStableRoundTrips = summary.stableRoundTrips;
  g_state.bulletActiveWorldResumedMoves = summary.resumedMoves;
  g_state.bulletActiveWorldTombstonedMasters =
      summary.tombstonedMasters;
  g_state.bulletActiveWorldFingerprint = summary.fingerprint;
  g_state.bulletActiveWorldReady = true;
  return true;
}

bool PublishExplosionActiveWorld(SimulationContext* context,
                                 double startTime) {
  const char* attribute =
      ExplosionSubjectState_SoundProbeAttributeName(context);
  ExplosionActiveWorldProbeSummary summary = {};
  if (!g_state.vehicleReady || !g_state.explosionSubjectReady ||
      !g_state.explosionSoundReady || !g_state.explosionParticlesReady ||
      attribute == nullptr ||
      !ExplosionActiveWorldState_ProbeLiveRoundTrip(
          context, attribute, startTime, &summary) ||
      summary.capturedOwners != 1 || summary.capturedBranches <= 0 ||
      summary.schedulerEvents < 1 || summary.schedulerEvents > 2 ||
      summary.soundChildren != 1 || summary.stagedRollbacks != 1 ||
      summary.reconstructedOwners != 1 ||
      summary.stableRoundTrips != 2 || summary.resumedMoves != 1 ||
      summary.fingerprint == 0 ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0 ||
      ExplosionSubjectState_TracedParentCount() != 0) {
    char message[384] = {};
    std::snprintf(
        message, sizeof(message),
        "EXP1 owners/branches/events/sounds/rollback/recreated/roundtrips/"
        "resumed/fingerprint=%d/%d/%d/%d/%d/%d/%d/%d/%llu: %.140s",
        summary.capturedOwners, summary.capturedBranches,
        summary.schedulerEvents, summary.soundChildren,
        summary.stagedRollbacks, summary.reconstructedOwners,
        summary.stableRoundTrips, summary.resumedMoves,
        summary.fingerprint, ExplosionActiveWorldState_LastFailure());
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_ACTIVE_WORLD_FAILURE,
        message);
    return false;
  }
  g_state.explosionActiveWorldCapturedOwners = summary.capturedOwners;
  g_state.explosionActiveWorldCapturedBranches = summary.capturedBranches;
  g_state.explosionActiveWorldSchedulerEvents = summary.schedulerEvents;
  g_state.explosionActiveWorldSoundChildren = summary.soundChildren;
  g_state.explosionActiveWorldRollbacks = summary.stagedRollbacks;
  g_state.explosionActiveWorldReconstructedIDs =
      summary.reconstructedOwners;
  g_state.explosionActiveWorldStableRoundTrips = summary.stableRoundTrips;
  g_state.explosionActiveWorldResumedMoves = summary.resumedMoves;
  g_state.explosionActiveWorldFingerprint = summary.fingerprint;
  g_state.explosionActiveWorldReady = true;
  return true;
}

bool PublishSparkActiveWorld(SimulationContext* context,
                             double startTime) {
  // The January source-only fixture has no sprite resources. Its canonical
  // empty SPK1 section is still part of the envelope, but the live phase
  // reconstruction proof can only run after Spark.Flash resolves its sprite.
  if (!g_state.sparkVisualResourcesReady) return true;
  SparkActiveWorldProbeSummary summary = {};
  if (!g_state.sparkSubjectReady ||
      !SparkActiveWorldState_ProbeLiveRoundTrip(
          context, startTime, &summary) ||
      summary.capturedOwners != 2 || summary.schedulerEvents != 2 ||
      summary.stagedRollbacks != 1 || summary.reconstructedOwners != 2 ||
      summary.stableRoundTrips != 2 || summary.resumedPhases != 1 ||
      summary.fingerprint == 0 || SparkSubjectState_LiveCount() != 0) {
    char message[320] = {};
    std::snprintf(
        message, sizeof(message),
        "SPK1 owners/events/rollback/recreated/roundtrips/resumed/"
        "fingerprint=%d/%d/%d/%d/%d/%d/%llu: %.140s",
        summary.capturedOwners, summary.schedulerEvents,
        summary.stagedRollbacks, summary.reconstructedOwners,
        summary.stableRoundTrips, summary.resumedPhases,
        summary.fingerprint, SparkActiveWorldState_LastFailure());
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SPARK_ACTIVE_WORLD_FAILURE, message);
    return false;
  }
  g_state.sparkActiveWorldCapturedOwners = summary.capturedOwners;
  g_state.sparkActiveWorldSchedulerEvents = summary.schedulerEvents;
  g_state.sparkActiveWorldRollbacks = summary.stagedRollbacks;
  g_state.sparkActiveWorldReconstructedIDs = summary.reconstructedOwners;
  g_state.sparkActiveWorldStableRoundTrips = summary.stableRoundTrips;
  g_state.sparkActiveWorldResumedPhases = summary.resumedPhases;
  g_state.sparkActiveWorldFingerprint = summary.fingerprint;
  g_state.sparkActiveWorldReady = true;
  return true;
}

bool PublishSmokeActiveWorld(SimulationContext* context,
                             double startTime) {
  // The source-only fixture deliberately has no Smoke sprite resources. Its
  // canonical empty SMK1 section is still admitted by the envelope; the live
  // two-owner blob reconstruction proof is retail-only.
  if (!g_state.smokeVisualResourcesReady) return true;
  SmokeActiveWorldProbeSummary summary = {};
  if (!g_state.smokeSubjectReady ||
      !SmokeActiveWorldState_ProbeLiveRoundTrip(
          context, "Smoke.Attr.Trace", startTime, &summary) ||
      summary.capturedOwners != 2 || summary.capturedBlobs != 2 ||
      summary.schedulerEvents != 2 || summary.stagedRollbacks != 1 ||
      summary.reconstructedOwners != 2 ||
      summary.stableRoundTrips != 2 || summary.resumedMoves != 1 ||
      summary.fingerprint == 0 || SmokeSubjectState_LiveCount() != 0) {
    char message[352] = {};
    std::snprintf(
        message, sizeof(message),
        "SMK1 owners/blobs/events/rollback/recreated/roundtrips/resumed/"
        "fingerprint=%d/%d/%d/%d/%d/%d/%d/%llu: %.140s",
        summary.capturedOwners, summary.capturedBlobs,
        summary.schedulerEvents, summary.stagedRollbacks,
        summary.reconstructedOwners, summary.stableRoundTrips,
        summary.resumedMoves, summary.fingerprint,
        SmokeActiveWorldState_LastFailure());
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SMOKE_ACTIVE_WORLD_FAILURE, message);
    return false;
  }
  g_state.smokeActiveWorldCapturedOwners = summary.capturedOwners;
  g_state.smokeActiveWorldCapturedBlobs = summary.capturedBlobs;
  g_state.smokeActiveWorldSchedulerEvents = summary.schedulerEvents;
  g_state.smokeActiveWorldRollbacks = summary.stagedRollbacks;
  g_state.smokeActiveWorldReconstructedIDs = summary.reconstructedOwners;
  g_state.smokeActiveWorldStableRoundTrips = summary.stableRoundTrips;
  g_state.smokeActiveWorldResumedMoves = summary.resumedMoves;
  g_state.smokeActiveWorldFingerprint = summary.fingerprint;
  g_state.smokeActiveWorldReady = true;
  return true;
}

bool PublishCorpseActiveWorld(SimulationContext* context,
                              double startTime) {
  // The source-only fixture has no resolved Corpse skin graph. It still owns
  // a canonical empty COR1 section; the live parent/child proof is retail-only.
  if (!g_state.corpseRuntimeReady) return true;
  CorpseActiveWorldProbeSummary summary = {};
  if (!g_state.corpseSubjectReady || !g_state.dynSmokerReady ||
      !g_state.smokeSubjectReady ||
      !CorpseActiveWorldState_ProbeLiveRoundTrip(
          context, startTime, &summary) ||
      summary.capturedCorpses != 2 || summary.ownedSmokers != 4 ||
      summary.schedulerEvents < 6 || summary.schedulerEvents > 10 ||
      summary.stagedRollbacks != 1 ||
      summary.reconstructedObjects != 6 ||
      summary.stableRoundTrips != 2 || summary.resumedEmissions != 1 ||
      summary.resumedDeaths != 1 || summary.fingerprint == 0 ||
      CorpseSubjectState_LiveCount() != 0 ||
      SmokerSubjectState_DynLiveCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0) {
    char message[416] = {};
    std::snprintf(
        message, sizeof(message),
        "COR1 corpses/smokers/events/rollback/recreated/roundtrips/"
        "emissions/deaths/fingerprint=%d/%d/%d/%d/%d/%d/%d/%d/%llu: "
        "%.140s",
        summary.capturedCorpses, summary.ownedSmokers,
        summary.schedulerEvents, summary.stagedRollbacks,
        summary.reconstructedObjects, summary.stableRoundTrips,
        summary.resumedEmissions, summary.resumedDeaths,
        summary.fingerprint, CorpseActiveWorldState_LastFailure());
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_CORPSE_ACTIVE_WORLD_FAILURE, message);
    return false;
  }
  g_state.corpseActiveWorldCapturedOwners = summary.capturedCorpses;
  g_state.corpseActiveWorldOwnedSmokers = summary.ownedSmokers;
  g_state.corpseActiveWorldSchedulerEvents = summary.schedulerEvents;
  g_state.corpseActiveWorldRollbacks = summary.stagedRollbacks;
  g_state.corpseActiveWorldReconstructedObjects =
      summary.reconstructedObjects;
  g_state.corpseActiveWorldStableRoundTrips = summary.stableRoundTrips;
  g_state.corpseActiveWorldResumedEmissions = summary.resumedEmissions;
  g_state.corpseActiveWorldResumedDeaths = summary.resumedDeaths;
  g_state.corpseActiveWorldFingerprint = summary.fingerprint;
  g_state.corpseActiveWorldReady = true;
  return true;
}

bool PublishRouteTable() {
  if (g_arena.searchSeanceClassTable("Route") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_ROUTE_TABLE_MISSING,
           "script did not create the Route table");
    return false;
  }
  g_state.routeReady = true;
  return true;
}

bool PublishSparkAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("SparkAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_SPARK_TABLE_MISSING,
           "script did not create the SparkAttr table");
    return false;
  }

  KR_ObjectID flash = context->searchObject("Spark.Flash");
  if (flash.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_SPARK_OBJECT_MISSING,
           "script did not create Spark.Flash");
    return false;
  }
  if (!SparkAttributeState_IsRetailFlash(flash)) {
    Report(RECOVERED_ARENA_SEANCE_SPARK_DEFAULT_INVALID,
           "Spark.Flash does not match the retail phase table");
    return false;
  }

  // The public January source-only fixture intentionally owns no SkinSpr
  // objects. Keep its structural Spark table available so later transactional
  // failure probes can run, but do not claim visual/lifecycle readiness.
  if (g_state.skinSpriteCount == 0) {
    g_state.sparkAttributesReady = true;
    g_state.sparkSubjectReady = true;
    return true;
  }

  if (!SparkAttributeState_ResolveVisualResources(context) ||
      !SparkAttributeState_VisualResourcesResolved(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SPARK_VISUAL_RESOURCE_FAILURE,
        "Spark.Flash could not resolve the loaded sk.Fusion.0 sprite");
    return false;
  }
  const unsigned long long visualFingerprint =
      SparkAttributeState_VisualResourceFingerprint(context);
  if (visualFingerprint == 0) {
    SparkAttributeState_ClearVisualResources(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SPARK_VISUAL_RESOURCE_FAILURE,
        "Spark.Flash visual resource did not publish a stable fingerprint");
    return false;
  }

  SparkLifecycleProbeSummary probe = {};
  if (!SparkSubjectState_ProbeLifecycle(
          context, Session::m_moment, &probe) ||
      probe.invalidStarts != 2 || probe.queuedCreates != 1 ||
      probe.queueRollbacks != 1 || probe.phaseTransitions != 5 ||
      probe.expirations != 1 || SparkSubjectState_LiveCount() != 0) {
    SparkAttributeState_ClearVisualResources(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SPARK_SUBJECT_LIFECYCLE_FAILURE,
        "Spark validation/queue/May-phase/expiry lifecycle probe failed");
    return false;
  }
  const unsigned long long subjectFingerprint =
      SparkSubjectState_Fingerprint(context);
  if (subjectFingerprint == 0) {
    SparkAttributeState_ClearVisualResources(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SPARK_SUBJECT_LIFECYCLE_FAILURE,
        "Spark subject table did not return to a stable empty pool");
    return false;
  }

  g_state.sparkAttributesReady = true;
  g_state.sparkSubjectReady = true;
  g_state.sparkVisualResourcesReady = true;
  g_state.sparkSubjectFingerprint = subjectFingerprint;
  g_state.sparkVisualResourceFingerprint = visualFingerprint;
  g_state.sparkProbeInvalidStarts = probe.invalidStarts;
  g_state.sparkProbeQueuedCreates = probe.queuedCreates;
  g_state.sparkProbeQueueRollbacks = probe.queueRollbacks;
  g_state.sparkProbePhaseTransitions = probe.phaseTransitions;
  g_state.sparkProbeExpirations = probe.expirations;
  return true;
}

bool InitializeSparkSubjectTable(SimulationContext* context) {
  const ct_ClassTableID table =
      g_arena.addClassTable("Spark", kSparkSubjectCapacity);
  if (table == ct_NULLID ||
      !SparkSubjectState_TableReady(context, kSparkSubjectCapacity)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_SPARK_SUBJECT_TABLE_FAILURE,
                   "could not create the retail Spark(40) subject table");
    return false;
  }
  g_state.sparkSubjectCapacity = SparkSubjectState_Capacity();
  return true;
}

bool PublishBirdAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("BirdAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_BIRD_TABLE_MISSING,
           "script did not create the BirdAttr table");
    return false;
  }

  KR_ObjectID bird = context->searchObject("Bird.Attr.0");
  if (bird.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_BIRD_OBJECT_MISSING,
           "script did not create Bird.Attr.0");
    return false;
  }
  if (!BirdAttributeState_IsRetailDefault(bird)) {
    Report(RECOVERED_ARENA_SEANCE_BIRD_DEFAULT_INVALID,
           "Bird.Attr.0 does not match the retail common attribute");
    return false;
  }

  g_state.birdAttributesReady = true;
  return true;
}

bool PublishPortalTable() {
  if (g_arena.searchSeanceClassTable("Portal") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_PORTAL_TABLE_MISSING,
           "script did not create the Portal table");
    return false;
  }
  g_state.portalReady = true;
  return true;
}

bool PublishTeleportRoutes(SimulationContext* context, double startTime,
                           const TeleportScriptSummary& script) {
  g_state.teleportRoutesReady = true;
  g_state.teleportTargetLevel = script.capacity > 0;
  if (script.capacity == 0) return script.definitions.empty();

  if (!TeleportSubjectState_Initialize(
          context, script.capacity, script.definitions, startTime) ||
      !TeleportSubjectState_TableReady(context, script.capacity) ||
      TeleportSubjectState_LiveCount() !=
          static_cast<int>(script.definitions.size())) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE,
                   TeleportSubjectState_LastError());
    return false;
  }
  TeleportLifecycleProbeSummary probe = {};
  if (!TeleportSubjectState_ProbeLifecycle(context, startTime, &probe)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE,
                   TeleportSubjectState_LastError());
    return false;
  }
  const unsigned long long fingerprint =
      TeleportSubjectState_Fingerprint(context);
  if (fingerprint == 0) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE,
                   "Teleport route roster has no stable identity");
    return false;
  }

  g_state.teleportCapacity = TeleportSubjectState_Capacity();
  g_state.teleportRouteCount = TeleportSubjectState_LiveCount();
  g_state.teleportProbeRejectedNonPlayer =
      probe.rejectedNonPlayerCollisions;
  g_state.teleportProbePhysicsCollisions = probe.physicsCollisionEvents;
  g_state.teleportProbeAppliedPlayer = probe.appliedPlayerCollisions;
  g_state.teleportProbeVehicleRollbacks = probe.vehiclePoseRollbacks;
  g_state.teleportFingerprint = fingerprint;
  return true;
}

bool PublishOrphanAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("OrphanAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_ORPHAN_TABLE_MISSING,
           "script did not create the OrphanAttr table");
    return false;
  }

  KR_ObjectID orphan = context->searchObject("Orphan.Attr.Default");
  if (orphan.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_ORPHAN_OBJECT_MISSING,
           "script did not create Orphan.Attr.Default");
    return false;
  }
  if (!OrphanAttributeState_IsRetailDefault(orphan)) {
    Report(RECOVERED_ARENA_SEANCE_ORPHAN_DEFAULT_INVALID,
           "Orphan.Attr.Default does not match the retail common attribute");
    return false;
  }

  g_state.orphanAttributesReady = true;
  return true;
}

bool PublishArtefactAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("ArtefactAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_ARTEFACT_TABLE_MISSING,
           "script did not create the ArtefactAttr table");
    return false;
  }

  KR_ObjectID artefact = context->searchObject("Artefact.Attr.0");
  if (artefact.isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_ARTEFACT_OBJECT_MISSING,
           "script did not create Artefact.Attr.0");
    return false;
  }
  if (!ArtefactAttributeState_IsRetailDefault(artefact)) {
    Report(RECOVERED_ARENA_SEANCE_ARTEFACT_DEFAULT_INVALID,
           "Artefact.Attr.0 does not match the retail common attribute");
    return false;
  }

  g_state.artefactAttributesReady = true;
  return true;
}

bool PublishSmokeAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("SmokeAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_TABLE_MISSING,
           "retail fragment did not create the SmokeAttr table");
    return false;
  }
  if (context->searchObject("Smoke.Attr.Small").isNUL() ||
      context->searchObject("Smoke.Attr.Fire.Corpse").isNUL()) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_OBJECT_MISSING,
           "retail fragment did not create the complete SmokeAttr roster");
    return false;
  }
  if (!SmokeAttributeState_IsRetailRoster(context) ||
      !SmokeAttributeState_CachesUnresolved(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "SmokeAttr objects do not match unchanged retail "
                  "SMOKE.SCI (fingerprint=%llu)",
                  SmokeAttributeState_RetailFingerprint(context));
    Report(RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_ROSTER_INVALID,
           message);
    return false;
  }
  g_state.smokeAttributesReady = true;
  return true;
}

bool PublishSmokeSubject(SimulationContext* context) {
  const ct_ClassTableID table =
      g_arena.addClassTable("Smoke", kSmokeCapacity);
  if (table == ct_NULLID ||
      !SmokeSubjectState_TableReady(context, kSmokeCapacity)) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_SUBJECT_TABLE_FAILURE,
           "could not create the retail Smoke subject table");
    return false;
  }
  if (!SmokeSubjectState_ProbeLifecycle(context) ||
      !SmokeSubjectState_SimulationSupported(
          context, "Smoke.Attr.Trace") ||
      SmokeSubjectState_LiveCount() != 0) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_SUBJECT_LIFECYCLE_FAILURE,
           "Smoke simulation prerequisites are invalid");
    return false;
  }
  const unsigned long long fingerprint =
      SmokeSubjectState_Fingerprint(context);
  if (fingerprint == 0) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_SUBJECT_LIFECYCLE_FAILURE,
           "Smoke table did not return to an empty stable state");
    return false;
  }
  g_state.smokeSubjectCapacity = kSmokeCapacity;
  g_state.smokeSubjectFingerprint = fingerprint;
  g_state.smokeSubjectReady = true;
  return true;
}

bool PublishSmokeVisualResources(SimulationContext* context) {
  unsigned long long fingerprint = 0;
  const ESmokeVisualResourcePresence presence =
      SmokeVisualState_InspectResources(&fingerprint);
  if (presence == SMOKE_VISUAL_RESOURCES_NONE) {
    if (SmokerAttributeState_RosterSize(context) ==
        kSourceOnlySmokerAttributeCount) {
      g_state.smokerRuntimeReady = false;
      return true;
    }
    Report(RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_INVALID,
           "Smoke visual resource set is missing from a retail roster");
    return false;
  }
  if (presence != SMOKE_VISUAL_RESOURCES_COMPLETE) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_INVALID,
           presence == SMOKE_VISUAL_RESOURCES_PARTIAL
               ? "Smoke visual resource set is incomplete"
               : "Smoke visual resource set is invalid");
    return false;
  }
  if (fingerprint == 0 || !SmokeVisualState_Resolve(context) ||
      !SmokeVisualState_Ready(context)) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_LOAD_FAILURE,
           "Smoke visual resources could not be published atomically");
    return false;
  }
  g_state.smokeVisualResourceFingerprint =
      SmokeVisualState_Fingerprint(context);
  if (g_state.smokeVisualResourceFingerprint != fingerprint) {
    Report(RECOVERED_ARENA_SEANCE_SMOKE_VISUAL_RESOURCE_LOAD_FAILURE,
           "Smoke visual resource fingerprint changed during publication");
    return false;
  }
  g_state.smokeVisualResourcesReady = true;
  g_state.smokerRuntimeReady = SmokerAttributeState_RuntimeReady(context);
  return g_state.smokerRuntimeReady;
}

bool PublishExplosionSmokeVisualResources(SimulationContext* context) {
  unsigned long long fingerprint = 0;
  const EExplosionSmokeVisualResourcePresence presence =
      ExplosionAttributeState_InspectSmokeVisualResources(
          context, &fingerprint);
  if (presence == EXPLOSION_SMOKE_VISUAL_RESOURCES_NONE) {
    // The source-only parser fixture intentionally carries no bitmaps. A
    // retail seance has already published the common Smoke visuals here, so
    // missing Explosion resources in that case are an admission failure.
    if (!g_state.smokeVisualResourcesReady) return true;
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SMOKE_LIFECYCLE_FAILURE,
        "Explosion smoke sprite resources are missing from a visual roster");
    return false;
  }
  if (presence != EXPLOSION_SMOKE_VISUAL_RESOURCES_COMPLETE) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SMOKE_LIFECYCLE_FAILURE,
        presence == EXPLOSION_SMOKE_VISUAL_RESOURCES_PARTIAL
            ? "Explosion smoke sprite resource set is incomplete"
            : "Explosion smoke sprite resource set is invalid");
    return false;
  }
  if (fingerprint == 0 ||
      !ExplosionAttributeState_ProbeSmokeVisualAtomicity(context) ||
      !ExplosionAttributeState_ResolveSmokeVisuals(context) ||
      !ExplosionAttributeState_SmokeVisualsResolved(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SMOKE_LIFECYCLE_FAILURE,
        "Explosion smoke sprite resources could not be published atomically");
    return false;
  }
  g_state.explosionSmokeVisualFingerprint =
      ExplosionAttributeState_SmokeVisualFingerprint(context);
  if (g_state.explosionSmokeVisualFingerprint != fingerprint ||
      !ExplosionAttributeState_IsKnownSmokeVisualRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "Explosion smoke sprite roster is empty or unknown "
                  "(fingerprint=%llu)",
                  g_state.explosionSmokeVisualFingerprint);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SMOKE_LIFECYCLE_FAILURE,
        message);
    return false;
  }

  const char* probeAttribute =
      ExplosionSubjectState_SoundProbeAttributeName(context);
  ExplosionSmokeProbeSummary probe = {};
  if (probeAttribute == nullptr ||
      !ExplosionSubjectState_ProbeSmokeLifecycle(
          context, probeAttribute, Session::m_moment, &probe) ||
      probe.startedSprites <= 0 || probe.dependencyGateSkips != 1 ||
      probe.moveSteps <= 0 || probe.expiredParents != 1 ||
      probe.rolledBackSprites != probe.startedSprites ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SMOKE_LIFECYCLE_FAILURE,
        "Explosion smoke start/gate/move/expiry/rollback probe failed");
    return false;
  }
  g_state.explosionSmokeProbeStartedSprites = probe.startedSprites;
  g_state.explosionSmokeProbeDependencySkips =
      probe.dependencyGateSkips;
  g_state.explosionSmokeProbeMoveSteps = probe.moveSteps;
  g_state.explosionSmokeProbeExpiredParents = probe.expiredParents;
  g_state.explosionSmokeProbeRolledBackSprites =
      probe.rolledBackSprites;
  g_state.explosionSmokeReady = true;
  return true;
}

bool PublishSmokerAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("SmokerAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_TABLE_MISSING,
           "retail fragment did not create the SmokerAttr table");
    return false;
  }
  if (!SmokerAttributeState_IsKnownRoster(context) ||
      !SmokerAttributeState_CachesUnresolved(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "SmokerAttr objects do not match a bounded root roster "
                  "(count=%d capacity=%d fingerprint=%llu)",
                  SmokerAttributeState_RosterSize(context),
                  SmokerAttributeState_Capacity(),
                  SmokerAttributeState_Fingerprint(context));
    Report(RECOVERED_ARENA_SEANCE_SMOKER_ATTRIBUTE_ROSTER_INVALID, message);
    return false;
  }
  g_state.smokerAttributesReady = true;
  return true;
}

bool PublishDynSmokerSubject(SimulationContext* context, double startTime) {
  const ct_ClassTableID table =
      g_arena.addClassTable("DynSmoker", kDynSmokerCapacity);
  if (table == ct_NULLID ||
      !SmokerSubjectState_DynTableReady(context, kDynSmokerCapacity)) {
    Report(RECOVERED_ARENA_SEANCE_DYN_SMOKER_TABLE_FAILURE,
           "could not create the retail DynSmoker subject table");
    return false;
  }
  if (!SmokerSubjectState_ProbeDynLifecycle(
          context, "Smoker.Attr.Corpse", startTime) ||
      SmokerSubjectState_DynLiveCount() != 0) {
    Report(RECOVERED_ARENA_SEANCE_DYN_SMOKER_LIFECYCLE_FAILURE,
           "DynSmoker create/start/remove lifecycle probe failed");
    return false;
  }
  const unsigned long long fingerprint =
      SmokerSubjectState_DynFingerprint(context);
  if (fingerprint == 0) {
    Report(RECOVERED_ARENA_SEANCE_DYN_SMOKER_LIFECYCLE_FAILURE,
           "DynSmoker table did not return to an empty stable state");
    return false;
  }
  g_state.dynSmokerCapacity = kDynSmokerCapacity;
  g_state.dynSmokerFingerprint = fingerprint;
  g_state.dynSmokerReady = true;
  return true;
}

bool PublishExplosionAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("ExplosionAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Explosion") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_TABLE_MISSING,
           "retail fragments did not create the Explosion tables");
    return false;
  }
  if (!ExplosionAttributeState_IsKnownRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "ExplosionAttr objects do not match a bounded level "
                  "roster (count=%d fingerprint=%llu)",
                  ExplosionAttributeState_RosterSize(context),
                  ExplosionAttributeState_Fingerprint(context));
    Report(RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_ROSTER_INVALID,
           message);
    return false;
  }
  const int subjectCapacity = ExplosionSubjectState_Capacity();
  if (subjectCapacity < 2 ||
      !ExplosionSubjectState_TableReady(context, subjectCapacity)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SUBJECT_TABLE_FAILURE,
        "retail fragments did not create a bounded Explosion command table");
    return false;
  }
  const char* probeAttribute =
      ExplosionAttributeState_FirstAttributeName(context);
  ExplosionImpactProbeSummary probe = {};
  if (probeAttribute == nullptr ||
      !ExplosionSubjectState_ProbeLifecycle(
          context, probeAttribute, Session::m_moment, &probe) ||
      probe.invalidStarts != 2 || probe.allocationRollbacks != 1 ||
      probe.queuedCommands != 1 || probe.queueRollbacks != 1 ||
      probe.executedCommands != 1 || probe.damageApplications != 0 ||
      ExplosionSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SUBJECT_LIFECYCLE_FAILURE,
        "bounded Explosion validation/allocation/queue/execute probe failed");
    return false;
  }
  const char* lightProbeAttribute =
      ExplosionSubjectState_LightProbeAttributeName(context);
  if (!ExplosionSubjectState_LightRosterReady(context) ||
      lightProbeAttribute == nullptr ||
      !ExplosionSubjectState_ProbeLightLifecycle(
          context, lightProbeAttribute, Session::m_moment) ||
      ExplosionSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_LIGHT_LIFECYCLE_FAILURE,
        "Explosion m_useLight gate/publication/expiry probe failed");
    return false;
  }
  const unsigned long long subjectFingerprint =
      ExplosionSubjectState_Fingerprint(context);
  if (subjectFingerprint == 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SUBJECT_LIFECYCLE_FAILURE,
        "Explosion command table did not return to a stable empty pool");
    return false;
  }
  g_state.explosionSubjectCapacity = subjectCapacity;
  g_state.explosionSubjectFingerprint = subjectFingerprint;
  g_state.explosionProbeInvalidStarts = probe.invalidStarts;
  g_state.explosionProbeAllocationRollbacks = probe.allocationRollbacks;
  g_state.explosionProbeQueuedCommands = probe.queuedCommands;
  g_state.explosionProbeQueueRollbacks = probe.queueRollbacks;
  g_state.explosionProbeExecutedCommands = probe.executedCommands;
  g_state.explosionProbeDamageApplications = probe.damageApplications;
  g_state.explosionSubjectReady = true;
  g_state.explosionLightReady = true;
  g_state.explosionAttributesReady = true;
  return true;
}

bool PublishFarterAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("FarterAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_TABLE_MISSING,
           "retail fragments did not create the FarterAttr table");
    return false;
  }
  if (!FarterAttributeState_IsKnownRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "FarterAttr objects do not match a bounded level roster "
                  "(capacity=%d count=%d fingerprint=%llu)",
                  FarterAttributeState_Capacity(),
                  FarterAttributeState_RosterSize(context),
                  FarterAttributeState_Fingerprint(context));
    Report(RECOVERED_ARENA_SEANCE_FARTER_ATTRIBUTE_ROSTER_INVALID, message);
    return false;
  }
  g_state.farterAttributesReady = true;
  return true;
}

bool PublishTaxiAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("TaxiAttr") == ct_NULLID) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_TABLE_MISSING,
                   "retail fragment did not create the TaxiAttr table");
    return false;
  }
  if (!TaxiAttributeState_CachesUnresolved(context) ||
      !TaxiAttributeState_IsKnownRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "TaxiAttr objects do not match a bounded unresolved level "
                  "roster (capacity=%d count=%d fingerprint=%llu)",
                  TaxiAttributeState_Capacity(),
                  TaxiAttributeState_RosterSize(context),
                  TaxiAttributeState_Fingerprint(context));
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TAXI_ATTRIBUTE_ROSTER_INVALID,
                   message);
    return false;
  }
  g_state.taxiAttributesReady = true;
  return true;
}

bool PublishBulletAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("BulletAttr") == ct_NULLID) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_TABLE_MISSING,
        "retail fragments did not create the BulletAttr table");
    return false;
  }
  if (!BulletAttributeState_SubjectTableReady(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_SUBJECT_TABLE_FAILURE,
        "retail fragments did not create a bounded Bullet registration table");
    return false;
  }
  if (!BulletAttributeState_CachesUnresolved(context) ||
      !RecoveredGameplayTuning_AcceptsBulletRoster(context)) {
    char message[224] = {};
    std::snprintf(message, sizeof(message),
                  "BulletAttr objects do not match a bounded unresolved "
                  "level roster (capacity=%d count=%d subject_capacity=%d "
                  "fingerprint=%llu)",
                  BulletAttributeState_Capacity(),
                  BulletAttributeState_RosterSize(context),
                  BulletAttributeState_SubjectCapacity(),
                  BulletAttributeState_Fingerprint(context));
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_ATTRIBUTE_ROSTER_INVALID,
        message);
    return false;
  }
  g_state.bulletAttributeCount =
      BulletAttributeState_RosterSize(context);
  g_state.bulletAttributeCapacity = BulletAttributeState_Capacity();
  g_state.bulletSubjectCapacity =
      BulletAttributeState_SubjectCapacity();
  g_state.bulletAttributeFingerprint =
      BulletAttributeState_Fingerprint(context);
  g_state.bulletAttributesReady = true;
  g_state.bulletSubjectRegistrationReady = true;
  const char* probeAttribute =
      BulletAttributeState_FirstAttributeName(context);
  int probeMoveCount = 0;
  if (probeAttribute == nullptr ||
      !BulletSubjectState_ProbeBallisticLifecycle(
          context, probeAttribute, Session::m_moment, &probeMoveCount) ||
      probeMoveCount != 2 || BulletSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_SUBJECT_LIFECYCLE_FAILURE,
        "bounded Bullet start/move/ground-removal/rollback probe failed");
    return false;
  }
  BulletCollisionProbeSummary collisionSummary = {};
  if (!BulletSubjectState_ProbeCollisionLifecycle(
          context, probeAttribute, Session::m_moment,
          &collisionSummary) ||
      collisionSummary.scheduledChecks != 2 ||
      collisionSummary.executedChecks != 1 ||
      collisionSummary.sphereCases != 4 ||
      collisionSummary.earliestHitCases != 3 ||
      collisionSummary.waterlineCases != 4 ||
      (collisionSummary.sceneQueries != 0 &&
       collisionSummary.sceneQueries != 1) ||
      BulletSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_COLLISION_LIFECYCLE_FAILURE,
        "bounded Bullet collision scheduling/query/rollback probe failed");
    return false;
  }
  const unsigned long long subjectFingerprint =
      BulletSubjectState_Fingerprint(context);
  if (subjectFingerprint == 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_SUBJECT_LIFECYCLE_FAILURE,
        "Bullet subject did not return to a stable empty ballistic pool");
    return false;
  }
  g_state.bulletSubjectFingerprint = subjectFingerprint;
  g_state.bulletSubjectProbeMoveCount = probeMoveCount;
  g_state.bulletCollisionScheduledChecks =
      collisionSummary.scheduledChecks;
  g_state.bulletCollisionExecutedChecks =
      collisionSummary.executedChecks;
  g_state.bulletCollisionSphereCases = collisionSummary.sphereCases;
  g_state.bulletCollisionEarliestHitCases =
      collisionSummary.earliestHitCases;
  g_state.bulletCollisionWaterlineCases =
      collisionSummary.waterlineCases;
  g_state.bulletCollisionSceneQueries = collisionSummary.sceneQueries;
  g_state.bulletSubjectReady = true;
  return true;
}

bool PublishBulletReferences(SimulationContext* context) {
  if (!g_state.bulletAttributesReady) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_BULLET_REFERENCE_INVALID,
                   "BulletAttr roster is not ready for reference resolution");
    return false;
  }
  // The January public fixture deliberately substitutes a ten-object test
  // ExplosionAttr roster and owns no Bullet dependencies. Exercise the full
  // two-phase preflight, but require it to leave every cache untouched.
  const bool sourceOnlyFixture =
      g_state.bulletAttributeCount == 4 &&
      g_state.bulletAttributeCapacity == 4 &&
      g_state.bulletSubjectCapacity == 500;
  if (sourceOnlyFixture) {
    if (BulletAttributeState_ResolveReferences(context) ||
        !BulletAttributeState_CachesUnresolved(context)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_BULLET_REFERENCE_INVALID,
                     "source-only Bullet fixture leaked partial references");
      return false;
    }
    return true;
  }
  if (!BulletAttributeState_ResolveReferences(context) ||
      !BulletAttributeState_ReferencesResolved(context)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "BulletAttr could not resolve references atomically: %s",
                  BulletAttributeState_LastError());
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_BULLET_REFERENCE_INVALID,
                   message);
    return false;
  }
  g_state.bulletReferenceFingerprint =
      BulletAttributeState_ReferenceFingerprint(context);
  if (g_state.bulletReferenceFingerprint == 0 ||
      !RecoveredGameplayTuning_AcceptsBulletReferences(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "BulletAttr resolved references are not a bounded roster "
                  "(fingerprint=%llu)",
                  g_state.bulletReferenceFingerprint);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_BULLET_REFERENCE_INVALID,
                   message);
    return false;
  }
  const char* probeAttribute =
      BulletAttributeState_FirstAttributeName(context);
  BulletEffectProbeSummary effectSummary = {};
  if (probeAttribute == nullptr ||
      !BulletSubjectState_ProbeImpactEffectLifecycle(
          context, probeAttribute, Session::m_moment, &effectSummary) ||
      effectSummary.queuedBatches != 2 ||
      effectSummary.queuedChildren != 3 ||
      effectSummary.splashFirstCases != 1 ||
      effectSummary.rolledBackChildren != 3 ||
      ExplosionSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_EFFECT_TRANSACTION_FAILURE,
        "Bullet splash/impact allocation, ordering, or rollback probe failed");
    return false;
  }
  BulletGroundSparkProbeSummary sparkSummary = {};
  if (!BulletSubjectState_ProbeGroundSparkLifecycle(
          context, probeAttribute, Session::m_moment, &sparkSummary) ||
      sparkSummary.queuedSparks != 1 ||
      sparkSummary.rolledBackSparks != 1 ||
      BulletSubjectState_LiveCount() != 0 ||
      SparkSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_GROUND_SPARK_FAILURE,
        "Bullet ground removal did not queue/rollback one self-owned Spark");
    return false;
  }
  const char* barrelSmokeAttribute =
      BulletAttributeState_FirstBarrelSmokeAttributeName(context);
  BulletBarrelSmokeProbeSummary barrelSmokeSummary = {};
  if (barrelSmokeAttribute == nullptr ||
      !BulletSubjectState_ProbeBarrelSmokeLifecycle(
          context, barrelSmokeAttribute, Session::m_moment,
          &barrelSmokeSummary) ||
      barrelSmokeSummary.thresholdStarts != 1 ||
      barrelSmokeSummary.frameGateSkips != 1 ||
      barrelSmokeSummary.attributeGateSkips != 1 ||
      barrelSmokeSummary.rolledBackSmokes != 1 ||
      BulletSubjectState_LiveCount() != 0 ||
      SmokeSubjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_BULLET_BARREL_SMOKE_FAILURE,
        "Bullet barrel Smoke threshold/gates/rollback probe failed");
    return false;
  }
  g_state.bulletEffectQueuedBatches = effectSummary.queuedBatches;
  g_state.bulletEffectQueuedChildren = effectSummary.queuedChildren;
  g_state.bulletEffectSplashFirstCases = effectSummary.splashFirstCases;
  g_state.bulletEffectRolledBackChildren =
      effectSummary.rolledBackChildren;
  g_state.bulletGroundSparkQueued = sparkSummary.queuedSparks;
  g_state.bulletGroundSparkRolledBack = sparkSummary.rolledBackSparks;
  g_state.bulletBarrelSmokeThresholdStarts =
      barrelSmokeSummary.thresholdStarts;
  g_state.bulletBarrelSmokeFrameGateSkips =
      barrelSmokeSummary.frameGateSkips;
  g_state.bulletBarrelSmokeAttributeGateSkips =
      barrelSmokeSummary.attributeGateSkips;
  g_state.bulletBarrelSmokeRollbacks =
      barrelSmokeSummary.rolledBackSmokes;
  g_state.bulletImpactEffectsReady = true;
  g_state.bulletGroundSparkReady = true;
  g_state.bulletBarrelSmokeReady = true;
  g_state.bulletReferencesReady = true;
  return true;
}

bool PublishVehicleReferences(SimulationContext* context) {
  if (!g_state.vehicleAttributesReady ||
      !g_state.bulletAttributesReady ||
      !g_state.taxiAttributesReady) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_VEHICLE_REFERENCE_INVALID,
                   "VehicleAttr dependencies are not ready for resolution");
    return false;
  }
  if (!VehicleAttributeState_ProbeReferenceAtomicity(context) ||
      !VehicleAttributeState_ResolveReferences(context) ||
      !VehicleAttributeState_ReferencesResolved(context)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "VehicleAttr could not resolve Panel/Taxi/Bullet "
                  "references atomically: %s",
                  VehicleAttributeState_LastError());
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_VEHICLE_REFERENCE_INVALID,
                   message);
    return false;
  }
  if (!RecoveredGameplayTuning_FinalizeVehicleReferences(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_GAMEPLAY_TUNING_FAILURE,
        RecoveredGameplayTuning_LastError());
    return false;
  }
  g_state.vehicleReferenceFingerprint =
      VehicleAttributeState_ReferenceFingerprint(context);
  if (g_state.vehicleReferenceFingerprint == 0 ||
      !RecoveredGameplayTuning_AcceptsVehicleReferences(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "VehicleAttr resolved references are not a bounded "
                  "roster (fingerprint=%llu)",
                  g_state.vehicleReferenceFingerprint);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_VEHICLE_REFERENCE_INVALID,
                   message);
    return false;
  }
  g_state.vehicleReferencesReady = true;
  return true;
}

bool PublishTaxiSubject(SimulationContext* context, double startTime,
                        const TaxiSubjectScriptSummary& script) {
  const int capacity = TaxiSubjectState_Capacity();
  const int count = TaxiSubjectState_LiveCount();
  const int sounds = TaxiSubjectState_SoundCount();
  const unsigned long long fingerprint =
      TaxiSubjectState_Fingerprint(context);
  if (!TaxiSubjectState_TableReady(context, script.capacity) ||
      capacity != script.capacity || count != script.objectCount ||
      !TaxiSubjectState_AllReady(context) || fingerprint == 0 ||
      !TaxiSubjectState_IsKnownRetailRoster(context)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "retail Taxi subjects are not exact or runtime-ready "
                  "(capacity=%d/%d count=%d/%d sound=%d fingerprint=%llu)",
                  capacity, script.capacity, count, script.objectCount,
                  sounds, fingerprint);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_ROSTER_INVALID,
                   message);
    return false;
  }

  STaxiSubjectLifecycleProbeSummary probe = {};
  if (!TaxiSubjectState_ProbeLifecycle(
          context, startTime < 0.1 ? 0.1 : startTime, &probe) ||
      TaxiSubjectState_LiveCount() != count ||
      TaxiSubjectState_SoundCount() != sounds ||
      TaxiSubjectState_Fingerprint(context) != fingerprint) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Taxi create/render/sound rollback probe failed "
                  "(%d/%d/%d/%d/%d live=%d sound=%d)",
                  probe.invalidStarts, probe.validStarts,
                  probe.renderReady, probe.soundReady, probe.rollbacks,
                  TaxiSubjectState_LiveCount(),
                  TaxiSubjectState_SoundCount());
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TAXI_SUBJECT_LIFECYCLE_FAILURE,
        message);
    return false;
  }

  g_state.taxiSubjectCapacity = capacity;
  g_state.taxiSubjectCount = count;
  g_state.taxiSubjectSoundCount = sounds;
  g_state.taxiSubjectFingerprint = fingerprint;
  g_state.taxiProbeInvalidStarts = probe.invalidStarts;
  g_state.taxiProbeValidStarts = probe.validStarts;
  g_state.taxiProbeRenderReady = probe.renderReady;
  g_state.taxiProbeSoundReady = probe.soundReady;
  g_state.taxiProbeRollbacks = probe.rollbacks;
  g_state.taxiSubjectReady = true;
  return true;
}

bool PublishLampAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("LampAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_TABLE_MISSING,
           "retail fragment did not create the LampAttr table");
    return false;
  }
  if (!LampAttributeState_IsKnownRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "LampAttr objects do not match the bounded retail roster "
                  "(capacity=%d count=%d fingerprint=%llu)",
                  LampAttributeState_Capacity(),
                  LampAttributeState_RosterSize(context),
                  LampAttributeState_Fingerprint(context));
    Report(RECOVERED_ARENA_SEANCE_LAMP_ATTRIBUTE_ROSTER_INVALID, message);
    return false;
  }
  g_state.lampAttributesReady = true;
  return true;
}

bool PublishCorpseAttributes(SimulationContext* context) {
  if (g_arena.searchSeanceClassTable("CorpseAttr") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_TABLE_MISSING,
           "retail fragment did not create the CorpseAttr table");
    return false;
  }
  if (!CorpseAttributeState_IsKnownRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "CorpseAttr objects do not match a bounded level roster "
                  "(capacity=%d count=%d fingerprint=%llu)",
                  CorpseAttributeState_Capacity(),
                  CorpseAttributeState_RosterSize(context),
                  CorpseAttributeState_Fingerprint(context));
    Report(RECOVERED_ARENA_SEANCE_CORPSE_ATTRIBUTE_ROSTER_INVALID, message);
    return false;
  }
  g_state.corpseAttributesReady = true;
  return true;
}

bool PublishWavMetadata(
    SimulationContext* context,
    const SRecoveredWavMetadataCatalog& catalog) {
  if (g_arena.searchSeanceClassTable("WAVObj") == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_WAV_TABLE_FAILURE,
           "retail fragment did not create the WAVObj table");
    return false;
  }
  const int count = WAVResourceState_RosterSize(context);
  const int capacity = WAVResourceState_Capacity();
  const unsigned long long fingerprint =
      WAVResourceState_Fingerprint(context);
  if (!WAVResourceState_AllLoaded(context) || count != catalog.entryCount ||
      capacity != catalog.capacity || fingerprint != catalog.fingerprint) {
    char message[224] = {};
    std::snprintf(message, sizeof(message),
                  "WAVObj metadata differs from preflight "
                  "(count=%d/%d capacity=%d/%d fingerprint=%llu/%llu)",
                  count, catalog.entryCount, capacity, catalog.capacity,
                  fingerprint, catalog.fingerprint);
    Report(RECOVERED_ARENA_SEANCE_WAV_ROSTER_INVALID, message);
    return false;
  }
  g_state.wavMetadataCount = count;
  g_state.wavMetadataCapacity = capacity;
  g_state.wavCatalogFingerprint = catalog.fingerprint;
  g_state.wavResourceFingerprint = fingerprint;
  g_state.wavMetadataReady = true;
  return true;
}

bool PublishSoundObject(SimulationContext* context, double startTime) {
  const ct_ClassTableID table =
      g_arena.addClassTable("SoundObj", kSoundObjectCapacity);
  if (table == ct_NULLID ||
      !SoundObjectState_TableReady(context, kSoundObjectCapacity) ||
      !SoundObjectState_DeviceFree()) {
    Report(RECOVERED_ARENA_SEANCE_SOUND_OBJECT_TABLE_FAILURE,
           "could not create the device-free retail SoundObj table");
    return false;
  }
  if (!SoundObjectState_ProbeLifecycle(
          context, "wav.Explosion", startTime) ||
      SoundObjectState_LiveCount() != 0) {
    Report(RECOVERED_ARENA_SEANCE_SOUND_OBJECT_LIFECYCLE_FAILURE,
           "SoundObj WAV/move/start/end/reuse lifecycle probe failed");
    return false;
  }
  const unsigned long long fingerprint =
      SoundObjectState_Fingerprint(context);
  if (fingerprint == 0) {
    Report(RECOVERED_ARENA_SEANCE_SOUND_OBJECT_LIFECYCLE_FAILURE,
           "SoundObj table did not return to an empty stable state");
    return false;
  }
  g_state.soundObjectCapacity = kSoundObjectCapacity;
  g_state.soundObjectFingerprint = fingerprint;
  g_state.soundObjectReady = true;
  return true;
}

bool PublishSkinResources(SimulationContext* context) {
  SRecoveredSkinResourceCatalog catalog = {};
  SRecoveredSkinResourceCatalogResult result = {};
  if (!RecoveredSkinResourceCatalog_Load(".", &catalog, &result)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Skin catalog preflight failed (issues=%u): %.190s",
                  result.issues, result.error);
    Report(RECOVERED_ARENA_SEANCE_SKIN_CATALOG_INVALID, message);
    return false;
  }
  if (!RecoveredSkinResourceCatalog_IsKnown(&catalog)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "Skin catalog is not a bounded retail roster "
                  "(models=%d sprites=%d fingerprint=%llu)",
                  catalog.modelCount, catalog.spriteCount,
                  catalog.fingerprint);
    Report(RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_ROSTER_INVALID, message);
    return false;
  }

  const ct_ClassTableID modelTable =
      g_arena.addClassTable("Skin", catalog.modelCapacity);
  const ct_ClassTableID spriteTable =
      g_arena.addClassTable("SkinSpr", catalog.spriteCapacity);
  if (modelTable == ct_NULLID || spriteTable == ct_NULLID) {
    Report(RECOVERED_ARENA_SEANCE_SKIN_TABLE_FAILURE,
           "could not create the retail Skin resource tables");
    return false;
  }

  for (int index = 0; index < catalog.entryCount; ++index) {
    const SRecoveredSkinResourceEntry& entry = catalog.entries[index];
    const ct_ClassTableID table =
        entry.kind == RECOVERED_SKIN_RESOURCE_MODEL ? modelTable : spriteTable;
    KR_ObjectID object = g_arena.newObject(table, entry.objectName);
    if (object.isNUL()) {
      Report(RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_LOAD_FAILURE,
             "could not allocate a retail Skin resource object");
      return false;
    }
    KR_Event event;
    event.label = sk_EV_LOAD;
    event.source = g_arena.getObjectID();
    event.destination = object;
    event.timeStamp = Session::m_moment;
    event.data.open(EDO_WRITE).putStr(entry.fileName).close();
    context->sendEventNow(event);
  }

  const int modelCount = SkinResourceState_ModelCount(context);
  const int spriteCount = SkinResourceState_SpriteCount(context);
  const bool emptyFixture = catalog.entryCount == 0;
  const unsigned long long resourceFingerprint =
      SkinResourceState_Fingerprint(context);
  if (modelCount != catalog.modelCount || spriteCount != catalog.spriteCount ||
      (!emptyFixture &&
       (!SkinResourceState_AllLoaded(context) || resourceFingerprint == 0))) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "Skin resources did not load completely "
                  "(models=%d/%d sprites=%d/%d fingerprint=%llu)",
                  modelCount, catalog.modelCount, spriteCount,
                  catalog.spriteCount, resourceFingerprint);
    Report(RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_LOAD_FAILURE, message);
    return false;
  }
  g_state.skinModelCount = modelCount;
  g_state.skinSpriteCount = spriteCount;
  g_state.skinCatalogFingerprint = catalog.fingerprint;
  g_state.skinResourceFingerprint = resourceFingerprint;
  g_state.skinResourcesReady = true;
  return true;
}

bool PublishSkinAnimations(SimulationContext* context, double startTime) {
  std::string program;
  int entryCallCount = 0;
  unsigned long long sourceFingerprint = 0;
  SRecoveredSkinResourceCatalogResult catalogResult = {};
  if (!RecoveredSkinResourceCatalog_BuildAnimationProgram(
          ".", &program, &entryCallCount, &sourceFingerprint,
          &catalogResult)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Skin animation source rejected (issues=%u): %.180s",
                  catalogResult.issues, catalogResult.error);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_SCRIPT_FAILURE, message);
    return false;
  }
  if (entryCallCount < 0 || entryCallCount > 32 ||
      sourceFingerprint == 0 || program.empty()) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_SCRIPT_FAILURE,
        "Skin animation entry exceeds its bounded source contract");
    return false;
  }

  if (entryCallCount == 0) {
    const unsigned long long stateFingerprint =
        SkinResourceState_AnimationFingerprint(context);
    if (!SkinResourceState_AnimationProgramsReady(context) ||
        SkinResourceState_AnimatedModelCount(context) != 0 ||
        SkinResourceState_AnimationCommandCount(context) != 0 ||
        stateFingerprint == 0) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_STATE_FAILURE,
          "empty Skin animation entry does not have empty owner state");
      return false;
    }
    g_state.skinAnimationEntryCallCount = 0;
    g_state.skinAnimatedModelCount = 0;
    g_state.skinAnimationCommandCount = 0;
    g_state.skinAnimationSourceFingerprint = sourceFingerprint;
    g_state.skinAnimationStateFingerprint = stateFingerprint;
    g_state.skinAnimationsReady = true;
    return true;
  }

  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult runResult = {};
  SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_RetailFragmentProfile();
  profile.compilerWordBufferSize = 64 * 1024;
  profile.compilerNameCount = 4096;
  profile.compilerTreeBufferSize = 256 * 1024;
  profile.compilerCodeStreamSize = 256 * 1024;
  profile.compilerLinkInfoSize = 64 * 1024;
  profile.processStorageStackSize = 4096;
  profile.processStackSize = 4096;
  profile.processQuants = 32768;
  profile.maximumVmSlices = 8192;
  if (!RecoveredLegacyScript_RunMemory(
          program.c_str(), kSkinAnimationProgramName, profile, context,
          startTime, &host, &runResult)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Skin animation script failed (status=%d issues=%u): %.160s",
                  static_cast<int>(runResult.status), runResult.hostIssues,
                  runResult.error);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_SCRIPT_FAILURE, message);
    return false;
  }

  const int animatedModels =
      SkinResourceState_AnimatedModelCount(context);
  const int commands = SkinResourceState_AnimationCommandCount(context);
  const unsigned long long stateFingerprint =
      SkinResourceState_AnimationFingerprint(context);
  SSkinAnimationPoseProbeSummary poseProbe = {};
  if (animatedModels > 0 &&
      (!SkinResourceState_ProbeAnimationPoses(context, startTime, &poseProbe) ||
       poseProbe.animatedModels != animatedModels ||
       poseProbe.temporalModels <= 0 ||
       poseProbe.changedModels != poseProbe.temporalModels ||
       poseProbe.sampledPoses != animatedModels * 10 ||
       poseProbe.restoredModifiers <= 0 || poseProbe.fingerprint == 0)) {
    char message[224] = {};
    std::snprintf(message, sizeof(message),
                  "Skin live-pose proof failed "
                  "(models=%d/%d temporal=%d changed=%d samples=%d "
                  "restored=%d fingerprint=%llu)",
                  poseProbe.animatedModels, animatedModels,
                  poseProbe.temporalModels, poseProbe.changedModels,
                  poseProbe.sampledPoses, poseProbe.restoredModifiers,
                  poseProbe.fingerprint);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_STATE_FAILURE, message);
    return false;
  }
  if (!SkinResourceState_AnimationProgramsReady(context) ||
      animatedModels < 0 || animatedModels > g_state.skinModelCount ||
      commands < 0 || commands > 4096 || stateFingerprint == 0 ||
      (entryCallCount == 0 && (animatedModels != 0 || commands != 0)) ||
      (entryCallCount != 0 && (animatedModels == 0 || commands == 0))) {
    char message[224] = {};
    std::snprintf(message, sizeof(message),
                  "Skin animation state is incomplete "
                  "(entries=%d models=%d commands=%d fingerprint=%llu)",
                  entryCallCount, animatedModels, commands,
                  stateFingerprint);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_SKIN_ANIMATION_STATE_FAILURE, message);
    return false;
  }
  g_state.skinAnimationEntryCallCount = entryCallCount;
  g_state.skinAnimatedModelCount = animatedModels;
  g_state.skinAnimationCommandCount = commands;
  g_state.skinAnimationSourceFingerprint = sourceFingerprint;
  g_state.skinAnimationStateFingerprint = stateFingerprint;
  g_state.skinAnimationPoseTemporalModelCount = poseProbe.temporalModels;
  g_state.skinAnimationPoseChangedModelCount = poseProbe.changedModels;
  g_state.skinAnimationPoseSampleCount = poseProbe.sampledPoses;
  g_state.skinAnimationPoseRestoredModifierCount =
      poseProbe.restoredModifiers;
  g_state.skinAnimationPoseFingerprint = poseProbe.fingerprint;
  g_state.skinAnimationsReady = true;
  return true;
}

bool PublishStaticMechanisms(double startTime) {
  SRecoveredStaticMechanismSummary summary = {};
  if (!RecoveredStaticMechanism_Initialize(
          ZAV_Scene(), RecoveredLevelRuntime_Directory(), startTime,
          &summary)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE,
        RecoveredStaticMechanism_LastError());
    return false;
  }
  if (!summary.initialized || summary.bindingCount < 0 ||
      summary.waterwheelBindings < 0 || summary.flagBindings < 0 ||
      summary.rotatingBindings < 0 || summary.doorBindings < 0 ||
      summary.pol16Bindings < 0 ||
      summary.changedBindings < 0 || summary.sampledPoses < 0 ||
      summary.restoredModifiers < 0 ||
      (summary.levelOne &&
       (!summary.targetLevel || summary.levelFive ||
        summary.bindingCount != 97 || summary.waterwheelBindings != 0 ||
        summary.flagBindings != 3 || summary.rotatingBindings != 27 ||
        summary.doorBindings != 17 || summary.pol16Bindings != 50 ||
        summary.changedBindings != summary.bindingCount ||
        summary.sampledPoses != summary.bindingCount * 5 ||
        summary.restoredModifiers != 209 || summary.fingerprint == 0)) ||
      (summary.levelFive &&
       (!summary.targetLevel || summary.levelOne ||
        summary.bindingCount != 13 || summary.waterwheelBindings != 11 ||
        summary.flagBindings != 2 || summary.rotatingBindings != 0 ||
        summary.doorBindings != 0 || summary.pol16Bindings != 0 ||
        summary.changedBindings != summary.bindingCount ||
        summary.sampledPoses != summary.bindingCount * 5 ||
        summary.restoredModifiers != 84 || summary.fingerprint == 0)) ||
      (!summary.targetLevel &&
       (summary.levelOne || summary.levelFive || summary.bindingCount != 0 ||
        summary.waterwheelBindings != 0 || summary.flagBindings != 0 ||
        summary.rotatingBindings != 0 || summary.doorBindings != 0 ||
        summary.pol16Bindings != 0 || summary.changedBindings != 0 ||
        summary.sampledPoses != 0 || summary.restoredModifiers != 0 ||
        summary.fingerprint != 0))) {
    RecoveredStaticMechanism_Release();
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE,
        "Level static mechanism telemetry violated its bounded contract");
    return false;
  }
  g_state.staticMechanismTargetLevel = summary.targetLevel;
  g_state.staticMechanismLevelOne = summary.levelOne;
  g_state.staticMechanismLevelFive = summary.levelFive;
  g_state.staticMechanismBindingCount = summary.bindingCount;
  g_state.staticMechanismWaterwheelCount = summary.waterwheelBindings;
  g_state.staticMechanismFlagCount = summary.flagBindings;
  g_state.staticMechanismRotatingCount = summary.rotatingBindings;
  g_state.staticMechanismDoorCount = summary.doorBindings;
  g_state.staticMechanismPol16Count = summary.pol16Bindings;
  g_state.staticMechanismChangedBindingCount = summary.changedBindings;
  g_state.staticMechanismPoseSampleCount = summary.sampledPoses;
  g_state.staticMechanismRestoredModifierCount = summary.restoredModifiers;
  g_state.staticMechanismFingerprint = summary.fingerprint;
  g_state.staticMechanismsReady = true;
  return true;
}

bool PublishDependentAttributeReferences(SimulationContext* context) {
  // The public January fixture intentionally has no model assets. Execute the
  // complete preflight anyway: VehicleAttr, Corpse table and CorpseAttr resolve
  // before Skin fails, proving the two-phase helper leaves every Taxi cache
  // untouched. Canonical retail Levels must resolve and publish the full graph.
  if (g_state.skinModelCount == 0) {
    if (TaxiAttributeState_ResolveReferences(context) ||
        !TaxiAttributeState_CachesUnresolved(context)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TAXI_REFERENCE_INVALID,
                     "source-only Taxi fixture leaked partial references");
      return false;
    }
  } else {
    if (!TaxiAttributeState_ResolveReferences(context) ||
        !TaxiAttributeState_ReferencesResolved(context)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TAXI_REFERENCE_INVALID,
                     "TaxiAttr could not resolve Skin/VehicleAttr/Corpse "
                     "references atomically");
      return false;
    }
    g_state.taxiReferenceFingerprint =
        TaxiAttributeState_ReferenceFingerprint(context);
    if (g_state.taxiReferenceFingerprint == 0 ||
        !TaxiAttributeState_IsKnownReferenceRoster(context)) {
      char message[192] = {};
      std::snprintf(message, sizeof(message),
                    "TaxiAttr resolved references are not a bounded retail "
                    "roster (fingerprint=%llu)",
                    g_state.taxiReferenceFingerprint);
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TAXI_REFERENCE_INVALID,
                     message);
      return false;
    }
    g_state.taxiReferencesReady = true;
  }

  if (g_state.skinModelCount == 0) {
    if (OrphanAttributeState_ResolveReferences(context) ||
        !OrphanAttributeState_CachesUnresolved(context)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ORPHAN_REFERENCE_INVALID,
                     "source-only Orphan fixture leaked partial references");
      return false;
    }
  } else {
    if (!OrphanAttributeState_ResolveReferences(context) ||
        !OrphanAttributeState_ReferencesResolved(context)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ORPHAN_REFERENCE_INVALID,
                     "Orphan.Attr.Default could not resolve Explosion/Smoke "
                     "references atomically");
      return false;
    }
    g_state.orphanReferenceFingerprint =
        OrphanAttributeState_ReferenceFingerprint(context);
    if (g_state.orphanReferenceFingerprint == 0 ||
        !OrphanAttributeState_RuntimeReady(context)) {
      char message[192] = {};
      std::snprintf(message, sizeof(message),
                    "Orphan references are incomplete (fingerprint=%llu)",
                    g_state.orphanReferenceFingerprint);
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ORPHAN_REFERENCE_INVALID,
                     message);
      return false;
    }
    g_state.orphanReferencesReady = true;
  }

  if (!SmokerAttributeState_ResolveReferences(context) ||
      !SmokerAttributeState_ReferencesResolved(context)) {
    Report(RECOVERED_ARENA_SEANCE_SMOKER_REFERENCE_INVALID,
           "SmokerAttr could not resolve its SmokeAttr references");
    return false;
  }
  g_state.smokerReferenceFingerprint =
      SmokerAttributeState_ReferenceFingerprint(context);
  if (g_state.smokerReferenceFingerprint == 0 ||
      !SmokerAttributeState_IsKnownReferenceRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "SmokerAttr resolved references are not a bounded roster "
                  "(fingerprint=%llu)",
                  g_state.smokerReferenceFingerprint);
    Report(RECOVERED_ARENA_SEANCE_SMOKER_REFERENCE_INVALID, message);
    return false;
  }
  g_state.smokerReferencesReady = true;
  g_state.smokerRuntimeReady = SmokerAttributeState_RuntimeReady(context);

  if (!FarterAttributeState_ResolveReferences(context) ||
      !FarterAttributeState_ReferencesResolved(context)) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_REFERENCE_INVALID,
           "FarterAttr could not resolve its loaded WAV references");
    return false;
  }
  g_state.farterReferenceFingerprint =
      FarterAttributeState_ReferenceFingerprint(context);
  if (g_state.farterReferenceFingerprint == 0 ||
      !FarterAttributeState_IsKnownReferenceRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "FarterAttr resolved references are not a bounded roster "
                  "(fingerprint=%llu)",
                  g_state.farterReferenceFingerprint);
    Report(RECOVERED_ARENA_SEANCE_FARTER_REFERENCE_INVALID, message);
    return false;
  }
  g_state.farterReferencesReady = true;
  g_state.farterRuntimeReady = false;

  // The source-only parser fixture intentionally declares an empty Skin
  // roster. It must exercise the complete Piece preflight without publishing
  // a model pointer. Every May retail Level owns Expl.Piece and therefore must
  // publish the full reference and lifecycle boundary.
  if (g_state.skinModelCount == 0) {
    if (ExplosionAttributeState_ResolvePieceReferences(context) ||
        !ExplosionAttributeState_PieceCachesUnresolved(context)) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PIECE_LIFECYCLE_FAILURE,
          "source-only Explosion Piece fixture leaked a model reference");
      return false;
    }
  } else {
    if (!ExplosionAttributeState_ProbePieceReferenceAtomicity(context) ||
        !ExplosionAttributeState_ResolvePieceReferences(context) ||
        !ExplosionAttributeState_PieceReferencesResolved(context)) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PIECE_LIFECYCLE_FAILURE,
          "ExplosionAttr could not resolve Piece Skin references atomically");
      return false;
    }
    g_state.explosionPieceReferenceFingerprint =
        ExplosionAttributeState_PieceReferenceFingerprint(context);
    if (g_state.explosionPieceReferenceFingerprint == 0 ||
        !ExplosionAttributeState_IsKnownPieceReferenceRoster(context)) {
      char message[192] = {};
      std::snprintf(message, sizeof(message),
                    "Explosion Piece references are not a bounded retail "
                    "roster (fingerprint=%llu)",
                    g_state.explosionPieceReferenceFingerprint);
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PIECE_LIFECYCLE_FAILURE,
          message);
      return false;
    }
    const char* pieceProbeAttribute =
        ExplosionSubjectState_PieceProbeAttributeName(context);
    ExplosionPieceProbeSummary pieceProbe = {};
    if (pieceProbeAttribute == nullptr ||
        !ExplosionSubjectState_ProbePieceLifecycle(
            context, pieceProbeAttribute, Session::m_moment, &pieceProbe) ||
        pieceProbe.startedPieces <= 0 ||
        pieceProbe.dependencyGateSkips != 1 ||
        pieceProbe.moveSteps <= 0 || pieceProbe.expiredParents != 1 ||
        pieceProbe.rolledBackPieces != pieceProbe.startedPieces ||
        ExplosionSubjectState_LiveCount() != 0 ||
        ExplosionSubjectState_ParticleBranchLiveCount() != 0) {
      char message[256] = {};
      std::snprintf(
          message, sizeof(message),
          "Explosion Piece start/gate/move/expiry/rollback probe failed "
          "(piece=%d gate=%d move=%d expired=%d rollback=%d live=%d/%d)",
          pieceProbe.startedPieces, pieceProbe.dependencyGateSkips,
          pieceProbe.moveSteps, pieceProbe.expiredParents,
          pieceProbe.rolledBackPieces, ExplosionSubjectState_LiveCount(),
          ExplosionSubjectState_ParticleBranchLiveCount());
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PIECE_LIFECYCLE_FAILURE,
          message);
      return false;
    }
    g_state.explosionPieceProbeStartedPieces = pieceProbe.startedPieces;
    g_state.explosionPieceProbeDependencySkips =
        pieceProbe.dependencyGateSkips;
    g_state.explosionPieceProbeMoveSteps = pieceProbe.moveSteps;
    g_state.explosionPieceProbeExpiredParents = pieceProbe.expiredParents;
    g_state.explosionPieceProbeRolledBackPieces =
        pieceProbe.rolledBackPieces;
    g_state.explosionPieceReady = true;
  }

  if (!ExplosionAttributeState_ProbeParticleVisualAtomicity(context) ||
      !ExplosionAttributeState_ResolveParticleVisuals(context) ||
      !ExplosionAttributeState_ParticleVisualsResolved(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PARTICLE_LIFECYCLE_FAILURE,
        "ExplosionAttr could not publish its particle colors atomically");
    return false;
  }
  g_state.explosionParticleVisualFingerprint =
      ExplosionAttributeState_ParticleVisualFingerprint(context);
  if (g_state.explosionParticleVisualFingerprint == 0 ||
      !ExplosionAttributeState_IsKnownParticleVisualRoster(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PARTICLE_LIFECYCLE_FAILURE,
        "Explosion particle visual roster is empty or unknown");
    return false;
  }

  if (!ExplosionAttributeState_ProbeSoundReferenceAtomicity(context) ||
      !ExplosionAttributeState_ResolveSoundReferences(context) ||
      !ExplosionAttributeState_SoundReferencesResolved(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SOUND_LIFECYCLE_FAILURE,
        "ExplosionAttr could not resolve its loaded WAV/SoundObj references "
        "atomically");
    return false;
  }
  g_state.explosionSoundReferenceFingerprint =
      ExplosionAttributeState_SoundReferenceFingerprint(context);
  if (g_state.explosionSoundReferenceFingerprint == 0 ||
      !ExplosionAttributeState_IsKnownSoundReferenceRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "ExplosionAttr sound references are not a bounded roster "
                  "(fingerprint=%llu)",
                  g_state.explosionSoundReferenceFingerprint);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SOUND_LIFECYCLE_FAILURE,
        message);
    return false;
  }
  const char* soundProbeAttribute =
      ExplosionSubjectState_SoundProbeAttributeName(context);
  ExplosionParticleProbeSummary particleProbe = {};
  if (soundProbeAttribute == nullptr ||
      !ExplosionSubjectState_ProbeParticleLifecycle(
          context, soundProbeAttribute, Session::m_moment,
          &particleProbe) ||
      particleProbe.startedBranches <= 0 ||
      particleProbe.simpleParticles <= 0 ||
      particleProbe.snakeParticles <= 0 ||
      particleProbe.dependencyGateSkips != 1 ||
      particleProbe.moveSteps <= 0 ||
      particleProbe.expiredParents != 1 ||
      particleProbe.rolledBackBranches !=
          particleProbe.startedBranches ||
      ExplosionSubjectState_LiveCount() != 0 ||
      ExplosionSubjectState_ParticleBranchLiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_PARTICLE_LIFECYCLE_FAILURE,
        "Explosion simple/snake/ray start/gate/move/expiry/rollback probe "
        "failed");
    return false;
  }
  g_state.explosionParticleProbeStartedBranches =
      particleProbe.startedBranches;
  g_state.explosionParticleProbeSimpleParticles =
      particleProbe.simpleParticles;
  g_state.explosionParticleProbeSnakeParticles =
      particleProbe.snakeParticles;
  g_state.explosionParticleProbeRays = particleProbe.rays;
  g_state.explosionParticleProbeDependencySkips =
      particleProbe.dependencyGateSkips;
  g_state.explosionParticleProbeMoveSteps = particleProbe.moveSteps;
  g_state.explosionParticleProbeExpiredParents =
      particleProbe.expiredParents;
  g_state.explosionParticleProbeRolledBackBranches =
      particleProbe.rolledBackBranches;
  g_state.explosionParticlesReady = true;

  ExplosionSoundProbeSummary soundProbe = {};
  if (soundProbeAttribute == nullptr ||
      !ExplosionSubjectState_ProbeSoundLifecycle(
          context, soundProbeAttribute, Session::m_moment, &soundProbe) ||
      soundProbe.startedSounds != 1 ||
      soundProbe.dependencyGateSkips != 1 ||
      soundProbe.rolledBackSounds != 1 ||
      ExplosionSubjectState_LiveCount() != 0 ||
      SoundObjectState_LiveCount() != 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_SOUND_LIFECYCLE_FAILURE,
        "Explosion one-shot start/dependency-gate/parent rollback probe "
        "failed");
    return false;
  }
  g_state.explosionSoundProbeStarted = soundProbe.startedSounds;
  g_state.explosionSoundProbeDependencySkips =
      soundProbe.dependencyGateSkips;
  g_state.explosionSoundProbeRollbacks = soundProbe.rolledBackSounds;
  g_state.explosionSoundReady = true;

  if (g_state.skinModelCount == 0) {
    if (ExplosionAttributeState_ResolveTraceReferences(context) ||
        !ExplosionAttributeState_TraceCachesUnresolved(context)) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_TRACE_LIFECYCLE_FAILURE,
          "source-only Explosion trace fixture leaked Smoke references");
      return false;
    }
  } else {
    if (!ExplosionAttributeState_ProbeTraceReferenceAtomicity(context) ||
        !ExplosionAttributeState_ResolveTraceReferences(context) ||
        !ExplosionAttributeState_TraceReferencesResolved(context)) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_TRACE_LIFECYCLE_FAILURE,
          "ExplosionAttr could not resolve traced Piece Smoke references "
          "atomically");
      return false;
    }
    g_state.explosionTraceReferenceFingerprint =
        ExplosionAttributeState_TraceReferenceFingerprint(context);
    if (g_state.explosionTraceReferenceFingerprint == 0 ||
        !ExplosionAttributeState_IsKnownTraceReferenceRoster(context)) {
      char message[192] = {};
      std::snprintf(message, sizeof(message),
                    "Explosion trace references are not a bounded retail "
                    "roster (fingerprint=%llu)",
                    g_state.explosionTraceReferenceFingerprint);
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_TRACE_LIFECYCLE_FAILURE,
          message);
      return false;
    }
    const char* traceProbeAttribute =
        ExplosionSubjectState_TraceProbeAttributeName(context);
    ExplosionTraceProbeSummary traceProbe = {};
    if (traceProbeAttribute == nullptr ||
        !ExplosionSubjectState_ProbeTraceLifecycle(
            context, traceProbeAttribute, Session::m_moment, &traceProbe) ||
        traceProbe.startedPieces <= 0 ||
        traceProbe.quotaGateSkips != 1 || traceProbe.puffEvents != 1 ||
        traceProbe.smokeChildren <= 0 || traceProbe.moveSteps <= 0 ||
        traceProbe.expiredParents != 1 ||
        traceProbe.rolledBackPieces != traceProbe.startedPieces ||
        ExplosionSubjectState_LiveCount() != 0 ||
        ExplosionSubjectState_ParticleBranchLiveCount() != 0 ||
        ExplosionSubjectState_TracedParentCount() != 0 ||
        SmokeSubjectState_LiveCount() != 0) {
      char message[320] = {};
      std::snprintf(
          message, sizeof(message),
          "Explosion trace start/quota/puff/smoke/move/expiry/rollback "
          "probe failed (piece=%d quota=%d puff=%d smoke=%d move=%d "
          "expired=%d rollback=%d live=%d/%d/%d/%d)",
          traceProbe.startedPieces, traceProbe.quotaGateSkips,
          traceProbe.puffEvents, traceProbe.smokeChildren,
          traceProbe.moveSteps, traceProbe.expiredParents,
          traceProbe.rolledBackPieces, ExplosionSubjectState_LiveCount(),
          ExplosionSubjectState_ParticleBranchLiveCount(),
          ExplosionSubjectState_TracedParentCount(),
          SmokeSubjectState_LiveCount());
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_EXPLOSION_TRACE_LIFECYCLE_FAILURE,
          message);
      return false;
    }
    g_state.explosionTraceProbeStartedPieces = traceProbe.startedPieces;
    g_state.explosionTraceProbeQuotaGateSkips = traceProbe.quotaGateSkips;
    g_state.explosionTraceProbePuffEvents = traceProbe.puffEvents;
    g_state.explosionTraceProbeSmokeChildren = traceProbe.smokeChildren;
    g_state.explosionTraceProbeMoveSteps = traceProbe.moveSteps;
    g_state.explosionTraceProbeExpiredParents = traceProbe.expiredParents;
    g_state.explosionTraceProbeRolledBackPieces =
        traceProbe.rolledBackPieces;
    g_state.explosionTraceReady = true;
  }

  // The repository CI fixture deliberately has no model assets. Keep that
  // source-only fixture useful, while requiring real Corpse references for
  // every retail Level whose Skin catalog is populated.
  if (g_state.skinModelCount == 0) {
    if (!CorpseAttributeState_CachesUnresolved(context)) {
      Report(RECOVERED_ARENA_SEANCE_CORPSE_REFERENCE_INVALID,
             "source-only Corpse fixture unexpectedly published caches");
      return false;
    }
    return true;
  }
  if (!CorpseAttributeState_ResolveReferences(context) ||
      !CorpseAttributeState_ReferencesResolved(context)) {
    Report(RECOVERED_ARENA_SEANCE_CORPSE_REFERENCE_INVALID,
           "CorpseAttr could not resolve Skin/SmokerAttr references");
    return false;
  }
  g_state.corpseReferenceFingerprint =
      CorpseAttributeState_ReferenceFingerprint(context);
  if (g_state.corpseReferenceFingerprint == 0 ||
      !CorpseAttributeState_IsKnownReferenceRoster(context)) {
    char message[192] = {};
    std::snprintf(message, sizeof(message),
                  "CorpseAttr resolved references are not a bounded retail "
                  "roster (fingerprint=%llu)",
                  g_state.corpseReferenceFingerprint);
    Report(RECOVERED_ARENA_SEANCE_CORPSE_REFERENCE_INVALID, message);
    return false;
  }
  g_state.corpseReferencesReady = true;
  g_state.corpseRuntimeReady = CorpseAttributeState_RuntimeReady(context);
  return true;
}

bool PublishFarterSubject(SimulationContext* context, double startTime) {
  if (!g_state.farterReferencesReady ||
      !FarterAttributeState_RuntimeReady(context)) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "Farter command references are not ready for subject activation");
    return false;
  }
  std::string source;
  FarterSubjectScriptSummary script = {};
  if (!ReadBoundedRetailAttributeSource("SCINC\\SET_FARTER.SCI", &source) ||
      !InspectFarterSubjectScript(source, &script)) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "could not inspect the comment-aware Farter subject script");
    return false;
  }
  const int rosterSize = FarterAttributeState_RosterSize(context);
  const int expectedRosterSize = script.objectCount == 23 ? 4 : 0;
  if (rosterSize != expectedRosterSize) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "Farter subject script and attribute roster disagree");
    return false;
  }
  g_state.farterScriptObjectCount = script.objectCount;
  if (!RunFarterSubjectBootstrap(context, startTime)) {
    return false;
  }
  if (!script.tablePresent) {
    g_state.farterSubjectCapacity = 0;
    g_state.farterSubjectFingerprint =
        FarterSubjectState_AbsentFingerprint();
    g_state.farterSubjectReady =
        g_state.farterSubjectFingerprint != 0 &&
        FarterSubjectState_Capacity() == 0 &&
        FarterSubjectState_LiveCount() == 0 &&
        SoundObjectState_LiveCount() == 0;
    g_state.farterLiveObjectCount = 0;
    g_state.farterSoundObjectCount = 0;
    g_state.farterRuntimeReady = g_state.farterSubjectReady;
    return g_state.farterSubjectReady;
  }

  const ct_ClassTableID table =
      g_arena.searchSeanceClassTable("Farter");
  if (table == ct_NULLID ||
      !FarterSubjectState_TableReady(context, kFarterSubjectCapacity)) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "could not create the retail Farter subject table");
    return false;
  }
  g_state.farterLiveObjectCount = FarterSubjectState_LiveCount();
  g_state.farterSoundObjectCount = SoundObjectState_LiveCount();
  if (g_state.farterLiveObjectCount != script.objectCount ||
      g_state.farterSoundObjectCount != script.objectCount) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "persistent Farter/SoundObj roster does not match retail script");
    return false;
  }
  // A populated retail roster has already executed START_FARTING once for
  // every persistent object. Keep the synthetic create/remove/reuse probe for
  // empty-table coverage only; injecting it into a nearly full capacity-25
  // retail table is neither necessary nor representative.
  if ((rosterSize > 0 && script.objectCount == 0 &&
       !FarterSubjectState_ProbeLifecycle(
           context, "Farter.Attr.Factory", startTime)) ||
      FarterSubjectState_LiveCount() != script.objectCount ||
      SoundObjectState_LiveCount() != script.objectCount) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "Farter START/audible/SoundObj lifecycle probe failed");
    return false;
  }
  if (script.objectCount > 0) {
    if (!g_state.soundDistanceReady ||
        !FarterSubjectState_ProbeAudibleFrames(
            context, g_state.soundDistance,
            &g_state.farterNearFrameAudibleCount,
            &g_state.farterFarFrameAudibleCount)) {
      Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
             "real Farter near/far audible frame transition failed");
      return false;
    }
    g_state.farterAudibleFrameTransition = true;
  }
  const unsigned long long fingerprint =
      FarterSubjectState_Fingerprint(context);
  if (fingerprint == 0) {
    Report(RECOVERED_ARENA_SEANCE_FARTER_SUBJECT_FAILURE,
           "Farter subject roster did not reach a stable silent state");
    return false;
  }
  g_state.farterSubjectCapacity = kFarterSubjectCapacity;
  g_state.farterSubjectFingerprint = fingerprint;
  g_state.farterSubjectReady = true;
  g_state.farterRuntimeReady = true;
  return true;
}

bool PublishPeopleAttributes(SimulationContext* context,
                             const PeopleScriptSummary& script) {
  const bool tablePresent =
      g_arena.searchSeanceClassTable("PeopleAttr") != ct_NULLID;
  if (tablePresent != (script.attributeCapacity > 0) ||
      PeopleSubjectState_AttributeCapacity() != script.attributeCapacity ||
      PeopleSubjectState_AttributeCount(context) != script.attributeCount ||
      !RecoveredGameplayTuning_AcceptsPeopleRoster(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_ROSTER_INVALID,
        "PeopleAttr owner does not match Level-local PEOPLE.SCI");
    return false;
  }
  const unsigned long long fingerprint =
      PeopleSubjectState_AttributeFingerprint(context);
  if (fingerprint == 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_PEOPLE_ATTRIBUTE_ROSTER_INVALID,
        "PeopleAttr roster has no stable identity");
    return false;
  }
  g_state.peopleAttributeCapacity = script.attributeCapacity;
  g_state.peopleAttributeCount = script.attributeCount;
  g_state.peopleAttributeFingerprint = fingerprint;
  g_state.peopleAttributesReady = true;
  return true;
}

bool PublishHowitzerTables(SimulationContext* context,
                           const HowitzerScriptSummary& script) {
  // The fragment host maps retail time zero to the first valid scheduler
  // boundary.  Reach that boundary before any active-world capture so a save
  // can never observe half-created Howitzers with holder names only in event
  // payloads.
  if (!HowitzerSubjectState_ActivateImmediateStarts(context, 0.1)) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "Howitzer immediate start boundary is incomplete");
    return false;
  }
  const int attributeCount = HowitzerSubjectState_AttributeCount();
  const int holderCount = HowitzerSubjectState_HolderCount();
  const int liveCount = HowitzerSubjectState_LiveCount();
  const int readyLiveCount =
      HowitzerSubjectState_ReadyLiveCount(context);
  const int occupiedHolders =
      HowitzerSubjectState_OccupiedHolderCount();
  const bool attributeRosterReady = script.attributeCapacity == 0
      ? attributeCount == 0
      : attributeCount > 0 &&
            attributeCount <= script.attributeCapacity;
  const bool subjectRosterReady = script.subjectCapacity == 0
      ? liveCount == 0 && readyLiveCount == 0 && occupiedHolders == 0
      : liveCount >= 0 && liveCount <= script.subjectCapacity &&
            readyLiveCount >= 0 && readyLiveCount <= liveCount &&
            occupiedHolders == readyLiveCount;
  if (!HowitzerSubjectState_TableReady(context) ||
      HowitzerSubjectState_AttributeCapacity() != script.attributeCapacity ||
      HowitzerSubjectState_SubjectCapacity() != script.subjectCapacity ||
      !attributeRosterReady ||
      !subjectRosterReady || holderCount <= 0) {
    char detail[256] = {};
    std::snprintf(
        detail, sizeof(detail),
        "Howitzer tables/holders mismatch cap=%d/%d state=%d/%d "
        "attr=%d holders=%d live=%d ready=%d occupied=%d table=%d",
        script.attributeCapacity, script.subjectCapacity,
        HowitzerSubjectState_AttributeCapacity(),
        HowitzerSubjectState_SubjectCapacity(), attributeCount, holderCount,
        liveCount, readyLiveCount, occupiedHolders,
        HowitzerSubjectState_TableReady(context) ? 1 : 0);
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           detail);
    return false;
  }
  const unsigned long long fingerprint =
      HowitzerSubjectState_Fingerprint(context);
  if (fingerprint == 0) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "Howitzer table/holder identity is unstable");
    return false;
  }
  return true;
}

bool PublishTankCannonAttributes(
    SimulationContext* context, const TankCannonScriptSummary& script) {
  const int cannonCount = CannonSubjectState_AttributeCount(context);
  const int tankCount = TankSubjectState_AttributeCount(context);
  const unsigned long long cannonFingerprint =
      CannonSubjectState_AttributeFingerprint(context);
  const unsigned long long tankFingerprint =
      TankSubjectState_AttributeFingerprint(context);
  const bool cannonTablePresent =
      g_arena.searchSeanceClassTable("CannonAttr") != ct_NULLID;
  const bool tankTablePresent =
      g_arena.searchSeanceClassTable("TankAttr") != ct_NULLID;
  if (cannonTablePresent != (script.cannonAttributeCapacity > 0) ||
      tankTablePresent != (script.tankAttributeCapacity > 0) ||
      CannonSubjectState_AttributeCapacity() !=
          script.cannonAttributeCapacity ||
      TankSubjectState_AttributeCapacity() != script.tankAttributeCapacity ||
      cannonCount < 0 || cannonCount > script.cannonAttributeCapacity ||
      tankCount < 0 || tankCount > script.tankAttributeCapacity ||
      cannonFingerprint == 0 || tankFingerprint == 0 ||
      !RecoveredGameplayTuning_AcceptsTankRoster(context)) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_ATTRIBUTE_ROSTER_INVALID,
        "CannonAttr/TankAttr owner does not match Level-local TANK.SCI");
    return false;
  }
  g_state.cannonAttributeCapacity = script.cannonAttributeCapacity;
  g_state.cannonAttributeCount = cannonCount;
  g_state.tankAttributeCapacity = script.tankAttributeCapacity;
  g_state.tankAttributeCount = tankCount;
  g_state.cannonAttributeFingerprint = cannonFingerprint;
  g_state.tankAttributeFingerprint = tankFingerprint;
  g_state.tankCannonAttributesReady = true;
  return true;
}

bool PublishCommander(SimulationContext* context,
                      const CommanderScriptSummary& script) {
  const int count = CommanderState_LiveCount(context);
  const int hostileLinks = CommanderState_HostileLinkCount(context);
  const unsigned long long fingerprint =
      CommanderState_Fingerprint(context);
  if (count != script.count || hostileLinks != script.hostilePairs * 2 ||
      fingerprint == 0 || !CommanderState_StableRoundTrip(context)) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Commander cap/count/hostile/fingerprint/roundtrip="
                  "%d/%d/%d/%llu/%d expected=%d/%d/%d",
                  script.capacity, count, hostileLinks, fingerprint,
                  CommanderState_StableRoundTrip(context) ? 1 : 0,
                  script.capacity, script.count, script.hostilePairs * 2);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_COMMANDER_SOURCE_OR_ROSTER_INVALID,
        message);
    return false;
  }
  g_state.commanderCapacity = script.capacity;
  g_state.commanderCount = count;
  g_state.commanderHostileLinks = hostileLinks;
  g_state.commanderFingerprint = fingerprint;
  g_state.commanderReady = true;
  return true;
}

bool PublishTankReferences(SimulationContext* context, double startTime) {
  if (g_state.tankAttributeCount > 0 &&
      !TankSubjectState_UpdateAttributes(context, startTime)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TANK_REFERENCE_INVALID,
                   "TankAttr finalization failed");
    return false;
  }
  const bool referencesResolved =
      TankSubjectState_AttributeReferencesResolved(context);
  if (!g_state.tankCannonAttributesReady || !referencesResolved ||
      CannonSubjectState_AttributeFingerprint(context) !=
          g_state.cannonAttributeFingerprint ||
      TankSubjectState_AttributeFingerprint(context) !=
          g_state.tankAttributeFingerprint) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "Tank attribute dependencies are unresolved or unstable: %s",
                  referencesResolved ? "fingerprint" :
                      TankSubjectState_FirstUnresolvedReference());
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TANK_REFERENCE_INVALID, message);
    return false;
  }
  g_state.tankReferencesReady = true;
  return true;
}

bool PublishTankCannonSubjectTables(
    SimulationContext* context, const TankCannonScriptSummary& script) {
  const int cannonCount = CannonSubjectState_LiveCount(context);
  const int tankCount = TankSubjectState_LiveCount(context);
  const unsigned long long cannonFingerprint =
      CannonSubjectState_SubjectFingerprint(context);
  const unsigned long long tankFingerprint =
      TankSubjectState_SubjectFingerprint(context);
  if (!g_state.tankReferencesReady ||
      TankGroupState_LiveCount(context) != 0 ||
      CannonSubjectState_SubjectCapacity() != script.cannonSubjectCapacity ||
      TankSubjectState_SubjectCapacity() != script.tankSubjectCapacity ||
      cannonCount != 0 || tankCount != 0 || cannonFingerprint == 0 ||
      tankFingerprint == 0) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_TANK_CANNON_SUBJECT_TABLE_FAILURE,
        "Level-local Cannon/Tank subject owners are not pristine");
    return false;
  }
  g_state.cannonSubjectCapacity = script.cannonSubjectCapacity;
  g_state.cannonSubjectCount = cannonCount;
  g_state.tankSubjectCapacity = script.tankSubjectCapacity;
  g_state.tankGroupSubjectCapacity = script.tankGroupSubjectCapacity;
  g_state.tankSubjectCount = tankCount;
  g_state.cannonSubjectFingerprint = cannonFingerprint;
  g_state.tankSubjectFingerprint = tankFingerprint;
  g_state.tankCannonSubjectTablesReady = true;
  return true;
}

bool PublishTankLifecycle(SimulationContext* context, double startTime) {
  if (!g_state.tankCannonSubjectTablesReady || !g_state.vehicleReady) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TANK_LIFECYCLE_FAILURE,
                   "Tank lifecycle requires pristine owners and Vehicle");
    return false;
  }
  STankLifecycleProbeSummary probe = {};
  if (g_state.tankAttributeCount > 0) {
    if (!TankSubjectState_ProbeLifecycle(context, startTime, &probe)) {
      char message[256] = {};
      std::snprintf(
          message, sizeof(message),
          "Tank probe available/start/dyn/render/cannon/move/cadence/frames/"
          "view/bullet/death/effects/save/rollback="
          "%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d",
          probe.available, probe.validStarts, probe.dynamicReady,
          probe.renderReady, probe.cannonReady, probe.scheduledMoves,
          probe.cadenceBounded, probe.renderedPoseFrames,
          probe.viewBoundaryResets, probe.bulletDamageApplications,
          probe.deathTransitions, probe.deathEffects,
          probe.saveStateRoundTrips, probe.rollbacks);
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_TANK_LIFECYCLE_FAILURE,
                     message);
      return false;
    }
  } else {
    // Retail Level.06 creates an empty TankAttr owner; Level.07 omits the
    // entire Tank/Cannon layer. Both are exact not-applicable outcomes.
    probe.rollbacks = 1;
  }
  if (!RecoveredGameplayTuning_FinalizeTankLifecycle(context, startTime)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_GAMEPLAY_TUNING_FAILURE,
                   RecoveredGameplayTuning_LastError());
    return false;
  }
  g_state.tankProbeAvailable = probe.available;
  g_state.tankProbeValidStarts = probe.validStarts;
  g_state.tankProbeDynamicReady = probe.dynamicReady;
  g_state.tankProbeRenderReady = probe.renderReady;
  g_state.tankProbeCannonReady = probe.cannonReady;
  g_state.tankProbeScheduledMoves = probe.scheduledMoves;
  g_state.tankProbeCadenceBounded = probe.cadenceBounded;
  g_state.tankProbeRenderedPoseFrames = probe.renderedPoseFrames;
  g_state.tankProbeViewBoundaryResets = probe.viewBoundaryResets;
  g_state.tankProbeBulletDamageApplications =
      probe.bulletDamageApplications;
  g_state.tankProbeDeathTransitions = probe.deathTransitions;
  g_state.tankProbeDeathEffects = probe.deathEffects;
  g_state.tankProbeSaveStateRoundTrips = probe.saveStateRoundTrips;
  g_state.tankProbeRollbacks = probe.rollbacks;
  return true;
}

void RemoveAllEvents(SimulationContext* context, int label,
                     const KR_ObjectID& source) {
  if (context == nullptr) return;
  while (context->removeEvent(label, source) == 1) {
  }
}

void RollBackAER00TankSpawn(SimulationContext* context) {
  if (context == nullptr) return;
  const KR_ObjectID group = context->isExist("C.Group.aer00.00")
      ? context->searchObject("C.Group.aer00.00") : KR_ObjectID::NUL();
  const KR_ObjectID tank = context->isExist("C.Unit.aer00.00")
      ? context->searchObject("C.Unit.aer00.00") : KR_ObjectID::NUL();
  KR_ObjectID mutableGroup = group;
  KR_ObjectID mutableTank = tank;
  if (!mutableGroup.isNUL()) {
    RemoveAllEvents(context, tg_EVC_FIND_ENEMY, group);
    RemoveAllEvents(context, tg_EVC_MOVING, group);
    RemoveAllEvents(context, tg_EV_REACHED, group);
    if (context->isExist(group)) context->removeObject(group);
  }
  if (!mutableTank.isNUL()) {
    std::vector<KR_ObjectID> tanks(1, tank);
    TankActiveWorldState_RemoveStableOwners(context, &tanks);
  }
}

void RollBackAER00TankCombatOwners(SimulationContext* context) {
  if (context == nullptr) return;
  if (context->isExist("C.Group.aer00.00")) {
    const KR_ObjectID group = context->searchObject("C.Group.aer00.00");
    RemoveAllEvents(context, tg_EVC_FIND_ENEMY, group);
    RemoveAllEvents(context, tg_EVC_MOVING, group);
    RemoveAllEvents(context, tg_EV_REACHED, group);
    if (context->isExist(group)) context->removeObject(group);
  }
  if (context->isExist("C.Unit.aer00.00")) {
    std::vector<KR_ObjectID> tanks(
        1, context->searchObject("C.Unit.aer00.00"));
    TankActiveWorldState_RemoveStableOwners(context, &tanks);
  }
}

unsigned long long ActiveWorldContentFingerprint() {
  if (RecoveredRetailScriptManifest_IsReady()) {
    const SRecoveredRetailScriptManifestSummary* manifest =
        RecoveredRetailScriptManifest_Summary();
    if (manifest != nullptr && manifest->contentFingerprint != 0)
      return manifest->contentFingerprint;
  }
  // Direct seance tests intentionally bypass the level preflight. Retain a
  // deterministic content identity there by folding already committed retail
  // table fingerprints; normal game startup always uses the script manifest.
  unsigned long long fingerprint = g_state.commanderFingerprint;
  fingerprint ^= g_state.tankAttributeFingerprint +
                 0x9e3779b97f4a7c15ull + (fingerprint << 6) +
                 (fingerprint >> 2);
  fingerprint ^= g_state.peopleAttributeFingerprint +
                 0x9e3779b97f4a7c15ull + (fingerprint << 6) +
                 (fingerprint >> 2);
  return fingerprint == 0 ? 0x5252324e57535631ull : fingerprint;
}

std::string ActiveWorldLevelIdentity() {
  const char* catalogIdentity =
      RecoveredModRuntime_ActiveLevelIdentity();
  if (catalogIdentity != nullptr && catalogIdentity[0] != '\0')
    return catalogIdentity;
  const char* selected = RecoveredLevelRuntime_Directory();
  if (selected == nullptr || selected[0] == '\0') return "direct-context";
  std::string path(selected);
  while (!path.empty() && (path.back() == '\\' || path.back() == '/'))
    path.pop_back();
  const std::size_t separator = path.find_last_of("\\/");
  const std::string name = separator == std::string::npos
                               ? path
                               : path.substr(separator + 1);
  return name.empty() ? "direct-context" : name;
}

void PublishActiveWorldSummary(
    const SActiveWorldRuntimeProbeSummary& summary) {
  g_state.activeWorldPersistenceReady = summary.ready;
  g_state.activeWorldSections = summary.sections;
  g_state.activeWorldEvents = summary.events;
  g_state.activeWorldOwnerPhases = summary.ownerPhases;
  g_state.activeWorldReferencePhases = summary.referencePhases;
  g_state.activeWorldEventPhases = summary.eventPhases;
  g_state.activeWorldCreatedOwners = summary.createdOwners;
  g_state.activeWorldMissionRecords = summary.missionRecords;
  g_state.activeWorldMissionConditionReferences =
      summary.missionConditionReferences;
  g_state.activeWorldMissionRouteReferences = summary.missionRouteReferences;
  g_state.activeWorldMissionCheckEvents = summary.missionCheckEvents;
  g_state.activeWorldClockRecords = summary.clockRecords;
  g_state.activeWorldRngAlgorithm = summary.rngAlgorithm;
  g_state.activeWorldRngStateBytes = summary.rngStateBytes;
  g_state.activeWorldRngDrawCount =
      static_cast<unsigned long long>(summary.rngDrawCount);
  g_state.activeWorldCorruptionRejects = summary.corruptionRejects;
  g_state.activeWorldRollbacks = summary.rollbacks;
  g_state.activeWorldContainerBytes =
      static_cast<unsigned long long>(summary.containerBytes);
  g_state.activeWorldFingerprint = summary.worldFingerprint;
}

bool CaptureActiveWorldProbe(
    SimulationContext* context, std::vector<std::uint8_t>* bytes,
    SActiveWorldRuntimeProbeSummary* summary) {
  std::string failure;
  if (ActiveWorldRuntime_CaptureProbe(
          context, ActiveWorldContentFingerprint(), ActiveWorldLevelIdentity(),
          bytes, summary, &failure))
    return true;
  ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_ENVELOPE_FAILURE,
                 failure.c_str());
  return false;
}

bool RestoreActiveWorldProbe(
    SimulationContext* context, const std::vector<std::uint8_t>& bytes,
    SActiveWorldRuntimeProbeSummary* summary) {
  std::string failure;
  if (ActiveWorldRuntime_RestoreProbe(context, bytes, summary, &failure)) {
    PublishActiveWorldSummary(*summary);
    return true;
  }
  ReportExtended(RECOVERED_ARENA_SEANCE_EXT_ACTIVE_WORLD_RESTORE_FAILURE,
                 failure.c_str());
  return false;
}

unsigned long long MissionTankOwnershipFingerprint(
    SimulationContext* context) {
  const unsigned long long commander = CommanderState_Fingerprint(context);
  const unsigned long long group = TankGroupState_Fingerprint(context);
  const unsigned long long tank = TankSubjectState_SubjectFingerprint(context);
  if (commander == 0 || group == 0 || tank == 0) return 0;
  return commander ^ (group + 0x9e3779b97f4a7c15ull +
                      (commander << 6) + (commander >> 2)) ^
         (tank * 1099511628211ull);
}

bool ValidateAER00TankOwnership(SimulationContext* context,
                               KR_ObjectID* commander,
                               KR_ObjectID* group, KR_ObjectID* tank,
                               int* links) {
  if (context == nullptr || commander == nullptr || group == nullptr ||
      tank == nullptr || links == nullptr ||
      !context->isExist("Colony") ||
      !context->isExist("C.Group.aer00.00") ||
      !context->isExist("C.Unit.aer00.00"))
    return false;
  *commander = context->searchObject("Colony");
  *group = context->searchObject("C.Group.aer00.00");
  *tank = context->searchObject("C.Unit.aer00.00");
  IUnit* unit = static_cast<IUnit*>(
      context->queryInterface(*tank, IUnitIID));
  *links = 0;
  if (CommanderState_HasMember(context, *commander, *group)) ++*links;
  if (TankGroupState_Commander(context, *group) == *commander) ++*links;
  if (TankGroupState_HasMember(context, *group, *tank)) ++*links;
  if (unit != nullptr && unit->getCommander() == *commander) ++*links;
  return *links == 4 && unit != nullptr;
}

bool PublishMissionTankLifecycle(SimulationContext* context,
                                 double startTime) {
  if (!g_state.commanderReady || !g_state.tankCannonSubjectTablesReady) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        "mission Tank lifecycle requires Commander and pristine owners");
    return false;
  }
  const int baselineGroups = TankGroupState_LiveCount(context);
  const int baselineTanks = TankSubjectState_LiveCount(context);
  const int baselineCannons = CannonSubjectState_LiveCount(context);
  const int baselineSounds = SoundObjectState_LiveCount();
  const unsigned long long baselineCommander =
      CommanderState_Fingerprint(context);
  const unsigned long long baselineGroup =
      TankGroupState_Fingerprint(context);
  const unsigned long long baselineTank =
      TankSubjectState_SubjectFingerprint(context);
  if (baselineGroups != 0 || baselineTanks != 0 || baselineCannons != 0 ||
      baselineCommander == 0 || baselineGroup == 0 || baselineTank == 0)
    return false;

  std::vector<std::uint8_t> activeWorldBytes;
  SActiveWorldRuntimeProbeSummary activeWorldSummary;
  if (!LevelHasRetailAER00TankSpawn()) {
    if (!CaptureActiveWorldProbe(context, &activeWorldBytes,
                                 &activeWorldSummary) ||
        !RestoreActiveWorldProbe(context, activeWorldBytes,
                                 &activeWorldSummary))
      return false;
    g_state.missionTankAvailable = 0;
    g_state.missionTankRollbacks = 1;
    g_state.missionTankLifecycleReady = true;
    return true;
  }
  g_state.missionTankAvailable = 1;
  if (!context->isExist("Colony") || !context->isExist("TankLevEnglAttr")) {
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        "AER00 Tank spawn dependencies are missing");
    return false;
  }

  KR_ObjectID firstCommander, firstGroup, firstTank;
  int firstLinks = 0;
  const bool firstSpawned =
      RunRetailAER00TankSpawn(context, startTime + 0.5);
  const bool firstOwned = firstSpawned &&
      ValidateAER00TankOwnership(context, &firstCommander, &firstGroup,
                                 &firstTank, &firstLinks);
  const int firstCannons = CannonSubjectState_LiveCount(context);
  if (!firstSpawned || !firstOwned || firstCannons <= baselineCannons) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "first AER00 spawn/owned/links/cannons/baseline="
                  "%d/%d/%d/%d/%d group=%d tank=%d %.96s",
                  firstSpawned ? 1 : 0, firstOwned ? 1 : 0, firstLinks,
                  firstCannons, baselineCannons,
                  context->isExist("C.Group.aer00.00") ? 1 : 0,
                  context->isExist("C.Unit.aer00.00") ? 1 : 0,
                  g_state.lastError);
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        message);
    return false;
  }
  g_state.missionTankSpawns = 1;
  g_state.missionTankMembershipLinks = firstLinks;
  const unsigned long long firstCommanderFingerprint =
      CommanderState_Fingerprint(context);
  const unsigned long long firstGroupFingerprint =
      TankGroupState_Fingerprint(context);
  const unsigned long long firstTankFingerprint =
      TankSubjectState_SubjectFingerprint(context);
  const unsigned long long firstFingerprint =
      MissionTankOwnershipFingerprint(context);
  const bool firstCommanderRoundTrip =
      CommanderState_StableRoundTrip(context);
  const bool firstGroupRoundTrip =
      TankGroupState_StableRoundTrip(context, firstGroup);
  if (firstFingerprint == 0 || !firstCommanderRoundTrip ||
      !firstGroupRoundTrip) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "first AER00 stable state=%llu/%llu/%llu combined=%llu "
                  "roundtrips=%d/%d",
                  firstCommanderFingerprint, firstGroupFingerprint,
                  firstTankFingerprint, firstFingerprint,
                  firstCommanderRoundTrip ? 1 : 0,
                  firstGroupRoundTrip ? 1 : 0);
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        message);
    return false;
  }
  g_state.missionTankStableRoundTrips = 2;
  if (!CaptureActiveWorldProbe(context, &activeWorldBytes,
                               &activeWorldSummary)) {
    RollBackAER00TankSpawn(context);
    return false;
  }
  std::vector<std::uint8_t> tankStableBytes;
  std::vector<KR_ObjectID> firstOwnedCannons;
  if (!TankActiveWorldState_CaptureStable(context, &tankStableBytes) ||
      !TankActiveWorldState_CollectOwnedCannons(
          context, tankStableBytes, &firstOwnedCannons) ||
      static_cast<int>(firstOwnedCannons.size()) != firstCannons) {
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        "AER00 Tank/Cannon canonical ownership capture failed");
    return false;
  }

  STankGroupSchedulerProbeSummary scheduler = {};
  if (!TankGroupState_ProbeScheduler(context, firstGroup, startTime + 1.0,
                                     &scheduler)) {
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        "AER00 TankGroup scheduler did not repeat");
    return false;
  }
  g_state.missionTankFindEnemyCycles = scheduler.findEnemyCycles;
  g_state.missionTankMovingCycles = scheduler.movingCycles;
  RollBackAER00TankCombatOwners(context);
  if (context->isExist("C.Group.aer00.00") ||
      context->isExist("C.Unit.aer00.00") ||
      TankGroupState_LiveCount(context) != baselineGroups ||
      TankSubjectState_LiveCount(context) != baselineTanks ||
      CannonSubjectState_LiveCount(context) != baselineCannons) {
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        "AER00 combat-owner teardown retained a Tank/Cannon dependency");
    return false;
  }
  if (!RestoreActiveWorldProbe(context, activeWorldBytes,
                               &activeWorldSummary) ||
      activeWorldSummary.createdOwners != 2) {
    const std::string cause = g_state.lastError;
    RollBackAER00TankSpawn(context);
    char message[320] = {};
    std::snprintf(message, sizeof(message),
                  "active-world TankGroup/Tank allocation failed: %.220s",
                  cause.c_str());
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        message);
    return false;
  }

  KR_ObjectID secondCommander, secondGroup, secondTank;
  int secondLinks = 0;
  if (!ValidateAER00TankOwnership(context, &secondCommander, &secondGroup,
                                  &secondTank, &secondLinks)) {
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        "restored AER00 symbolic ownership is invalid");
    return false;
  }
  const unsigned long long secondCommanderFingerprint =
      CommanderState_Fingerprint(context);
  const unsigned long long secondGroupFingerprint =
      TankGroupState_Fingerprint(context);
  const unsigned long long secondTankFingerprint =
      TankSubjectState_SubjectFingerprint(context);
  const unsigned long long secondFingerprint =
      MissionTankOwnershipFingerprint(context);
  std::vector<KR_ObjectID> secondOwnedCannons;
  const bool cannonIDsReallocated =
      TankActiveWorldState_CollectOwnedCannons(
          context, tankStableBytes, &secondOwnedCannons) &&
      secondOwnedCannons.size() == firstOwnedCannons.size() &&
      !secondOwnedCannons.empty();
  bool everyCannonIDChanged = cannonIDsReallocated;
  for (std::size_t index = 0;
       index < secondOwnedCannons.size() && everyCannonIDChanged; ++index)
    everyCannonIDChanged =
        secondOwnedCannons[index] != firstOwnedCannons[index];
  if (secondFingerprint != firstFingerprint ||
      secondCommander != firstCommander || secondGroup == firstGroup ||
      secondTank == firstTank || secondLinks != firstLinks ||
      !everyCannonIDChanged ||
      !TankActiveWorldState_MatchesStable(context, tankStableBytes)) {
    char message[256] = {};
    std::snprintf(
        message, sizeof(message),
        "AER00 reconstruction combined=%llu/%llu parts="
        "%llu,%llu,%llu/%llu,%llu,%llu ids=%ld,%ld/%ld,%ld links=%d/%d",
        firstFingerprint, secondFingerprint, firstCommanderFingerprint,
        firstGroupFingerprint, firstTankFingerprint,
        secondCommanderFingerprint, secondGroupFingerprint,
        secondTankFingerprint, firstGroup.id, secondGroup.id,
        firstTank.id, secondTank.id, firstLinks, secondLinks);
    RollBackAER00TankSpawn(context);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        message);
    return false;
  }
  g_state.missionTankReconstructedIDs = 1;
  g_state.missionTankStableRoundTrips = 3;
  g_state.missionTankFingerprint = secondFingerprint;
  RollBackAER00TankSpawn(context);

  if (TankGroupState_LiveCount(context) != baselineGroups ||
      TankSubjectState_LiveCount(context) != baselineTanks ||
      CannonSubjectState_LiveCount(context) != baselineCannons ||
      SoundObjectState_LiveCount() != baselineSounds ||
      CommanderState_Fingerprint(context) != baselineCommander ||
      TankGroupState_Fingerprint(context) != baselineGroup ||
      TankSubjectState_SubjectFingerprint(context) != baselineTank) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "AER00 rollback groups/tanks/cannons/sounds=%d/%d/%d/%d "
                  "expected=%d/%d/%d/%d fingerprints=%d/%d/%d",
                  TankGroupState_LiveCount(context),
                  TankSubjectState_LiveCount(context),
                  CannonSubjectState_LiveCount(context),
                  SoundObjectState_LiveCount(), baselineGroups, baselineTanks,
                  baselineCannons, baselineSounds,
                  CommanderState_Fingerprint(context) == baselineCommander,
                  TankGroupState_Fingerprint(context) == baselineGroup,
                  TankSubjectState_SubjectFingerprint(context) == baselineTank);
    ReportExtended(
        RECOVERED_ARENA_SEANCE_EXT_MISSION_TANK_LIFECYCLE_FAILURE,
        message);
    return false;
  }
  g_state.missionTankRollbacks = 1;
  g_state.missionTankLifecycleReady = true;
  return true;
}

bool PublishPeopleReferences(SimulationContext* context, double startTime) {
  if (!g_state.peopleAttributesReady) return false;
  if (g_state.peopleAttributeCount > 0) {
    // This is the original LEVEL0.SC s_UpdateAttributes boundary. It runs only
    // after Skin, WAV, Bullet, Smoke, Explosion and Corpse have committed, so
    // PeopleAttr::update cannot observe a partially published dependency graph.
    if (!PeopleSubjectState_UpdateAttributes(context, startTime)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_REFERENCE_INVALID,
                     "PeopleAttr finalization failed");
      return false;
    }
  }
  const bool bulletGraphStable = g_state.bulletReferencesReady
      ? BulletAttributeState_ReferencesResolved(context) &&
            BulletAttributeState_ReferenceFingerprint(context) ==
                g_state.bulletReferenceFingerprint
      : g_state.bulletReferenceFingerprint == 0 &&
            BulletAttributeState_CachesUnresolved(context);
  if (PeopleSubjectState_AttributeFingerprint(context) !=
          g_state.peopleAttributeFingerprint ||
      !bulletGraphStable) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_REFERENCE_INVALID,
                   "PeopleAttr finalization changed a committed dependency graph");
    return false;
  }
  g_state.peopleReferencesReady = true;
  return true;
}

bool ProbePeopleActiveWorldPersistence(
    SimulationContext* context, int expectedCount, int expectedSounds,
    unsigned long long expectedSubjectFingerprint) {
  std::vector<unsigned char> bytes;
  const int baselineSounds = SoundObjectState_LiveCount();
  if (context == nullptr || expectedCount < 0 || expectedSounds < 0 ||
      baselineSounds < expectedSounds ||
      !PeopleActiveWorldState_CaptureStable(context, &bytes) ||
      !PeopleActiveWorldState_ValidateStable(bytes) ||
      !PeopleActiveWorldState_MatchesStable(context, bytes)) {
    std::string message = "People active-world capture rejected the live roster";
    const char* codecFailure = PeopleActiveWorldState_LastFailure();
    if (codecFailure != nullptr && codecFailure[0] != 0) {
      message += ": ";
      message += codecFailure;
    }
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   message.c_str());
    return false;
  }
  const unsigned long long fingerprint =
      PeopleActiveWorldState_Fingerprint(context);
  const int schedulerEvents =
      PeopleActiveWorldState_SchedulerEventCount(bytes);
  if (fingerprint == 0 || schedulerEvents < 0 ||
      PeopleActiveWorldState_LiveCount(context) != expectedCount) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world identity is invalid");
    return false;
  }

  std::vector<KR_ObjectID> oldOwners;
  std::vector<KR_ObjectID> heldRoutes;
  if (!PeopleActiveWorldState_CollectStableOwners(context, bytes,
                                                  &oldOwners) ||
      !PeopleActiveWorldState_HoldRouteReferences(context, bytes,
                                                  &heldRoutes)) {
    PeopleActiveWorldState_ReleaseRouteReferences(context, &heldRoutes);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world teardown preflight failed");
    return false;
  }

  std::vector<KR_ObjectID> removed = oldOwners;
  PeopleActiveWorldState_RemoveStableOwners(context, &removed);
  if (PeopleActiveWorldState_LiveCount(context) != 0 ||
      PeopleSubjectState_SoundCount(context) != 0 ||
      SoundObjectState_LiveCount() != baselineSounds - expectedSounds) {
    PeopleActiveWorldState_ReleaseRouteReferences(context, &heldRoutes);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world teardown leaked owners or sounds");
    return false;
  }

  // Exercise allocation rollback before committing any references. Route
  // guards keep shared and People-only paths alive across this empty world.
  std::vector<KR_ObjectID> staged;
  if (!PeopleActiveWorldState_CreateStableOwners(context, bytes, &staged) ||
      static_cast<int>(staged.size()) != expectedCount) {
    PeopleActiveWorldState_RemoveStableOwners(context, &staged);
    PeopleActiveWorldState_ReleaseRouteReferences(context, &heldRoutes);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world staged allocation failed");
    return false;
  }
  PeopleActiveWorldState_RemoveStableOwners(context, &staged);
  if (PeopleActiveWorldState_LiveCount(context) != 0 ||
      SoundObjectState_LiveCount() != baselineSounds - expectedSounds) {
    PeopleActiveWorldState_ReleaseRouteReferences(context, &heldRoutes);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world rollback retained staged owners");
    return false;
  }

  std::vector<KR_ObjectID> restored;
  if (!PeopleActiveWorldState_CreateStableOwners(context, bytes, &restored) ||
      static_cast<int>(restored.size()) != expectedCount ||
      !PeopleActiveWorldState_ApplyStableReferences(context, bytes)) {
    PeopleActiveWorldState_RemoveStableOwners(context, &restored);
    PeopleActiveWorldState_ReleaseRouteReferences(context, &heldRoutes);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world reconstruction failed");
    return false;
  }
  PeopleActiveWorldState_ReleaseRouteReferences(context, &heldRoutes);

  std::vector<KR_ObjectID> newOwners;
  bool allIDsReallocated =
      PeopleActiveWorldState_CollectStableOwners(context, bytes, &newOwners) &&
      newOwners.size() == oldOwners.size();
  for (std::size_t index = 0;
       allIDsReallocated && index < oldOwners.size(); ++index)
    allIDsReallocated = oldOwners[index] != newOwners[index];
  if (!allIDsReallocated ||
      !PeopleActiveWorldState_MatchesStable(context, bytes) ||
      PeopleActiveWorldState_Fingerprint(context) != fingerprint ||
      PeopleSubjectState_SubjectFingerprint(context) !=
          expectedSubjectFingerprint ||
      PeopleActiveWorldState_LiveCount(context) != expectedCount ||
      PeopleSubjectState_SoundCount(context) != expectedSounds ||
      SoundObjectState_LiveCount() != baselineSounds ||
      !PeopleSubjectState_AllReady(context)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   "People active-world reconstruction diverged");
    return false;
  }

  g_state.peopleActiveWorldReconstructedIDs = expectedCount;
  g_state.peopleActiveWorldSchedulerEvents = schedulerEvents;
  g_state.peopleActiveWorldRollbacks = 1;
  g_state.peopleActiveWorldFingerprint = fingerprint;
  return true;
}

bool PublishPeopleSubject(SimulationContext* context, double startTime,
                          const PeopleScriptSummary& script) {
  if (!g_state.peopleReferencesReady) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_ROSTER_INVALID,
                   "People references were not published before population");
    return false;
  }
  if (!RunPeopleSubjectBootstrap(context, startTime)) return false;
  const bool tablesReady = PeopleSubjectState_TablesReady(
      context, script.attributeCapacity, script.subjectCapacity);
  const int liveCount = PeopleSubjectState_LiveCount(context);
  const bool allReady = PeopleSubjectState_AllReady(context);
  if (!tablesReady || liveCount != script.subjectCount || !allReady) {
    char message[256] = {};
    std::snprintf(message, sizeof(message),
                  "People first=%s table=%d live=%d expected=%d ready=%d",
                  allReady ? "none" : PeopleSubjectState_FirstNotReady(),
                  tablesReady ? 1 : 0, liveCount, script.subjectCount,
                  allReady ? 1 : 0);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_ROSTER_INVALID,
                   message);
    return false;
  }
  const unsigned long long fingerprint =
      PeopleSubjectState_SubjectFingerprint(context);
  if (fingerprint == 0) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_ROSTER_INVALID,
                   "People subject roster has no stable identity");
    return false;
  }

  if (script.subjectCount > 0) {
    SPeopleLifecycleProbeSummary probe = {};
    if (!PeopleSubjectState_ProbeLifecycle(context, startTime, &probe)) {
      char message[256] = {};
      std::snprintf(
          message, sizeof(message),
          "People probe start/phase/end/corridor/obstacle/contact/dyn/render/move/cadence/"
          "frames/view/bullet/death/save/rollback="
          "%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d",
          probe.validStarts, probe.routePhaseExact,
          probe.routeEndPolicies, probe.corridorProjection,
          probe.obstacleRecovery, probe.contactResponse,
          probe.dynamicReady, probe.renderReady,
          probe.scheduledMoves, probe.cadenceBounded,
          probe.renderedPoseFrames, probe.viewBoundaryResets,
          probe.bulletDamageApplications, probe.deathTransitions,
          probe.saveStateRoundTrips, probe.rollbacks);
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                     message);
      return false;
    }
    g_state.peopleProbeScheduledMoves = probe.scheduledMoves;
    g_state.peopleProbeCadenceBounded = probe.cadenceBounded;
    g_state.peopleProbeRenderedPoseFrames = probe.renderedPoseFrames;
    g_state.peopleProbeViewBoundaryResets = probe.viewBoundaryResets;
    g_state.peopleProbeBulletDamageApplications =
        probe.bulletDamageApplications;
    g_state.peopleProbeDeathTransitions = probe.deathTransitions;
    g_state.peopleProbeSaveStateRoundTrips = probe.saveStateRoundTrips;
    g_state.peopleProbeRollbacks = probe.rollbacks;

    SPeopleCombatProbeSummary combat = {};
    if (!PeopleSubjectState_ProbeCombatLifecycle(
            context, startTime + 20.0, &combat)) {
      char message[512] = {};
      std::snprintf(
          message, sizeof(message),
          "People combat probe %s projectile=%s "
          "available/attacker/target/move/acquire/cadence/projectile/damage/"
          "death/effects/rollback="
          "%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d",
          combat.attacker[0] == 0 ? "<none>" : combat.attacker,
          combat.projectile[0] == 0 ? "<none>" : combat.projectile,
          combat.available, combat.attackerReady, combat.targetReady,
          combat.routeDisplacement, combat.targetAcquired,
          combat.targetCadence, combat.projectileStarted,
          combat.damageDelivered, combat.deathTransition,
          combat.deathEffects, combat.rollbacks);
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                     message);
      return false;
    }
    g_state.peopleCombatProbeAvailable = combat.available;
    g_state.peopleCombatProbeAttackerReady = combat.attackerReady;
    g_state.peopleCombatProbeTargetReady = combat.targetReady;
    g_state.peopleCombatProbeRouteDisplacement = combat.routeDisplacement;
    g_state.peopleCombatProbeTargetAcquired = combat.targetAcquired;
    g_state.peopleCombatProbeTargetCadence = combat.targetCadence;
    g_state.peopleCombatProbeProjectileStarted = combat.projectileStarted;
    g_state.peopleCombatProbeDamageDelivered = combat.damageDelivered;
    g_state.peopleCombatProbeDeathTransition = combat.deathTransition;
    g_state.peopleCombatProbeDeathEffects = combat.deathEffects;
    g_state.peopleCombatProbeRollbacks = combat.rollbacks;
  }
  g_state.peopleSubjectCapacity = script.subjectCapacity;
  g_state.peopleSubjectCount = script.subjectCount;
  g_state.peopleSubjectSoundCount = PeopleSubjectState_SoundCount(context);
  if (g_state.peopleSubjectSoundCount < 0 ||
      g_state.peopleSubjectSoundCount > script.subjectCount) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_SUBJECT_ROSTER_INVALID,
                   "People-owned SoundObj roster is inconsistent");
    return false;
  }
  if (!ProbePeopleActiveWorldPersistence(
          context, script.subjectCount, g_state.peopleSubjectSoundCount,
          fingerprint))
    return false;
  SPeopleCombatScheduleSummary schedule = {};
  if (!PeopleSubjectState_AuditCombatScheduling(context, &schedule)) {
    char message[320] = {};
    std::snprintf(
        message, sizeof(message),
        "People combat scheduling "
        "live/shooters/commanded/interfaces/find/motion/attack/malformed="
        "%d/%d/%d/%d/%d/%d/%d/%d",
        schedule.livePeople, schedule.shooters,
        schedule.commandedShooters, schedule.commanderInterfaces,
        schedule.scheduledFindEnemy, schedule.scheduledMotion,
        schedule.attackStates, schedule.malformedQueues);
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_PEOPLE_LIFECYCLE_FAILURE,
                   message);
    return false;
  }
  if (!RecoveredGameplayTuning_FinalizePeopleLifecycle(context, startTime)) {
    ReportExtended(RECOVERED_ARENA_SEANCE_EXT_GAMEPLAY_TUNING_FAILURE,
                   RecoveredGameplayTuning_LastError());
    return false;
  }
  // Startup probes intentionally mutate temporary People.  Live telemetry
  // begins only after every proof and rollback has completed.
  PeopleSubjectState_ResetLiveCombatTelemetry();
  g_state.peopleSubjectFingerprint = fingerprint;
  g_state.peopleSubjectReady = true;
  return true;
}

}  // namespace

int RecoveredArenaSeance_Initialize(SimulationContext* context,
                                    double startTime) {
  RecoveredArenaSeance_Release();
  g_state = {};

  if (context == nullptr) {
    Report(RECOVERED_ARENA_SEANCE_INVALID_CONTEXT,
           "seance requires a SimulationContext");
    return FALSE;
  }

  // The 1999 code relied on the process-global CRT default seed. Preserve its
  // deterministic Windows sequence, but reset a simulation-only stream for
  // each seance so rendering cannot consume gameplay entropy.
  SimulationRandom_Reset(1u);

  BirdAttributeState_Link();
  BulletAttributeState_Link();
  BulletSubjectState_Link();
  BulletActiveWorldState_Link();
  ArtefactAttributeState_Link();
  OrphanAttributeState_Link();
  OrphanSubjectState_Link();
  PeopleSubjectState_Link();
  PeopleActiveWorldState_Link();
  CannonSubjectState_Link();
  TankSubjectState_Link();
  HowitzerSubjectState_Link();
  CommanderState_Link();
  TankGroupState_Link();
  PortalClassTable_Link();
  RecruitCenterSubjectState_Link();
  TeleportSubjectState_Link();
  SparkAttributeState_Link();
  SparkSubjectState_Link();
  SparkActiveWorldState_Link();
  SmokeActiveWorldState_Link();
  CorpseActiveWorldState_Link();
  SmokeAttributeState_Link();
  SmokeSubjectState_Link();
  SmokeVisualState_Link();
  ExplosionAttributeState_Link();
  ExplosionActiveWorldState_Link();
  VehicleAttributeState_Link();
  VehicleActiveWorldState_Link();
  TaxiAttributeState_Link();
  TaxiSubjectState_Link();
  FarterAttributeState_Link();
  FarterSubjectState_Link();
  LampAttributeState_Link();
  CorpseAttributeState_Link();
  CorpseSubjectState_Link();
  SmokerAttributeState_Link();
  SmokerSubjectState_Link();
  WAVResourceState_Link();
  SoundObjectState_Link();
  SkinResourceState_Link();
  if (!InitializeDeviceFreeSoundDistance()) return FALSE;
  if (!OpenArena(context)) return FALSE;
  if (!HowitzerSubjectState_LoadHolders()) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           HowitzerSubjectState_LastError());
    RecoveredArenaSeance_Release();
    return FALSE;
  }
  if (!InitializeSparkSubjectTable(context)) {
    RecoveredArenaSeance_Release();
    return FALSE;
  }
  if (!InitializeCorpseSubjectTable(context)) {
    RecoveredArenaSeance_Release();
    return FALSE;
  }
  if (!InitializeOrphanSubjectTable(context)) {
    RecoveredArenaSeance_Release();
    return FALSE;
  }

  try {
    TaxiSubjectScriptSummary taxiSubjectScript = {};
    PeopleScriptSummary peopleScript = {};
    TankCannonScriptSummary tankCannonScript = {};
    HowitzerScriptSummary howitzerScript = {};
    CommanderScriptSummary commanderScript = {};
    MissionProjectSummary missionProject = {};
    TeleportScriptSummary teleportScript = {};
    SRecoveredWavMetadataCatalog wavCatalog = {};
    // local_createTables() owns WAVObj before LEVEL0.SC creates attributes.
    if (!RunWavMetadataBootstrap(context, startTime, &wavCatalog)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!ReadTeleportScriptSummary(&teleportScript)) {
      ReportExtended(RECOVERED_ARENA_SEANCE_EXT_STATIC_MECHANISM_FAILURE,
                     "localmain.sci Teleport roster is malformed");
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!ReadCommanderScriptSummary(&commanderScript) ||
        !RunCommanderBootstrap(context, startTime, commanderScript) ||
        !PublishCommander(context, commanderScript) ||
        !RunMissionProjectBootstrap(context, startTime, &missionProject) ||
        !ReadPeopleScriptSummary(&peopleScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    g_state.missionProjectCapacity = missionProject.capacity;
    g_state.missionProjectNodeCapacity = missionProject.nodeCapacity;
    g_state.missionProjectHeapCapacity = missionProject.heapCapacity;
    g_state.missionProjectCount = missionProject.projectCount;
    g_state.missionProjectNodeCount = missionProject.nodeCount;
    g_state.missionProjectDataBytes = missionProject.dataBytes;
    g_state.missionProjectSummaryCount = missionProject.summaryCount;
    g_state.missionProjectPermanentCount = missionProject.permanentCount;
    g_state.missionProjectDeferredHowitzerCount =
        missionProject.deferredHowitzerCount;
    g_state.missionProjectDeferredDestroyableCount =
        missionProject.deferredDestroyableCount;
    g_state.missionProjectFingerprint = missionProject.fingerprint;
    g_state.missionProjectsReady = true;
    PeopleSubjectState_SetExpectedCapacities(
        peopleScript.attributeCapacity, peopleScript.subjectCapacity);
    if (!ReadTankCannonScriptSummary(&tankCannonScript) ||
        !ReadHowitzerScriptSummary(&howitzerScript) ||
        !InitializeTankCannonSubjectTables(context, tankCannonScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    HowitzerSubjectState_SetExpectedCapacities(
        howitzerScript.attributeCapacity, howitzerScript.subjectCapacity);
    if (!RunRouteBootstrap(context, startTime) ||
        !RunCommonAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunVehicleAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunSmokeAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunExplosionAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunTankCannonAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunTaxiAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunSmokerAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunPeopleAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    // Preserve LEVEL0.SC ownership order for this attribute-only tranche.
    if (!RunFarterAttributeBootstrap(context, startTime) ||
        !RunLampAttributeBootstrap(context, startTime) ||
        !RunCorpseAttributeBootstrap(context, startTime) ||
        !RunBulletAttributeBootstrap(context, startTime) ||
        !RunHowitzerBootstrap(context, startTime, howitzerScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RecoveredGameplayTuning_Apply(context)) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_GAMEPLAY_TUNING_FAILURE,
          RecoveredGameplayTuning_LastError());
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishSkinResources(context) ||
        !PublishSkinAnimations(context, startTime) ||
        !PublishStaticMechanisms(startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    // The January Howitzer attribute implementation resolves its Skin,
    // Corpse and Bullet references in ct_Attribute::update.  Updating every
    // table here would prematurely resolve Vehicle/People/Tank and violate
    // their staged validation boundary, so update only HowitzerAttr.
    if (!HowitzerSubjectState_ResolveReferences(context,
                                                 Session::m_moment)) {
      Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
             "HowitzerAttr reference resolution failed");
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    g_state.scriptCompleted = true;

    if (!PublishWavMetadata(context, wavCatalog) ||
        !PublishSoundObject(context, startTime) ||
        !PublishSparkAttributes(context)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishVehicleAttributes(context) ||
        !PublishBirdAttributes(context) || !PublishPortalTable() ||
        !PublishOrphanAttributes(context) ||
        !PublishArtefactAttributes(context) ||
        !PublishSmokeAttributes(context) ||
        !PublishExplosionAttributes(context) ||
        !PublishTaxiAttributes(context) ||
        !PublishSmokerAttributes(context) ||
        !PublishFarterAttributes(context) ||
        !PublishLampAttributes(context) ||
         !PublishCorpseAttributes(context) ||
         !PublishBulletAttributes(context) ||
         !PublishPeopleAttributes(context, peopleScript) ||
         !PublishTankCannonAttributes(context, tankCannonScript) ||
         !PublishHowitzerTables(context, howitzerScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishSmokeSubject(context) ||
        !PublishDynSmokerSubject(context, startTime) ||
        !PublishDependentAttributeReferences(context) ||
        !PublishFarterSubject(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishSmokeVisualResources(context) ||
        !PublishExplosionSmokeVisualResources(context)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishBulletReferences(context) ||
        !PublishVehicleReferences(context) ||
        !PublishPeopleReferences(context, startTime) ||
        !PublishTankReferences(context, startTime) ||
        !PublishTankCannonSubjectTables(context, tankCannonScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishRouteTable()) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }

    if (!PublishVehicle(context)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    bool recruitCentersPublished = false;
    if (!RunRecruitCenterBootstrap(context, startTime,
                                   &recruitCentersPublished)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (recruitCentersPublished) {
      g_state.recruitCenterCapacity = RecruitCenterSubjectState_Capacity();
      g_state.recruitCenterCount = RecruitCenterSubjectState_LiveCount();
      g_state.recruitCenterVideoCount =
          RecruitCenterSubjectState_VideoCount();
      g_state.recruitCenterDefaultTaxiCount =
          RecruitCenterSubjectState_DefaultTaxiCount();
      g_state.recruitCenterDictionaryCount =
          RecruitCenterSubjectState_DictionaryCount();
      g_state.recruitCenterFingerprint =
          RecruitCenterSubjectState_Fingerprint(context);
      if (g_state.recruitCenterFingerprint == 0) {
        Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
               "retail RecruitCenter identity is invalid");
        RecoveredArenaSeance_Release();
        return FALSE;
      }
      g_state.recruitCentersReady = true;
    }
    if (!PublishTeleportRoutes(context, startTime, teleportScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishExplosionActiveWorld(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishSparkActiveWorld(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishSmokeActiveWorld(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishCorpseActiveWorld(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishBulletActiveWorld(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishTankLifecycle(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishMissionTankLifecycle(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    // The source-only CI fixture deliberately has no model resources, so its
    // TaxiAttr graph remains unresolved just like the other deferred visual
    // frontiers. A retail level with resolved Skin/Vehicle/Corpse references
    // must create and validate every parked vehicle before becoming ready.
    if (g_state.taxiReferencesReady) {
      if (!RunTaxiSubjectBootstrap(
              context, startTime, &taxiSubjectScript) ||
          !PublishTaxiSubject(context, startTime, taxiSubjectScript)) {
        RecoveredArenaSeance_Release();
        return FALSE;
      }
    }
    if (!PublishPeopleSubject(context, startTime, peopleScript)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    // All recovered lifecycle admission probes deliberately own a pristine
    // transient graph. Public events enter only after those probes complete;
    // Apply still performs canonical EVT1 capture, while product save/load
    // exercises the full transactional replacement path.
    if (!RecoveredScriptEvents_Apply(context, startTime)) {
      ReportExtended(
          RECOVERED_ARENA_SEANCE_EXT_SCRIPT_EVENT_FAILURE,
          RecoveredScriptEvents_LastError());
      RecoveredArenaSeance_Release();
      return FALSE;
    }
  } catch (const std::bad_alloc&) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_ALLOCATION_FAILURE,
           "seance bootstrap allocation failed");
    RecoveredArenaSeance_Release();
    return FALSE;
  } catch (...) {
    Report(RECOVERED_ARENA_SEANCE_SCRIPT_PROCESS_FAILURE,
           "seance bootstrap raised an exception");
    RecoveredArenaSeance_Release();
    return FALSE;
  }

  return TRUE;
}

void RecoveredArenaSeance_Release() {
  const bool restoreSoundDistance = g_state.soundDistanceReady;
  const double previousSoundDistance = g_state.previousSoundDistance;
  const double previousSoundDistanceSquared =
      g_state.previousSoundDistanceSquared;
  RecoveredStaticMechanism_Release();
  HowitzerSubjectState_ReleaseHolders();
  RecoveredScriptEvents_Release(g_arena.getContext());
  RecoveredGameplayTuning_Release(g_arena.getContext());
  OrphanAttributeState_ClearReferences(g_arena.getContext());
  VehicleAttributeState_ClearReferences(g_arena.getContext());
  ExplosionAttributeState_ClearTraceReferences(g_arena.getContext());
  ExplosionAttributeState_ClearPieceReferences(g_arena.getContext());
  ExplosionAttributeState_ClearSmokeVisuals(g_arena.getContext());
  SmokeVisualState_Release();
  SparkAttributeState_ClearVisualResources(g_arena.getContext());
  ExplosionSubjectState_ReleaseLightFrame();
  ExplosionSubjectState_UnbindImpulseTarget(g_arena.getContext());
  ExplosionAttributeState_ClearParticleVisuals(g_arena.getContext());
  g_state.vehicleReady = false;
  g_state.vehicleActiveWorldReconstructedIDs = 0;
  g_state.vehicleActiveWorldRollbacks = 0;
  g_state.vehicleActiveWorldFingerprint = 0;
  g_state.routeReady = false;
  g_state.peopleAttributesReady = false;
  g_state.peopleReferencesReady = false;
  g_state.peopleSubjectReady = false;
  g_state.peopleActiveWorldReconstructedIDs = 0;
  g_state.peopleActiveWorldSchedulerEvents = 0;
  g_state.peopleActiveWorldRollbacks = 0;
  g_state.peopleActiveWorldFingerprint = 0;
  g_state.peopleAttributeCapacity = 0;
  g_state.peopleAttributeCount = 0;
  g_state.peopleSubjectCapacity = 0;
  g_state.peopleSubjectCount = 0;
  g_state.peopleSubjectSoundCount = 0;
  g_state.peopleAttributeFingerprint = 0;
  g_state.peopleSubjectFingerprint = 0;
  g_state.peopleProbeScheduledMoves = 0;
  g_state.peopleProbeCadenceBounded = 0;
  g_state.peopleProbeRenderedPoseFrames = 0;
  g_state.peopleProbeViewBoundaryResets = 0;
  g_state.peopleProbeBulletDamageApplications = 0;
  g_state.peopleProbeDeathTransitions = 0;
  g_state.peopleProbeSaveStateRoundTrips = 0;
  g_state.peopleProbeRollbacks = 0;
  g_state.peopleCombatProbeAvailable = 0;
  g_state.peopleCombatProbeAttackerReady = 0;
  g_state.peopleCombatProbeTargetReady = 0;
  g_state.peopleCombatProbeRouteDisplacement = 0;
  g_state.peopleCombatProbeTargetAcquired = 0;
  g_state.peopleCombatProbeTargetCadence = 0;
  g_state.peopleCombatProbeProjectileStarted = 0;
  g_state.peopleCombatProbeDamageDelivered = 0;
  g_state.peopleCombatProbeDeathTransition = 0;
  g_state.peopleCombatProbeDeathEffects = 0;
  g_state.peopleCombatProbeRollbacks = 0;
  PeopleSubjectState_SetExpectedCapacities(0, 0);
  g_state.tankCannonAttributesReady = false;
  g_state.tankReferencesReady = false;
  g_state.tankCannonSubjectTablesReady = false;
  g_state.commanderReady = false;
  g_state.missionProjectsReady = false;
  g_state.recruitCentersReady = false;
  g_state.missionTankLifecycleReady = false;
  g_state.activeWorldPersistenceReady = false;
  g_state.commanderCapacity = 0;
  g_state.commanderCount = 0;
  g_state.commanderHostileLinks = 0;
  g_state.commanderFingerprint = 0;
  g_state.missionProjectCapacity = 0;
  g_state.missionProjectNodeCapacity = 0;
  g_state.missionProjectHeapCapacity = 0;
  g_state.missionProjectCount = 0;
  g_state.missionProjectNodeCount = 0;
  g_state.missionProjectDataBytes = 0;
  g_state.missionProjectSummaryCount = 0;
  g_state.missionProjectPermanentCount = 0;
  g_state.missionProjectDeferredHowitzerCount = 0;
  g_state.missionProjectDeferredDestroyableCount = 0;
  g_state.missionProjectFingerprint = 0;
  g_state.recruitCenterCapacity = 0;
  g_state.recruitCenterCount = 0;
  g_state.recruitCenterVideoCount = 0;
  g_state.recruitCenterDefaultTaxiCount = 0;
  g_state.recruitCenterDictionaryCount = 0;
  g_state.recruitCenterFingerprint = 0;
  g_state.tankGroupSubjectCapacity = 0;
  g_state.missionTankAvailable = 0;
  g_state.missionTankSpawns = 0;
  g_state.missionTankMembershipLinks = 0;
  g_state.missionTankFindEnemyCycles = 0;
  g_state.missionTankMovingCycles = 0;
  g_state.missionTankStableRoundTrips = 0;
  g_state.missionTankReconstructedIDs = 0;
  g_state.missionTankRollbacks = 0;
  g_state.missionTankFingerprint = 0;
  g_state.activeWorldSections = 0;
  g_state.activeWorldEvents = 0;
  g_state.activeWorldOwnerPhases = 0;
  g_state.activeWorldReferencePhases = 0;
  g_state.activeWorldEventPhases = 0;
  g_state.activeWorldCreatedOwners = 0;
  g_state.activeWorldMissionRecords = 0;
  g_state.activeWorldMissionConditionReferences = 0;
  g_state.activeWorldMissionRouteReferences = 0;
  g_state.activeWorldMissionCheckEvents = 0;
  g_state.activeWorldClockRecords = 0;
  g_state.activeWorldRngAlgorithm = 0;
  g_state.activeWorldRngStateBytes = 0;
  g_state.activeWorldRngDrawCount = 0;
  g_state.activeWorldCorruptionRejects = 0;
  g_state.activeWorldRollbacks = 0;
  g_state.activeWorldContainerBytes = 0;
  g_state.activeWorldFingerprint = 0;
  g_state.cannonAttributeCapacity = 0;
  g_state.cannonAttributeCount = 0;
  g_state.cannonSubjectCapacity = 0;
  g_state.cannonSubjectCount = 0;
  g_state.tankAttributeCapacity = 0;
  g_state.tankAttributeCount = 0;
  g_state.tankSubjectCapacity = 0;
  g_state.tankSubjectCount = 0;
  g_state.cannonAttributeFingerprint = 0;
  g_state.cannonSubjectFingerprint = 0;
  g_state.tankAttributeFingerprint = 0;
  g_state.tankSubjectFingerprint = 0;
  g_state.tankProbeAvailable = 0;
  g_state.tankProbeValidStarts = 0;
  g_state.tankProbeDynamicReady = 0;
  g_state.tankProbeRenderReady = 0;
  g_state.tankProbeCannonReady = 0;
  g_state.tankProbeScheduledMoves = 0;
  g_state.tankProbeCadenceBounded = 0;
  g_state.tankProbeRenderedPoseFrames = 0;
  g_state.tankProbeViewBoundaryResets = 0;
  g_state.tankProbeBulletDamageApplications = 0;
  g_state.tankProbeDeathTransitions = 0;
  g_state.tankProbeDeathEffects = 0;
  g_state.tankProbeSaveStateRoundTrips = 0;
  g_state.tankProbeRollbacks = 0;
  CannonSubjectState_SetExpectedCapacities(0, 0);
  TankSubjectState_SetExpectedCapacities(0, 0);
  g_state.sparkAttributesReady = false;
  g_state.sparkSubjectReady = false;
  g_state.sparkVisualResourcesReady = false;
  g_state.sparkActiveWorldReady = false;
  g_state.smokeActiveWorldReady = false;
  g_state.corpseActiveWorldReady = false;
  g_state.sparkSubjectCapacity = 0;
  g_state.sparkSubjectFingerprint = 0;
  g_state.sparkVisualResourceFingerprint = 0;
  g_state.sparkProbeInvalidStarts = 0;
  g_state.sparkProbeQueuedCreates = 0;
  g_state.sparkProbeQueueRollbacks = 0;
  g_state.sparkProbePhaseTransitions = 0;
  g_state.sparkProbeExpirations = 0;
  g_state.sparkActiveWorldCapturedOwners = 0;
  g_state.sparkActiveWorldSchedulerEvents = 0;
  g_state.sparkActiveWorldRollbacks = 0;
  g_state.sparkActiveWorldReconstructedIDs = 0;
  g_state.sparkActiveWorldStableRoundTrips = 0;
  g_state.sparkActiveWorldResumedPhases = 0;
  g_state.sparkActiveWorldFingerprint = 0;
  g_state.smokeActiveWorldCapturedOwners = 0;
  g_state.smokeActiveWorldCapturedBlobs = 0;
  g_state.smokeActiveWorldSchedulerEvents = 0;
  g_state.smokeActiveWorldRollbacks = 0;
  g_state.smokeActiveWorldReconstructedIDs = 0;
  g_state.smokeActiveWorldStableRoundTrips = 0;
  g_state.smokeActiveWorldResumedMoves = 0;
  g_state.smokeActiveWorldFingerprint = 0;
  g_state.corpseActiveWorldCapturedOwners = 0;
  g_state.corpseActiveWorldOwnedSmokers = 0;
  g_state.corpseActiveWorldSchedulerEvents = 0;
  g_state.corpseActiveWorldRollbacks = 0;
  g_state.corpseActiveWorldReconstructedObjects = 0;
  g_state.corpseActiveWorldStableRoundTrips = 0;
  g_state.corpseActiveWorldResumedEmissions = 0;
  g_state.corpseActiveWorldResumedDeaths = 0;
  g_state.corpseActiveWorldFingerprint = 0;
  g_state.smokeAttributesReady = false;
  g_state.smokeSubjectReady = false;
  g_state.smokeSubjectCapacity = 0;
  g_state.smokeSubjectFingerprint = 0;
  g_state.smokeVisualResourcesReady = false;
  g_state.smokeVisualResourceFingerprint = 0;
  g_state.explosionAttributesReady = false;
  g_state.explosionSubjectReady = false;
  g_state.explosionImpulseReady = false;
  g_state.explosionLightReady = false;
  g_state.explosionSoundReady = false;
  g_state.explosionParticlesReady = false;
  g_state.explosionSmokeReady = false;
  g_state.explosionPieceReady = false;
  g_state.explosionTraceReady = false;
  g_state.explosionActiveWorldReady = false;
  g_state.explosionSubjectCapacity = 0;
  g_state.explosionSubjectFingerprint = 0;
  g_state.explosionProbeInvalidStarts = 0;
  g_state.explosionProbeAllocationRollbacks = 0;
  g_state.explosionProbeQueuedCommands = 0;
  g_state.explosionProbeQueueRollbacks = 0;
  g_state.explosionProbeExecutedCommands = 0;
  g_state.explosionProbeDamageApplications = 0;
  g_state.explosionSoundReferenceFingerprint = 0;
  g_state.explosionSoundProbeStarted = 0;
  g_state.explosionSoundProbeDependencySkips = 0;
  g_state.explosionSoundProbeRollbacks = 0;
  g_state.explosionParticleVisualFingerprint = 0;
  g_state.explosionParticleProbeStartedBranches = 0;
  g_state.explosionParticleProbeSimpleParticles = 0;
  g_state.explosionParticleProbeSnakeParticles = 0;
  g_state.explosionParticleProbeRays = 0;
  g_state.explosionParticleProbeDependencySkips = 0;
  g_state.explosionParticleProbeMoveSteps = 0;
  g_state.explosionParticleProbeExpiredParents = 0;
  g_state.explosionParticleProbeRolledBackBranches = 0;
  g_state.explosionSmokeVisualFingerprint = 0;
  g_state.explosionSmokeProbeStartedSprites = 0;
  g_state.explosionSmokeProbeDependencySkips = 0;
  g_state.explosionSmokeProbeMoveSteps = 0;
  g_state.explosionSmokeProbeExpiredParents = 0;
  g_state.explosionSmokeProbeRolledBackSprites = 0;
  g_state.explosionPieceReferenceFingerprint = 0;
  g_state.explosionPieceProbeStartedPieces = 0;
  g_state.explosionPieceProbeDependencySkips = 0;
  g_state.explosionPieceProbeMoveSteps = 0;
  g_state.explosionPieceProbeExpiredParents = 0;
  g_state.explosionPieceProbeRolledBackPieces = 0;
  g_state.explosionTraceReferenceFingerprint = 0;
  g_state.explosionTraceProbeStartedPieces = 0;
  g_state.explosionTraceProbeQuotaGateSkips = 0;
  g_state.explosionTraceProbePuffEvents = 0;
  g_state.explosionTraceProbeSmokeChildren = 0;
  g_state.explosionTraceProbeMoveSteps = 0;
  g_state.explosionTraceProbeExpiredParents = 0;
  g_state.explosionTraceProbeRolledBackPieces = 0;
  g_state.explosionActiveWorldCapturedOwners = 0;
  g_state.explosionActiveWorldCapturedBranches = 0;
  g_state.explosionActiveWorldSchedulerEvents = 0;
  g_state.explosionActiveWorldSoundChildren = 0;
  g_state.explosionActiveWorldRollbacks = 0;
  g_state.explosionActiveWorldReconstructedIDs = 0;
  g_state.explosionActiveWorldStableRoundTrips = 0;
  g_state.explosionActiveWorldResumedMoves = 0;
  g_state.explosionActiveWorldFingerprint = 0;
  g_state.vehicleAttributesReady = false;
  g_state.vehicleReferencesReady = false;
  g_state.vehicleAttributeCount = 0;
  g_state.vehicleAttributeCapacity = 0;
  g_state.vehicleAttributeFingerprint = 0;
  g_state.vehicleReferenceFingerprint = 0;
  VehicleAttributeState_SetCapacity(0);
  g_state.taxiAttributesReady = false;
  g_state.taxiReferencesReady = false;
  g_state.taxiReferenceFingerprint = 0;
  g_state.taxiSubjectReady = false;
  g_state.taxiSubjectCapacity = 0;
  g_state.taxiSubjectCount = 0;
  g_state.taxiSubjectSoundCount = 0;
  g_state.taxiSubjectFingerprint = 0;
  g_state.taxiProbeInvalidStarts = 0;
  g_state.taxiProbeValidStarts = 0;
  g_state.taxiProbeRenderReady = 0;
  g_state.taxiProbeSoundReady = 0;
  g_state.taxiProbeRollbacks = 0;
  g_state.bulletAttributesReady = false;
  g_state.bulletReferencesReady = false;
  g_state.bulletSubjectRegistrationReady = false;
  g_state.bulletSubjectReady = false;
  g_state.bulletImpactEffectsReady = false;
  g_state.bulletGroundSparkReady = false;
  g_state.bulletBarrelSmokeReady = false;
  g_state.bulletActiveWorldReady = false;
  g_state.bulletAttributeCount = 0;
  g_state.bulletAttributeCapacity = 0;
  g_state.bulletSubjectCapacity = 0;
  g_state.bulletAttributeFingerprint = 0;
  g_state.bulletReferenceFingerprint = 0;
  g_state.bulletSubjectFingerprint = 0;
  g_state.bulletSubjectProbeMoveCount = 0;
  g_state.bulletCollisionScheduledChecks = 0;
  g_state.bulletCollisionExecutedChecks = 0;
  g_state.bulletCollisionSphereCases = 0;
  g_state.bulletCollisionEarliestHitCases = 0;
  g_state.bulletCollisionWaterlineCases = 0;
  g_state.bulletCollisionSceneQueries = 0;
  g_state.bulletEffectQueuedBatches = 0;
  g_state.bulletEffectQueuedChildren = 0;
  g_state.bulletEffectSplashFirstCases = 0;
  g_state.bulletEffectRolledBackChildren = 0;
  g_state.bulletGroundSparkQueued = 0;
  g_state.bulletGroundSparkRolledBack = 0;
  g_state.bulletBarrelSmokeThresholdStarts = 0;
  g_state.bulletBarrelSmokeFrameGateSkips = 0;
  g_state.bulletBarrelSmokeAttributeGateSkips = 0;
  g_state.bulletBarrelSmokeRollbacks = 0;
  g_state.bulletActiveWorldCapturedOwners = 0;
  g_state.bulletActiveWorldSchedulerEvents = 0;
  g_state.bulletActiveWorldRollbacks = 0;
  g_state.bulletActiveWorldReconstructedIDs = 0;
  g_state.bulletActiveWorldStableRoundTrips = 0;
  g_state.bulletActiveWorldResumedMoves = 0;
  g_state.bulletActiveWorldTombstonedMasters = 0;
  g_state.bulletActiveWorldFingerprint = 0;
  g_state.farterAttributesReady = false;
  g_state.farterReferencesReady = false;
  g_state.farterRuntimeReady = false;
  g_state.farterSubjectReady = false;
  g_state.farterSubjectCapacity = 0;
  g_state.farterSubjectFingerprint = 0;
  g_state.farterScriptObjectCount = 0;
  g_state.farterReferenceFingerprint = 0;
  g_state.lampAttributesReady = false;
  g_state.corpseAttributesReady = false;
  g_state.corpseReferencesReady = false;
  g_state.corpseRuntimeReady = false;
  g_state.corpseReferenceFingerprint = 0;
  g_state.corpseSubjectReady = false;
  g_state.corpseSubjectCapacity = 0;
  g_state.corpseSubjectFingerprint = 0;
  g_state.smokerAttributesReady = false;
  g_state.smokerReferencesReady = false;
  g_state.smokerRuntimeReady = false;
  g_state.smokerReferenceFingerprint = 0;
  g_state.dynSmokerReady = false;
  g_state.dynSmokerCapacity = 0;
  g_state.dynSmokerFingerprint = 0;
  g_state.wavMetadataReady = false;
  g_state.wavMetadataCount = 0;
  g_state.wavMetadataCapacity = 0;
  g_state.wavCatalogFingerprint = 0;
  g_state.wavResourceFingerprint = 0;
  g_state.soundObjectReady = false;
  g_state.soundObjectCapacity = 0;
  g_state.soundObjectFingerprint = 0;
  g_state.farterLiveObjectCount = 0;
  g_state.farterSoundObjectCount = 0;
  g_state.farterNearFrameAudibleCount = 0;
  g_state.farterFarFrameAudibleCount = 0;
  g_state.farterAudibleFrameTransition = false;
  g_state.skinResourcesReady = false;
  g_state.skinAnimationsReady = false;
  g_state.skinModelCount = 0;
  g_state.skinSpriteCount = 0;
  g_state.skinCatalogFingerprint = 0;
  g_state.skinResourceFingerprint = 0;
  g_state.skinAnimationEntryCallCount = 0;
  g_state.skinAnimatedModelCount = 0;
  g_state.skinAnimationCommandCount = 0;
  g_state.skinAnimationSourceFingerprint = 0;
  g_state.skinAnimationStateFingerprint = 0;
  g_state.skinAnimationPoseTemporalModelCount = 0;
  g_state.skinAnimationPoseChangedModelCount = 0;
  g_state.skinAnimationPoseSampleCount = 0;
  g_state.skinAnimationPoseRestoredModifierCount = 0;
  g_state.skinAnimationPoseFingerprint = 0;
  g_state.staticMechanismsReady = false;
  g_state.staticMechanismTargetLevel = false;
  g_state.staticMechanismLevelOne = false;
  g_state.staticMechanismLevelFive = false;
  g_state.staticMechanismBindingCount = 0;
  g_state.staticMechanismWaterwheelCount = 0;
  g_state.staticMechanismFlagCount = 0;
  g_state.staticMechanismRotatingCount = 0;
  g_state.staticMechanismDoorCount = 0;
  g_state.staticMechanismPol16Count = 0;
  g_state.staticMechanismChangedBindingCount = 0;
  g_state.staticMechanismPoseSampleCount = 0;
  g_state.staticMechanismRestoredModifierCount = 0;
  g_state.staticMechanismFingerprint = 0;
  g_state.artefactAttributesReady = false;
  g_state.orphanAttributesReady = false;
  g_state.orphanReferencesReady = false;
  g_state.orphanReferenceFingerprint = 0;
  g_state.orphanSubjectReady = false;
  g_state.orphanSubjectCapacity = 0;
  g_state.orphanSubjectFingerprint = 0;
  g_state.portalReady = false;
  g_state.teleportRoutesReady = false;
  g_state.teleportTargetLevel = false;
  g_state.teleportCapacity = 0;
  g_state.teleportRouteCount = 0;
  g_state.teleportProbeRejectedNonPlayer = 0;
  g_state.teleportProbePhysicsCollisions = 0;
  g_state.teleportProbeAppliedPlayer = 0;
  g_state.teleportProbeVehicleRollbacks = 0;
  g_state.teleportFingerprint = 0;
  g_state.birdAttributesReady = false;
  g_state.scriptCompleted = false;
  g_vehicle = nullptr;
  if (g_state.arenaOpen) {
    g_arena.closeSeance();
    g_state.arenaOpen = false;
  }
  if (restoreSoundDistance) {
    snd_distMax = previousSoundDistance;
    snd_distMax2 = previousSoundDistanceSquared;
  }
  g_state.soundDistanceReady = false;
  g_state.previousSoundDistance = 0.0;
  g_state.previousSoundDistanceSquared = 0.0;
  g_state.soundDistance = 0.0;
  g_state.soundDistanceSquared = 0.0;
}

bool RecoveredArenaSeance_IsOpen() { return g_state.arenaOpen; }

bool RecoveredArenaSeance_ScriptCompleted() {
  return g_state.scriptCompleted;
}

bool RecoveredArenaSeance_RouteReady() { return g_state.routeReady; }

bool RecoveredArenaSeance_PeopleAttributesReady() {
  return g_state.peopleAttributesReady;
}

bool RecoveredArenaSeance_PeopleReferencesReady() {
  return g_state.peopleReferencesReady;
}

bool RecoveredArenaSeance_PeopleSubjectReady() {
  return g_state.peopleSubjectReady;
}

int RecoveredArenaSeance_PeopleAttributeCapacity() {
  return g_state.peopleAttributesReady ? g_state.peopleAttributeCapacity : -1;
}

int RecoveredArenaSeance_PeopleAttributeCount() {
  return g_state.peopleAttributesReady ? g_state.peopleAttributeCount : -1;
}

int RecoveredArenaSeance_PeopleSubjectCapacity() {
  return g_state.peopleSubjectReady ? g_state.peopleSubjectCapacity : -1;
}

int RecoveredArenaSeance_PeopleSubjectCount() {
  return g_state.peopleSubjectReady ? g_state.peopleSubjectCount : -1;
}

int RecoveredArenaSeance_PeopleSubjectSoundCount() {
  return g_state.peopleSubjectReady ? g_state.peopleSubjectSoundCount : -1;
}

unsigned long long RecoveredArenaSeance_PeopleAttributeFingerprint() {
  return g_state.peopleAttributesReady ? g_state.peopleAttributeFingerprint : 0;
}

unsigned long long RecoveredArenaSeance_PeopleSubjectFingerprint() {
  return g_state.peopleSubjectReady ? g_state.peopleSubjectFingerprint : 0;
}

int RecoveredArenaSeance_PeopleProbeScheduledMoves() {
  return g_state.peopleSubjectReady ? g_state.peopleProbeScheduledMoves : -1;
}

int RecoveredArenaSeance_PeopleProbeCadenceBounded() {
  return g_state.peopleSubjectReady ? g_state.peopleProbeCadenceBounded : -1;
}

int RecoveredArenaSeance_PeopleProbeRenderedPoseFrames() {
  return g_state.peopleSubjectReady ? g_state.peopleProbeRenderedPoseFrames
                                    : -1;
}

int RecoveredArenaSeance_PeopleProbeViewBoundaryResets() {
  return g_state.peopleSubjectReady ? g_state.peopleProbeViewBoundaryResets
                                    : -1;
}

int RecoveredArenaSeance_PeopleProbeBulletDamageApplications() {
  return g_state.peopleSubjectReady
             ? g_state.peopleProbeBulletDamageApplications
             : -1;
}

int RecoveredArenaSeance_PeopleProbeDeathTransitions() {
  return g_state.peopleSubjectReady ? g_state.peopleProbeDeathTransitions : -1;
}

int RecoveredArenaSeance_PeopleProbeSaveStateRoundTrips() {
  return g_state.peopleSubjectReady
             ? g_state.peopleProbeSaveStateRoundTrips
             : -1;
}

int RecoveredArenaSeance_PeopleProbeRollbacks() {
  return g_state.peopleSubjectReady ? g_state.peopleProbeRollbacks : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeAvailable() {
  return g_state.peopleSubjectReady ? g_state.peopleCombatProbeAvailable : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeAttackerReady() {
  return g_state.peopleSubjectReady
             ? g_state.peopleCombatProbeAttackerReady
             : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeTargetReady() {
  return g_state.peopleSubjectReady ? g_state.peopleCombatProbeTargetReady
                                    : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeRouteDisplacement() {
  return g_state.peopleSubjectReady
             ? g_state.peopleCombatProbeRouteDisplacement
             : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeTargetAcquired() {
  return g_state.peopleSubjectReady
             ? g_state.peopleCombatProbeTargetAcquired
             : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeTargetCadence() {
  return g_state.peopleSubjectReady ? g_state.peopleCombatProbeTargetCadence
                                    : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeProjectileStarted() {
  return g_state.peopleSubjectReady
             ? g_state.peopleCombatProbeProjectileStarted
             : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeDamageDelivered() {
  return g_state.peopleSubjectReady
             ? g_state.peopleCombatProbeDamageDelivered
             : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeDeathTransition() {
  return g_state.peopleSubjectReady
             ? g_state.peopleCombatProbeDeathTransition
             : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeDeathEffects() {
  return g_state.peopleSubjectReady ? g_state.peopleCombatProbeDeathEffects
                                    : -1;
}

int RecoveredArenaSeance_PeopleCombatProbeRollbacks() {
  return g_state.peopleSubjectReady ? g_state.peopleCombatProbeRollbacks : -1;
}

int RecoveredArenaSeance_PeopleActiveWorldReconstructedIDs() {
  return g_state.peopleSubjectReady
             ? g_state.peopleActiveWorldReconstructedIDs
             : -1;
}

int RecoveredArenaSeance_PeopleActiveWorldSchedulerEvents() {
  return g_state.peopleSubjectReady
             ? g_state.peopleActiveWorldSchedulerEvents
             : -1;
}

int RecoveredArenaSeance_PeopleActiveWorldRollbacks() {
  return g_state.peopleSubjectReady ? g_state.peopleActiveWorldRollbacks : -1;
}

unsigned long long RecoveredArenaSeance_PeopleActiveWorldFingerprint() {
  return g_state.peopleSubjectReady
             ? g_state.peopleActiveWorldFingerprint
             : 0;
}

bool RecoveredArenaSeance_TankCannonAttributesReady() {
  return g_state.tankCannonAttributesReady;
}

bool RecoveredArenaSeance_TankReferencesReady() {
  return g_state.tankReferencesReady;
}

bool RecoveredArenaSeance_TankCannonSubjectTablesReady() {
  return g_state.tankCannonSubjectTablesReady;
}

int RecoveredArenaSeance_CannonAttributeCapacity() {
  return g_state.tankCannonAttributesReady ? g_state.cannonAttributeCapacity
                                           : -1;
}

int RecoveredArenaSeance_CannonAttributeCount() {
  return g_state.tankCannonAttributesReady ? g_state.cannonAttributeCount : -1;
}

int RecoveredArenaSeance_CannonSubjectCapacity() {
  return g_state.tankCannonSubjectTablesReady ? g_state.cannonSubjectCapacity
                                              : -1;
}

int RecoveredArenaSeance_CannonSubjectCount() {
  return g_state.tankCannonSubjectTablesReady ? g_state.cannonSubjectCount : -1;
}

unsigned long long RecoveredArenaSeance_CannonAttributeFingerprint() {
  return g_state.tankCannonAttributesReady
             ? g_state.cannonAttributeFingerprint
             : 0;
}

unsigned long long RecoveredArenaSeance_CannonSubjectFingerprint() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.cannonSubjectFingerprint
             : 0;
}

int RecoveredArenaSeance_TankAttributeCapacity() {
  return g_state.tankCannonAttributesReady ? g_state.tankAttributeCapacity
                                           : -1;
}

int RecoveredArenaSeance_TankAttributeCount() {
  return g_state.tankCannonAttributesReady ? g_state.tankAttributeCount : -1;
}

int RecoveredArenaSeance_TankSubjectCapacity() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankSubjectCapacity
                                              : -1;
}

int RecoveredArenaSeance_TankSubjectCount() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankSubjectCount : -1;
}

unsigned long long RecoveredArenaSeance_TankAttributeFingerprint() {
  return g_state.tankCannonAttributesReady ? g_state.tankAttributeFingerprint
                                           : 0;
}

unsigned long long RecoveredArenaSeance_TankSubjectFingerprint() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankSubjectFingerprint
                                              : 0;
}

int RecoveredArenaSeance_TankProbeAvailable() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeAvailable : -1;
}

int RecoveredArenaSeance_TankProbeValidStarts() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeValidStarts
                                              : -1;
}

int RecoveredArenaSeance_TankProbeDynamicReady() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeDynamicReady
                                              : -1;
}

int RecoveredArenaSeance_TankProbeRenderReady() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeRenderReady
                                              : -1;
}

int RecoveredArenaSeance_TankProbeCannonReady() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeCannonReady
                                              : -1;
}

int RecoveredArenaSeance_TankProbeScheduledMoves() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeScheduledMoves
                                              : -1;
}

int RecoveredArenaSeance_TankProbeCadenceBounded() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeCadenceBounded
                                              : -1;
}

int RecoveredArenaSeance_TankProbeRenderedPoseFrames() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.tankProbeRenderedPoseFrames
             : -1;
}

int RecoveredArenaSeance_TankProbeViewBoundaryResets() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.tankProbeViewBoundaryResets
             : -1;
}

int RecoveredArenaSeance_TankProbeBulletDamageApplications() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.tankProbeBulletDamageApplications
             : -1;
}

int RecoveredArenaSeance_TankProbeDeathTransitions() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.tankProbeDeathTransitions
             : -1;
}

int RecoveredArenaSeance_TankProbeDeathEffects() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeDeathEffects
                                              : -1;
}

int RecoveredArenaSeance_TankProbeSaveStateRoundTrips() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.tankProbeSaveStateRoundTrips
             : -1;
}

int RecoveredArenaSeance_TankProbeRollbacks() {
  return g_state.tankCannonSubjectTablesReady ? g_state.tankProbeRollbacks
                                              : -1;
}

bool RecoveredArenaSeance_CommanderReady() {
  return g_state.commanderReady;
}

int RecoveredArenaSeance_CommanderCapacity() {
  return g_state.commanderReady ? g_state.commanderCapacity : -1;
}

int RecoveredArenaSeance_CommanderCount() {
  return g_state.commanderReady ? g_state.commanderCount : -1;
}

int RecoveredArenaSeance_CommanderHostileLinks() {
  return g_state.commanderReady ? g_state.commanderHostileLinks : -1;
}

unsigned long long RecoveredArenaSeance_CommanderFingerprint() {
  return g_state.commanderReady ? g_state.commanderFingerprint : 0;
}

bool RecoveredArenaSeance_MissionProjectsReady() {
  return g_state.missionProjectsReady;
}

int RecoveredArenaSeance_MissionProjectCapacity() {
  return g_state.missionProjectsReady ? g_state.missionProjectCapacity : -1;
}

int RecoveredArenaSeance_MissionProjectNodeCapacity() {
  return g_state.missionProjectsReady ? g_state.missionProjectNodeCapacity
                                      : -1;
}

int RecoveredArenaSeance_MissionProjectHeapCapacity() {
  return g_state.missionProjectsReady ? g_state.missionProjectHeapCapacity
                                      : -1;
}

int RecoveredArenaSeance_MissionProjectCount() {
  return g_state.missionProjectsReady ? g_state.missionProjectCount : -1;
}

int RecoveredArenaSeance_MissionProjectNodeCount() {
  return g_state.missionProjectsReady ? g_state.missionProjectNodeCount : -1;
}

int RecoveredArenaSeance_MissionProjectDataBytes() {
  return g_state.missionProjectsReady ? g_state.missionProjectDataBytes : -1;
}

int RecoveredArenaSeance_MissionProjectSummaryCount() {
  return g_state.missionProjectsReady ? g_state.missionProjectSummaryCount
                                      : -1;
}

int RecoveredArenaSeance_MissionProjectPermanentCount() {
  return g_state.missionProjectsReady ? g_state.missionProjectPermanentCount
                                      : -1;
}

int RecoveredArenaSeance_MissionProjectDeferredHowitzerCount() {
  return g_state.missionProjectsReady
             ? g_state.missionProjectDeferredHowitzerCount
             : -1;
}

int RecoveredArenaSeance_MissionProjectDeferredDestroyableCount() {
  return g_state.missionProjectsReady
             ? g_state.missionProjectDeferredDestroyableCount
             : -1;
}

unsigned long long RecoveredArenaSeance_MissionProjectFingerprint() {
  return g_state.missionProjectsReady ? g_state.missionProjectFingerprint : 0;
}

bool RecoveredArenaSeance_RecruitCentersReady() {
  return g_state.recruitCentersReady;
}

int RecoveredArenaSeance_RecruitCenterCapacity() {
  return g_state.recruitCentersReady ? g_state.recruitCenterCapacity : -1;
}

int RecoveredArenaSeance_RecruitCenterCount() {
  return g_state.recruitCentersReady ? g_state.recruitCenterCount : -1;
}

int RecoveredArenaSeance_RecruitCenterVideoCount() {
  return g_state.recruitCentersReady ? g_state.recruitCenterVideoCount : -1;
}

int RecoveredArenaSeance_RecruitCenterDefaultTaxiCount() {
  return g_state.recruitCentersReady
             ? g_state.recruitCenterDefaultTaxiCount : -1;
}

int RecoveredArenaSeance_RecruitCenterDictionaryCount() {
  return g_state.recruitCentersReady
             ? g_state.recruitCenterDictionaryCount : -1;
}

int RecoveredArenaSeance_RecruitCenterRejectedCollisions() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_RejectedCollisionCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterPlayerCollisions() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_PlayerCollisionCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterAdmissions() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_AdmissionCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterStagedMissions() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_StagedMissionCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterExistingMissionVisits() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_ExistingMissionVisitCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterNoProjectVisits() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_NoProjectVisitCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterEjections() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_EjectionCount() : -1;
}

int RecoveredArenaSeance_RecruitCenterAdmissionFailures() {
  return g_state.recruitCentersReady
             ? RecruitCenterSubjectState_AdmissionFailureCount() : -1;
}

unsigned long long RecoveredArenaSeance_RecruitCenterFingerprint() {
  return g_state.recruitCentersReady ? g_state.recruitCenterFingerprint : 0;
}

bool RecoveredArenaSeance_MissionTankLifecycleReady() {
  return g_state.missionTankLifecycleReady;
}

int RecoveredArenaSeance_TankGroupSubjectCapacity() {
  return g_state.tankCannonSubjectTablesReady
             ? g_state.tankGroupSubjectCapacity : -1;
}

int RecoveredArenaSeance_MissionTankAvailable() {
  return g_state.missionTankLifecycleReady ? g_state.missionTankAvailable : -1;
}

int RecoveredArenaSeance_MissionTankSpawns() {
  return g_state.missionTankLifecycleReady ? g_state.missionTankSpawns : -1;
}

int RecoveredArenaSeance_MissionTankMembershipLinks() {
  return g_state.missionTankLifecycleReady
             ? g_state.missionTankMembershipLinks : -1;
}

int RecoveredArenaSeance_MissionTankFindEnemyCycles() {
  return g_state.missionTankLifecycleReady
             ? g_state.missionTankFindEnemyCycles : -1;
}

int RecoveredArenaSeance_MissionTankMovingCycles() {
  return g_state.missionTankLifecycleReady
             ? g_state.missionTankMovingCycles : -1;
}

int RecoveredArenaSeance_MissionTankStableRoundTrips() {
  return g_state.missionTankLifecycleReady
             ? g_state.missionTankStableRoundTrips : -1;
}

int RecoveredArenaSeance_MissionTankReconstructedIDs() {
  return g_state.missionTankLifecycleReady
             ? g_state.missionTankReconstructedIDs : -1;
}

int RecoveredArenaSeance_MissionTankRollbacks() {
  return g_state.missionTankLifecycleReady ? g_state.missionTankRollbacks : -1;
}

unsigned long long RecoveredArenaSeance_MissionTankFingerprint() {
  return g_state.missionTankLifecycleReady ? g_state.missionTankFingerprint : 0;
}

bool RecoveredArenaSeance_ActiveWorldPersistenceReady() {
  return g_state.activeWorldPersistenceReady;
}

int RecoveredArenaSeance_ActiveWorldFormatVersion() {
  return g_state.activeWorldPersistenceReady
             ? static_cast<int>(ActiveWorldSave_FormatVersion())
             : 0;
}

int RecoveredArenaSeance_ActiveWorldEngineCompatibility() {
  return g_state.activeWorldPersistenceReady
             ? static_cast<int>(
                   ActiveWorldSave_EngineCompatibilityVersion())
             : 0;
}

int RecoveredArenaSeance_ActiveWorldSections() {
  return g_state.activeWorldPersistenceReady ? g_state.activeWorldSections : -1;
}

int RecoveredArenaSeance_ActiveWorldEvents() {
  return g_state.activeWorldPersistenceReady ? g_state.activeWorldEvents : -1;
}

int RecoveredArenaSeance_ActiveWorldOwnerPhases() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldOwnerPhases
             : -1;
}

int RecoveredArenaSeance_ActiveWorldReferencePhases() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldReferencePhases
             : -1;
}

int RecoveredArenaSeance_ActiveWorldEventPhases() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldEventPhases
             : -1;
}

int RecoveredArenaSeance_ActiveWorldCreatedOwners() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldCreatedOwners
             : -1;
}

int RecoveredArenaSeance_ActiveWorldMissionRecords() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldMissionRecords
             : -1;
}

int RecoveredArenaSeance_ActiveWorldMissionConditionReferences() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldMissionConditionReferences
             : -1;
}

int RecoveredArenaSeance_ActiveWorldMissionRouteReferences() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldMissionRouteReferences
             : -1;
}

int RecoveredArenaSeance_ActiveWorldMissionCheckEvents() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldMissionCheckEvents
             : -1;
}

int RecoveredArenaSeance_ActiveWorldClockRecords() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldClockRecords
             : -1;
}

unsigned int RecoveredArenaSeance_ActiveWorldRngAlgorithm() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldRngAlgorithm
             : 0;
}

int RecoveredArenaSeance_ActiveWorldRngStateBytes() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldRngStateBytes
             : -1;
}

unsigned long long RecoveredArenaSeance_ActiveWorldRngDrawCount() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldRngDrawCount
             : 0;
}

int RecoveredArenaSeance_ActiveWorldCorruptionRejects() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldCorruptionRejects
             : -1;
}

int RecoveredArenaSeance_ActiveWorldRollbacks() {
  return g_state.activeWorldPersistenceReady ? g_state.activeWorldRollbacks
                                             : -1;
}

unsigned long long RecoveredArenaSeance_ActiveWorldContainerBytes() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldContainerBytes
             : 0;
}

unsigned long long RecoveredArenaSeance_ActiveWorldFingerprint() {
  return g_state.activeWorldPersistenceReady
             ? g_state.activeWorldFingerprint
             : 0;
}

bool RecoveredArenaSeance_BirdAttributesReady() {
  return g_state.birdAttributesReady;
}

bool RecoveredArenaSeance_PortalReady() { return g_state.portalReady; }

bool RecoveredArenaSeance_TeleportRoutesReady() {
  return g_state.teleportRoutesReady;
}

bool RecoveredArenaSeance_TeleportTargetLevel() {
  return g_state.teleportTargetLevel;
}

int RecoveredArenaSeance_TeleportCapacity() {
  return g_state.teleportCapacity;
}

int RecoveredArenaSeance_TeleportRouteCount() {
  return g_state.teleportRouteCount;
}

int RecoveredArenaSeance_TeleportProbeRejectedNonPlayer() {
  return g_state.teleportProbeRejectedNonPlayer;
}

int RecoveredArenaSeance_TeleportProbePhysicsCollisions() {
  return g_state.teleportProbePhysicsCollisions;
}

int RecoveredArenaSeance_TeleportProbeAppliedPlayer() {
  return g_state.teleportProbeAppliedPlayer;
}

int RecoveredArenaSeance_TeleportProbeVehicleRollbacks() {
  return g_state.teleportProbeVehicleRollbacks;
}

unsigned long long RecoveredArenaSeance_TeleportFingerprint() {
  return g_state.teleportFingerprint;
}

bool RecoveredArenaSeance_OrphanAttributesReady() {
  return g_state.orphanAttributesReady;
}

bool RecoveredArenaSeance_OrphanReferencesReady() {
  return g_state.orphanReferencesReady;
}

unsigned long long RecoveredArenaSeance_OrphanReferenceFingerprint() {
  return g_state.orphanReferencesReady
             ? g_state.orphanReferenceFingerprint
             : 0;
}

bool RecoveredArenaSeance_OrphanSubjectReady() {
  return g_state.orphanSubjectReady;
}

int RecoveredArenaSeance_OrphanSubjectCapacity() {
  return g_state.orphanSubjectReady ? g_state.orphanSubjectCapacity : 0;
}

int RecoveredArenaSeance_OrphanSubjectCount() {
  return g_state.orphanSubjectReady ? OrphanSubjectState_LiveCount() : 0;
}

unsigned long long RecoveredArenaSeance_OrphanSubjectFingerprint() {
  return g_state.orphanSubjectReady
             ? g_state.orphanSubjectFingerprint
             : 0;
}

bool RecoveredArenaSeance_ArtefactAttributesReady() {
  return g_state.artefactAttributesReady;
}

bool RecoveredArenaSeance_SmokeAttributesReady() {
  return g_state.smokeAttributesReady;
}

bool RecoveredArenaSeance_SmokeSubjectReady() {
  return g_state.smokeSubjectReady;
}

int RecoveredArenaSeance_SmokeSubjectCapacity() {
  return g_state.smokeSubjectReady ? g_state.smokeSubjectCapacity : 0;
}

unsigned long long RecoveredArenaSeance_SmokeSubjectFingerprint() {
  return g_state.smokeSubjectReady ? g_state.smokeSubjectFingerprint : 0;
}

bool RecoveredArenaSeance_SmokeVisualResourcesReady() {
  return g_state.smokeVisualResourcesReady;
}

unsigned long long RecoveredArenaSeance_SmokeVisualResourceFingerprint() {
  return g_state.smokeVisualResourcesReady
             ? g_state.smokeVisualResourceFingerprint
             : 0;
}

bool RecoveredArenaSeance_ExplosionAttributesReady() {
  return g_state.explosionAttributesReady;
}

bool RecoveredArenaSeance_ExplosionSubjectReady() {
  return g_state.explosionSubjectReady;
}

bool RecoveredArenaSeance_ExplosionImpulseReady() {
  return g_state.explosionImpulseReady;
}

bool RecoveredArenaSeance_ExplosionLightReady() {
  return g_state.explosionLightReady;
}

bool RecoveredArenaSeance_ExplosionSoundReady() {
  return g_state.explosionSoundReady;
}

unsigned long long RecoveredArenaSeance_ExplosionSoundReferenceFingerprint() {
  return g_state.explosionSoundReady
             ? g_state.explosionSoundReferenceFingerprint
             : 0;
}

int RecoveredArenaSeance_ExplosionSoundProbeStarted() {
  return g_state.explosionSoundReady ? g_state.explosionSoundProbeStarted : -1;
}

int RecoveredArenaSeance_ExplosionSoundProbeDependencySkips() {
  return g_state.explosionSoundReady
             ? g_state.explosionSoundProbeDependencySkips
             : -1;
}

int RecoveredArenaSeance_ExplosionSoundProbeRollbacks() {
  return g_state.explosionSoundReady
             ? g_state.explosionSoundProbeRollbacks
             : -1;
}

bool RecoveredArenaSeance_ExplosionParticlesReady() {
  return g_state.explosionParticlesReady;
}

unsigned long long
RecoveredArenaSeance_ExplosionParticleVisualFingerprint() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleVisualFingerprint
             : 0;
}

int RecoveredArenaSeance_ExplosionParticleProbeStartedBranches() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeStartedBranches
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeSimpleParticles() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeSimpleParticles
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeSnakeParticles() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeSnakeParticles
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeRays() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeRays
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeDependencySkips() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeDependencySkips
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeMoveSteps() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeMoveSteps
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeExpiredParents() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeExpiredParents
             : -1;
}

int RecoveredArenaSeance_ExplosionParticleProbeRolledBackBranches() {
  return g_state.explosionParticlesReady
             ? g_state.explosionParticleProbeRolledBackBranches
             : -1;
}

bool RecoveredArenaSeance_ExplosionSmokeReady() {
  return g_state.explosionSmokeReady;
}

unsigned long long
RecoveredArenaSeance_ExplosionSmokeVisualFingerprint() {
  return g_state.explosionSmokeReady
             ? g_state.explosionSmokeVisualFingerprint
             : 0;
}

int RecoveredArenaSeance_ExplosionSmokeProbeStartedSprites() {
  return g_state.explosionSmokeReady
             ? g_state.explosionSmokeProbeStartedSprites
             : -1;
}

int RecoveredArenaSeance_ExplosionSmokeProbeDependencySkips() {
  return g_state.explosionSmokeReady
             ? g_state.explosionSmokeProbeDependencySkips
             : -1;
}

int RecoveredArenaSeance_ExplosionSmokeProbeMoveSteps() {
  return g_state.explosionSmokeReady
             ? g_state.explosionSmokeProbeMoveSteps
             : -1;
}

int RecoveredArenaSeance_ExplosionSmokeProbeExpiredParents() {
  return g_state.explosionSmokeReady
             ? g_state.explosionSmokeProbeExpiredParents
             : -1;
}

int RecoveredArenaSeance_ExplosionSmokeProbeRolledBackSprites() {
  return g_state.explosionSmokeReady
             ? g_state.explosionSmokeProbeRolledBackSprites
             : -1;
}

bool RecoveredArenaSeance_ExplosionPieceReady() {
  return g_state.explosionPieceReady;
}

unsigned long long
RecoveredArenaSeance_ExplosionPieceReferenceFingerprint() {
  return g_state.explosionPieceReady
             ? g_state.explosionPieceReferenceFingerprint
             : 0;
}

int RecoveredArenaSeance_ExplosionPieceProbeStartedPieces() {
  return g_state.explosionPieceReady
             ? g_state.explosionPieceProbeStartedPieces
             : -1;
}

int RecoveredArenaSeance_ExplosionPieceProbeDependencySkips() {
  return g_state.explosionPieceReady
             ? g_state.explosionPieceProbeDependencySkips
             : -1;
}

int RecoveredArenaSeance_ExplosionPieceProbeMoveSteps() {
  return g_state.explosionPieceReady
             ? g_state.explosionPieceProbeMoveSteps
             : -1;
}

int RecoveredArenaSeance_ExplosionPieceProbeExpiredParents() {
  return g_state.explosionPieceReady
             ? g_state.explosionPieceProbeExpiredParents
             : -1;
}

int RecoveredArenaSeance_ExplosionPieceProbeRolledBackPieces() {
  return g_state.explosionPieceReady
             ? g_state.explosionPieceProbeRolledBackPieces
             : -1;
}

bool RecoveredArenaSeance_ExplosionTraceReady() {
  return g_state.explosionTraceReady;
}

unsigned long long
RecoveredArenaSeance_ExplosionTraceReferenceFingerprint() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceReferenceFingerprint
             : 0;
}

int RecoveredArenaSeance_ExplosionTraceProbeStartedPieces() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbeStartedPieces
             : -1;
}

int RecoveredArenaSeance_ExplosionTraceProbeQuotaGateSkips() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbeQuotaGateSkips
             : -1;
}

int RecoveredArenaSeance_ExplosionTraceProbePuffEvents() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbePuffEvents
             : -1;
}

int RecoveredArenaSeance_ExplosionTraceProbeSmokeChildren() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbeSmokeChildren
             : -1;
}

int RecoveredArenaSeance_ExplosionTraceProbeMoveSteps() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbeMoveSteps
             : -1;
}

int RecoveredArenaSeance_ExplosionTraceProbeExpiredParents() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbeExpiredParents
             : -1;
}

int RecoveredArenaSeance_ExplosionTraceProbeRolledBackPieces() {
  return g_state.explosionTraceReady
             ? g_state.explosionTraceProbeRolledBackPieces
             : -1;
}

int RecoveredArenaSeance_ExplosionSubjectCapacity() {
  return g_state.explosionSubjectReady ? g_state.explosionSubjectCapacity : 0;
}

unsigned long long RecoveredArenaSeance_ExplosionSubjectFingerprint() {
  return g_state.explosionSubjectReady
             ? g_state.explosionSubjectFingerprint
             : 0;
}

int RecoveredArenaSeance_ExplosionProbeInvalidStarts() {
  return g_state.explosionSubjectReady ? g_state.explosionProbeInvalidStarts
                                       : -1;
}

int RecoveredArenaSeance_ExplosionProbeAllocationRollbacks() {
  return g_state.explosionSubjectReady
             ? g_state.explosionProbeAllocationRollbacks
             : -1;
}

int RecoveredArenaSeance_ExplosionProbeQueuedCommands() {
  return g_state.explosionSubjectReady ? g_state.explosionProbeQueuedCommands
                                       : -1;
}

int RecoveredArenaSeance_ExplosionProbeQueueRollbacks() {
  return g_state.explosionSubjectReady ? g_state.explosionProbeQueueRollbacks
                                       : -1;
}

int RecoveredArenaSeance_ExplosionProbeExecutedCommands() {
  return g_state.explosionSubjectReady
             ? g_state.explosionProbeExecutedCommands
             : -1;
}

int RecoveredArenaSeance_ExplosionProbeDamageApplications() {
  return g_state.explosionSubjectReady
             ? g_state.explosionProbeDamageApplications
             : -1;
}

bool RecoveredArenaSeance_ExplosionActiveWorldReady() {
  return g_state.explosionActiveWorldReady;
}

int RecoveredArenaSeance_ExplosionActiveWorldCapturedOwners() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldCapturedOwners : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldCapturedBranches() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldCapturedBranches : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldSchedulerEvents() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldSchedulerEvents : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldSoundChildren() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldSoundChildren : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldRollbacks() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldRollbacks : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldReconstructedIDs() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldReconstructedIDs : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldStableRoundTrips() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldStableRoundTrips : -1;
}

int RecoveredArenaSeance_ExplosionActiveWorldResumedMoves() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldResumedMoves : -1;
}

unsigned long long RecoveredArenaSeance_ExplosionActiveWorldFingerprint() {
  return g_state.explosionActiveWorldReady
             ? g_state.explosionActiveWorldFingerprint : 0;
}

bool RecoveredArenaSeance_VehicleAttributesReady() {
  return g_state.vehicleAttributesReady;
}

bool RecoveredArenaSeance_VehicleReferencesReady() {
  return g_state.vehicleReferencesReady;
}

int RecoveredArenaSeance_VehicleAttributeCount() {
  return g_state.vehicleAttributesReady ? g_state.vehicleAttributeCount : -1;
}

int RecoveredArenaSeance_VehicleAttributeCapacity() {
  return g_state.vehicleAttributesReady ? g_state.vehicleAttributeCapacity : 0;
}

unsigned long long RecoveredArenaSeance_VehicleAttributeFingerprint() {
  return g_state.vehicleAttributesReady
             ? g_state.vehicleAttributeFingerprint
             : 0;
}

unsigned long long RecoveredArenaSeance_VehicleReferenceFingerprint() {
  return g_state.vehicleReferencesReady
             ? g_state.vehicleReferenceFingerprint
             : 0;
}

bool RecoveredArenaSeance_TaxiAttributesReady() {
  return g_state.taxiAttributesReady;
}

bool RecoveredArenaSeance_TaxiReferencesReady() {
  return g_state.taxiReferencesReady;
}

unsigned long long RecoveredArenaSeance_TaxiReferenceFingerprint() {
  return g_state.taxiReferencesReady ? g_state.taxiReferenceFingerprint : 0;
}

bool RecoveredArenaSeance_TaxiSubjectReady() {
  return g_state.taxiSubjectReady;
}

int RecoveredArenaSeance_TaxiSubjectCapacity() {
  return g_state.taxiSubjectReady ? g_state.taxiSubjectCapacity : 0;
}

int RecoveredArenaSeance_TaxiSubjectCount() {
  return g_state.taxiSubjectReady ? g_state.taxiSubjectCount : 0;
}

int RecoveredArenaSeance_TaxiSubjectSoundCount() {
  return g_state.taxiSubjectReady ? g_state.taxiSubjectSoundCount : 0;
}

unsigned long long RecoveredArenaSeance_TaxiSubjectFingerprint() {
  return g_state.taxiSubjectReady ? g_state.taxiSubjectFingerprint : 0;
}

int RecoveredArenaSeance_TaxiProbeInvalidStarts() {
  return g_state.taxiSubjectReady ? g_state.taxiProbeInvalidStarts : 0;
}

int RecoveredArenaSeance_TaxiProbeValidStarts() {
  return g_state.taxiSubjectReady ? g_state.taxiProbeValidStarts : 0;
}

int RecoveredArenaSeance_TaxiProbeRenderReady() {
  return g_state.taxiSubjectReady ? g_state.taxiProbeRenderReady : 0;
}

int RecoveredArenaSeance_TaxiProbeSoundReady() {
  return g_state.taxiSubjectReady ? g_state.taxiProbeSoundReady : 0;
}

int RecoveredArenaSeance_TaxiProbeRollbacks() {
  return g_state.taxiSubjectReady ? g_state.taxiProbeRollbacks : 0;
}

bool RecoveredArenaSeance_BulletAttributesReady() {
  return g_state.bulletAttributesReady;
}

bool RecoveredArenaSeance_HowitzerTablesReady() {
  return g_state.arenaOpen &&
         HowitzerSubjectState_TableReady(g_arena.getContext());
}

int RecoveredArenaSeance_HowitzerAttributeCount() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_AttributeCount()
             : 0;
}

int RecoveredArenaSeance_HowitzerAttributeCapacity() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_AttributeCapacity()
             : 0;
}

int RecoveredArenaSeance_HowitzerSubjectCapacity() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_SubjectCapacity()
             : 0;
}

int RecoveredArenaSeance_HowitzerLiveCount() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_LiveCount()
             : 0;
}

int RecoveredArenaSeance_HowitzerReadyLiveCount() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_ReadyLiveCount(g_arena.getContext())
             : 0;
}

int RecoveredArenaSeance_HowitzerHolderCount() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_HolderCount()
             : 0;
}

int RecoveredArenaSeance_HowitzerSupportedHolderCount() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_SupportedHolderCount()
             : 0;
}

int RecoveredArenaSeance_HowitzerOccupiedHolderCount() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_OccupiedHolderCount()
             : 0;
}

unsigned long long RecoveredArenaSeance_HowitzerFingerprint() {
  return RecoveredArenaSeance_HowitzerTablesReady()
             ? HowitzerSubjectState_Fingerprint(g_arena.getContext())
             : 0;
}

bool RecoveredArenaSeance_BulletReferencesReady() {
  return g_state.bulletReferencesReady;
}

int RecoveredArenaSeance_BulletAttributeCount() {
  return g_state.bulletAttributesReady ? g_state.bulletAttributeCount : -1;
}

int RecoveredArenaSeance_BulletAttributeCapacity() {
  return g_state.bulletAttributesReady ? g_state.bulletAttributeCapacity : 0;
}

unsigned long long RecoveredArenaSeance_BulletAttributeFingerprint() {
  return g_state.bulletAttributesReady
             ? g_state.bulletAttributeFingerprint
             : 0;
}

unsigned long long RecoveredArenaSeance_BulletReferenceFingerprint() {
  return g_state.bulletReferencesReady
             ? g_state.bulletReferenceFingerprint
             : 0;
}

bool RecoveredArenaSeance_BulletSubjectRegistrationReady() {
  return g_state.bulletSubjectRegistrationReady;
}

bool RecoveredArenaSeance_BulletSubjectReady() {
  return g_state.bulletSubjectReady;
}

bool RecoveredArenaSeance_BulletImpactEffectsReady() {
  return g_state.bulletImpactEffectsReady;
}

bool RecoveredArenaSeance_BulletGroundSparkReady() {
  return g_state.bulletGroundSparkReady;
}

bool RecoveredArenaSeance_BulletBarrelSmokeReady() {
  return g_state.bulletBarrelSmokeReady;
}

int RecoveredArenaSeance_BulletSubjectCapacity() {
  return g_state.bulletSubjectRegistrationReady
             ? g_state.bulletSubjectCapacity
             : 0;
}

unsigned long long RecoveredArenaSeance_BulletSubjectFingerprint() {
  return g_state.bulletSubjectReady ? g_state.bulletSubjectFingerprint : 0;
}

int RecoveredArenaSeance_BulletSubjectProbeMoveCount() {
  return g_state.bulletSubjectReady ? g_state.bulletSubjectProbeMoveCount : -1;
}

int RecoveredArenaSeance_BulletCollisionScheduledChecks() {
  return g_state.bulletSubjectReady
             ? g_state.bulletCollisionScheduledChecks
             : -1;
}

int RecoveredArenaSeance_BulletCollisionExecutedChecks() {
  return g_state.bulletSubjectReady
             ? g_state.bulletCollisionExecutedChecks
             : -1;
}

int RecoveredArenaSeance_BulletCollisionSphereCases() {
  return g_state.bulletSubjectReady ? g_state.bulletCollisionSphereCases : -1;
}

int RecoveredArenaSeance_BulletCollisionEarliestHitCases() {
  return g_state.bulletSubjectReady
             ? g_state.bulletCollisionEarliestHitCases
             : -1;
}

int RecoveredArenaSeance_BulletCollisionWaterlineCases() {
  return g_state.bulletSubjectReady
             ? g_state.bulletCollisionWaterlineCases
             : -1;
}

int RecoveredArenaSeance_BulletCollisionSceneQueries() {
  return g_state.bulletSubjectReady ? g_state.bulletCollisionSceneQueries : -1;
}

int RecoveredArenaSeance_BulletEffectQueuedBatches() {
  return g_state.bulletImpactEffectsReady ? g_state.bulletEffectQueuedBatches
                                          : -1;
}

int RecoveredArenaSeance_BulletEffectQueuedChildren() {
  return g_state.bulletImpactEffectsReady ? g_state.bulletEffectQueuedChildren
                                          : -1;
}

int RecoveredArenaSeance_BulletEffectSplashFirstCases() {
  return g_state.bulletImpactEffectsReady
             ? g_state.bulletEffectSplashFirstCases
             : -1;
}

int RecoveredArenaSeance_BulletEffectRolledBackChildren() {
  return g_state.bulletImpactEffectsReady
             ? g_state.bulletEffectRolledBackChildren
             : -1;
}

int RecoveredArenaSeance_BulletGroundSparkQueued() {
  return g_state.bulletGroundSparkReady
             ? g_state.bulletGroundSparkQueued
             : -1;
}

int RecoveredArenaSeance_BulletGroundSparkRolledBack() {
  return g_state.bulletGroundSparkReady
             ? g_state.bulletGroundSparkRolledBack
             : -1;
}

int RecoveredArenaSeance_BulletBarrelSmokeThresholdStarts() {
  return g_state.bulletBarrelSmokeReady
             ? g_state.bulletBarrelSmokeThresholdStarts
             : -1;
}

int RecoveredArenaSeance_BulletBarrelSmokeFrameGateSkips() {
  return g_state.bulletBarrelSmokeReady
             ? g_state.bulletBarrelSmokeFrameGateSkips
             : -1;
}

int RecoveredArenaSeance_BulletBarrelSmokeAttributeGateSkips() {
  return g_state.bulletBarrelSmokeReady
             ? g_state.bulletBarrelSmokeAttributeGateSkips
             : -1;
}

int RecoveredArenaSeance_BulletBarrelSmokeRollbacks() {
  return g_state.bulletBarrelSmokeReady
             ? g_state.bulletBarrelSmokeRollbacks
             : -1;
}

bool RecoveredArenaSeance_BulletActiveWorldReady() {
  return g_state.bulletActiveWorldReady;
}

int RecoveredArenaSeance_BulletActiveWorldCapturedOwners() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldCapturedOwners : -1;
}

int RecoveredArenaSeance_BulletActiveWorldSchedulerEvents() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldSchedulerEvents : -1;
}

int RecoveredArenaSeance_BulletActiveWorldRollbacks() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldRollbacks : -1;
}

int RecoveredArenaSeance_BulletActiveWorldReconstructedIDs() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldReconstructedIDs : -1;
}

int RecoveredArenaSeance_BulletActiveWorldStableRoundTrips() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldStableRoundTrips : -1;
}

int RecoveredArenaSeance_BulletActiveWorldResumedMoves() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldResumedMoves : -1;
}

int RecoveredArenaSeance_BulletActiveWorldTombstonedMasters() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldTombstonedMasters : -1;
}

unsigned long long RecoveredArenaSeance_BulletActiveWorldFingerprint() {
  return g_state.bulletActiveWorldReady
             ? g_state.bulletActiveWorldFingerprint : 0;
}

bool RecoveredArenaSeance_FarterAttributesReady() {
  return g_state.farterAttributesReady;
}

bool RecoveredArenaSeance_LampAttributesReady() {
  return g_state.lampAttributesReady;
}

bool RecoveredArenaSeance_CorpseAttributesReady() {
  return g_state.corpseAttributesReady;
}

bool RecoveredArenaSeance_SmokerAttributesReady() {
  return g_state.smokerAttributesReady;
}

bool RecoveredArenaSeance_SmokerReferencesReady() {
  return g_state.smokerReferencesReady;
}

bool RecoveredArenaSeance_SmokerRuntimeReady() {
  return g_state.smokerRuntimeReady;
}

unsigned long long RecoveredArenaSeance_SmokerReferenceFingerprint() {
  return g_state.smokerReferencesReady ? g_state.smokerReferenceFingerprint
                                       : 0;
}

bool RecoveredArenaSeance_DynSmokerReady() {
  return g_state.dynSmokerReady;
}

int RecoveredArenaSeance_DynSmokerCapacity() {
  return g_state.dynSmokerReady ? g_state.dynSmokerCapacity : 0;
}

unsigned long long RecoveredArenaSeance_DynSmokerFingerprint() {
  return g_state.dynSmokerReady ? g_state.dynSmokerFingerprint : 0;
}

int RecoveredArenaSeance_SmokerAttributeCount() {
  return g_state.smokerAttributesReady
             ? SmokerAttributeState_RosterSize(g_arena.getContext())
             : -1;
}

int RecoveredArenaSeance_SmokerAttributeCapacity() {
  return g_state.smokerAttributesReady ? SmokerAttributeState_Capacity() : 0;
}

unsigned long long RecoveredArenaSeance_SmokerAttributeFingerprint() {
  return g_state.smokerAttributesReady
             ? SmokerAttributeState_Fingerprint(g_arena.getContext())
             : 0;
}

int RecoveredArenaSeance_TaxiAttributeCount() {
  return g_state.taxiAttributesReady
             ? TaxiAttributeState_RosterSize(g_arena.getContext())
             : -1;
}

int RecoveredArenaSeance_TaxiAttributeCapacity() {
  return g_state.taxiAttributesReady ? TaxiAttributeState_Capacity() : 0;
}

unsigned long long RecoveredArenaSeance_TaxiAttributeFingerprint() {
  return g_state.taxiAttributesReady
             ? TaxiAttributeState_Fingerprint(g_arena.getContext())
             : 0;
}

int RecoveredArenaSeance_FarterAttributeCount() {
  return g_state.farterAttributesReady
             ? FarterAttributeState_RosterSize(g_arena.getContext())
             : -1;
}

int RecoveredArenaSeance_FarterAttributeCapacity() {
  return g_state.farterAttributesReady ? FarterAttributeState_Capacity() : 0;
}

unsigned long long RecoveredArenaSeance_FarterAttributeFingerprint() {
  return g_state.farterAttributesReady
             ? FarterAttributeState_Fingerprint(g_arena.getContext())
             : 0;
}

bool RecoveredArenaSeance_FarterReferencesReady() {
  return g_state.farterReferencesReady;
}

bool RecoveredArenaSeance_FarterRuntimeReady() {
  return g_state.farterRuntimeReady;
}

bool RecoveredArenaSeance_FarterSubjectReady() {
  return g_state.farterSubjectReady;
}

int RecoveredArenaSeance_FarterSubjectCapacity() {
  return g_state.farterSubjectReady ? g_state.farterSubjectCapacity : 0;
}

unsigned long long RecoveredArenaSeance_FarterSubjectFingerprint() {
  return g_state.farterSubjectReady ? g_state.farterSubjectFingerprint : 0;
}

int RecoveredArenaSeance_FarterScriptObjectCount() {
  return g_state.farterSubjectReady ? g_state.farterScriptObjectCount : -1;
}

int RecoveredArenaSeance_FarterLiveObjectCount() {
  return g_state.farterSubjectReady ? g_state.farterLiveObjectCount : -1;
}

int RecoveredArenaSeance_FarterSoundObjectCount() {
  return g_state.farterSubjectReady ? g_state.farterSoundObjectCount : -1;
}

int RecoveredArenaSeance_FarterNearFrameAudibleCount() {
  return g_state.farterSubjectReady
             ? g_state.farterNearFrameAudibleCount
             : -1;
}

int RecoveredArenaSeance_FarterFarFrameAudibleCount() {
  return g_state.farterSubjectReady ? g_state.farterFarFrameAudibleCount : -1;
}

bool RecoveredArenaSeance_FarterAudibleFrameTransition() {
  return g_state.farterAudibleFrameTransition;
}

bool RecoveredArenaSeance_SoundDistanceReady() {
  return g_state.soundDistanceReady;
}

double RecoveredArenaSeance_SoundDistance() {
  return g_state.soundDistanceReady ? g_state.soundDistance : 0.0;
}

double RecoveredArenaSeance_SoundDistanceSquared() {
  return g_state.soundDistanceReady ? g_state.soundDistanceSquared : 0.0;
}

unsigned long long RecoveredArenaSeance_FarterReferenceFingerprint() {
  return g_state.farterReferencesReady ? g_state.farterReferenceFingerprint
                                       : 0;
}

int RecoveredArenaSeance_LampAttributeCount() {
  return g_state.lampAttributesReady
             ? LampAttributeState_RosterSize(g_arena.getContext())
             : 0;
}

int RecoveredArenaSeance_LampAttributeCapacity() {
  return g_state.lampAttributesReady ? LampAttributeState_Capacity() : 0;
}

unsigned long long RecoveredArenaSeance_LampAttributeFingerprint() {
  return g_state.lampAttributesReady
             ? LampAttributeState_Fingerprint(g_arena.getContext())
             : 0;
}

int RecoveredArenaSeance_CorpseAttributeCount() {
  return g_state.corpseAttributesReady
             ? CorpseAttributeState_RosterSize(g_arena.getContext())
             : 0;
}

int RecoveredArenaSeance_CorpseAttributeCapacity() {
  return g_state.corpseAttributesReady ? CorpseAttributeState_Capacity() : 0;
}

unsigned long long RecoveredArenaSeance_CorpseAttributeFingerprint() {
  return g_state.corpseAttributesReady
             ? CorpseAttributeState_Fingerprint(g_arena.getContext())
             : 0;
}

bool RecoveredArenaSeance_CorpseReferencesReady() {
  return g_state.corpseReferencesReady;
}

bool RecoveredArenaSeance_CorpseRuntimeReady() {
  return g_state.corpseRuntimeReady;
}

unsigned long long RecoveredArenaSeance_CorpseReferenceFingerprint() {
  return g_state.corpseReferencesReady ? g_state.corpseReferenceFingerprint
                                       : 0;
}

bool RecoveredArenaSeance_CorpseSubjectReady() {
  return g_state.corpseSubjectReady;
}

int RecoveredArenaSeance_CorpseSubjectCapacity() {
  return g_state.corpseSubjectReady ? g_state.corpseSubjectCapacity : 0;
}

unsigned long long RecoveredArenaSeance_CorpseSubjectFingerprint() {
  return g_state.corpseSubjectReady ? g_state.corpseSubjectFingerprint : 0;
}

bool RecoveredArenaSeance_WavMetadataReady() {
  return g_state.wavMetadataReady;
}

int RecoveredArenaSeance_WavMetadataCount() {
  return g_state.wavMetadataReady ? g_state.wavMetadataCount : 0;
}

int RecoveredArenaSeance_WavMetadataCapacity() {
  return g_state.wavMetadataReady ? g_state.wavMetadataCapacity : 0;
}

unsigned long long RecoveredArenaSeance_WavCatalogFingerprint() {
  return g_state.wavMetadataReady ? g_state.wavCatalogFingerprint : 0;
}

unsigned long long RecoveredArenaSeance_WavResourceFingerprint() {
  return g_state.wavMetadataReady ? g_state.wavResourceFingerprint : 0;
}

bool RecoveredArenaSeance_SoundObjectReady() {
  return g_state.soundObjectReady;
}

int RecoveredArenaSeance_SoundObjectCapacity() {
  return g_state.soundObjectReady ? g_state.soundObjectCapacity : 0;
}

unsigned long long RecoveredArenaSeance_SoundObjectFingerprint() {
  return g_state.soundObjectReady ? g_state.soundObjectFingerprint : 0;
}

bool RecoveredArenaSeance_SkinResourcesReady() {
  return g_state.skinResourcesReady;
}

int RecoveredArenaSeance_SkinModelCount() {
  return g_state.skinModelCount;
}

int RecoveredArenaSeance_SkinSpriteCount() {
  return g_state.skinSpriteCount;
}

unsigned long long RecoveredArenaSeance_SkinCatalogFingerprint() {
  return g_state.skinCatalogFingerprint;
}

unsigned long long RecoveredArenaSeance_SkinResourceFingerprint() {
  return g_state.skinResourceFingerprint;
}

bool RecoveredArenaSeance_SkinAnimationsReady() {
  return g_state.skinAnimationsReady;
}

int RecoveredArenaSeance_SkinAnimationEntryCallCount() {
  return g_state.skinAnimationEntryCallCount;
}

int RecoveredArenaSeance_SkinAnimatedModelCount() {
  return g_state.skinAnimatedModelCount;
}

int RecoveredArenaSeance_SkinAnimationCommandCount() {
  return g_state.skinAnimationCommandCount;
}

unsigned long long RecoveredArenaSeance_SkinAnimationSourceFingerprint() {
  return g_state.skinAnimationSourceFingerprint;
}

unsigned long long RecoveredArenaSeance_SkinAnimationStateFingerprint() {
  return g_state.skinAnimationStateFingerprint;
}

int RecoveredArenaSeance_SkinAnimationPoseTemporalModelCount() {
  return g_state.skinAnimationPoseTemporalModelCount;
}

int RecoveredArenaSeance_SkinAnimationPoseChangedModelCount() {
  return g_state.skinAnimationPoseChangedModelCount;
}

int RecoveredArenaSeance_SkinAnimationPoseSampleCount() {
  return g_state.skinAnimationPoseSampleCount;
}

int RecoveredArenaSeance_SkinAnimationPoseRestoredModifierCount() {
  return g_state.skinAnimationPoseRestoredModifierCount;
}

unsigned long long RecoveredArenaSeance_SkinAnimationPoseFingerprint() {
  return g_state.skinAnimationPoseFingerprint;
}

bool RecoveredArenaSeance_StaticMechanismsReady() {
  return g_state.staticMechanismsReady;
}

bool RecoveredArenaSeance_StaticMechanismTargetLevel() {
  return g_state.staticMechanismTargetLevel;
}

bool RecoveredArenaSeance_StaticMechanismLevelOne() {
  return g_state.staticMechanismLevelOne;
}

bool RecoveredArenaSeance_StaticMechanismLevelFive() {
  return g_state.staticMechanismLevelFive;
}

int RecoveredArenaSeance_StaticMechanismBindingCount() {
  return g_state.staticMechanismBindingCount;
}

int RecoveredArenaSeance_StaticMechanismWaterwheelCount() {
  return g_state.staticMechanismWaterwheelCount;
}

int RecoveredArenaSeance_StaticMechanismFlagCount() {
  return g_state.staticMechanismFlagCount;
}

int RecoveredArenaSeance_StaticMechanismRotatingCount() {
  return g_state.staticMechanismRotatingCount;
}

int RecoveredArenaSeance_StaticMechanismDoorCount() {
  return g_state.staticMechanismDoorCount;
}

int RecoveredArenaSeance_StaticMechanismPol16Count() {
  return g_state.staticMechanismPol16Count;
}

int RecoveredArenaSeance_StaticMechanismChangedBindingCount() {
  return g_state.staticMechanismChangedBindingCount;
}

int RecoveredArenaSeance_StaticMechanismPoseSampleCount() {
  return g_state.staticMechanismPoseSampleCount;
}

int RecoveredArenaSeance_StaticMechanismRestoredModifierCount() {
  return g_state.staticMechanismRestoredModifierCount;
}

unsigned long long RecoveredArenaSeance_StaticMechanismFingerprint() {
  return g_state.staticMechanismFingerprint;
}

bool RecoveredArenaSeance_SparkAttributesReady() {
  return g_state.sparkAttributesReady;
}

bool RecoveredArenaSeance_SparkSubjectReady() {
  return g_state.sparkSubjectReady;
}

bool RecoveredArenaSeance_SparkVisualResourcesReady() {
  return g_state.sparkVisualResourcesReady;
}

int RecoveredArenaSeance_SparkSubjectCapacity() {
  return g_state.sparkSubjectReady ? g_state.sparkSubjectCapacity : 0;
}

unsigned long long RecoveredArenaSeance_SparkSubjectFingerprint() {
  return g_state.sparkSubjectReady ? g_state.sparkSubjectFingerprint : 0;
}

unsigned long long RecoveredArenaSeance_SparkVisualResourceFingerprint() {
  return g_state.sparkVisualResourcesReady
             ? g_state.sparkVisualResourceFingerprint
             : 0;
}

int RecoveredArenaSeance_SparkProbeInvalidStarts() {
  return g_state.sparkVisualResourcesReady ? g_state.sparkProbeInvalidStarts
                                           : -1;
}

int RecoveredArenaSeance_SparkProbeQueuedCreates() {
  return g_state.sparkVisualResourcesReady ? g_state.sparkProbeQueuedCreates
                                           : -1;
}

int RecoveredArenaSeance_SparkProbeQueueRollbacks() {
  return g_state.sparkVisualResourcesReady ? g_state.sparkProbeQueueRollbacks
                                           : -1;
}

int RecoveredArenaSeance_SparkProbePhaseTransitions() {
  return g_state.sparkVisualResourcesReady
             ? g_state.sparkProbePhaseTransitions
             : -1;
}

int RecoveredArenaSeance_SparkProbeExpirations() {
  return g_state.sparkVisualResourcesReady ? g_state.sparkProbeExpirations
                                           : -1;
}

bool RecoveredArenaSeance_SparkActiveWorldReady() {
  return g_state.sparkActiveWorldReady;
}

int RecoveredArenaSeance_SparkActiveWorldCapturedOwners() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldCapturedOwners : -1;
}

int RecoveredArenaSeance_SparkActiveWorldSchedulerEvents() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldSchedulerEvents : -1;
}

int RecoveredArenaSeance_SparkActiveWorldRollbacks() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldRollbacks : -1;
}

int RecoveredArenaSeance_SparkActiveWorldReconstructedIDs() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldReconstructedIDs : -1;
}

int RecoveredArenaSeance_SparkActiveWorldStableRoundTrips() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldStableRoundTrips : -1;
}

int RecoveredArenaSeance_SparkActiveWorldResumedPhases() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldResumedPhases : -1;
}

unsigned long long RecoveredArenaSeance_SparkActiveWorldFingerprint() {
  return g_state.sparkActiveWorldReady
             ? g_state.sparkActiveWorldFingerprint : 0;
}

bool RecoveredArenaSeance_SmokeActiveWorldReady() {
  return g_state.smokeActiveWorldReady;
}

int RecoveredArenaSeance_SmokeActiveWorldCapturedOwners() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldCapturedOwners : -1;
}

int RecoveredArenaSeance_SmokeActiveWorldCapturedBlobs() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldCapturedBlobs : -1;
}

int RecoveredArenaSeance_SmokeActiveWorldSchedulerEvents() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldSchedulerEvents : -1;
}

int RecoveredArenaSeance_SmokeActiveWorldRollbacks() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldRollbacks : -1;
}

int RecoveredArenaSeance_SmokeActiveWorldReconstructedIDs() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldReconstructedIDs : -1;
}

int RecoveredArenaSeance_SmokeActiveWorldStableRoundTrips() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldStableRoundTrips : -1;
}

int RecoveredArenaSeance_SmokeActiveWorldResumedMoves() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldResumedMoves : -1;
}

unsigned long long RecoveredArenaSeance_SmokeActiveWorldFingerprint() {
  return g_state.smokeActiveWorldReady
             ? g_state.smokeActiveWorldFingerprint : 0;
}

bool RecoveredArenaSeance_CorpseActiveWorldReady() {
  return g_state.corpseActiveWorldReady;
}

int RecoveredArenaSeance_CorpseActiveWorldCapturedOwners() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldCapturedOwners : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldOwnedSmokers() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldOwnedSmokers : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldSchedulerEvents() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldSchedulerEvents : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldRollbacks() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldRollbacks : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldReconstructedObjects() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldReconstructedObjects : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldStableRoundTrips() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldStableRoundTrips : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldResumedEmissions() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldResumedEmissions : -1;
}

int RecoveredArenaSeance_CorpseActiveWorldResumedDeaths() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldResumedDeaths : -1;
}

unsigned long long RecoveredArenaSeance_CorpseActiveWorldFingerprint() {
  return g_state.corpseActiveWorldReady
             ? g_state.corpseActiveWorldFingerprint : 0;
}

bool RecoveredArenaSeance_VehicleReady() { return g_state.vehicleReady; }

int RecoveredArenaSeance_VehicleActiveWorldReconstructedIDs() {
  return g_state.vehicleReady ? g_state.vehicleActiveWorldReconstructedIDs
                              : -1;
}

int RecoveredArenaSeance_VehicleActiveWorldRollbacks() {
  return g_state.vehicleReady ? g_state.vehicleActiveWorldRollbacks : -1;
}

unsigned long long RecoveredArenaSeance_VehicleActiveWorldFingerprint() {
  return g_state.vehicleReady ? g_state.vehicleActiveWorldFingerprint : 0;
}

double RecoveredArenaSeance_VehicleVesselMass() {
  return g_state.vehicleReady && g_vehicle != nullptr
             ? g_vehicle->VesselMass()
             : 0.0;
}

unsigned long long RecoveredArenaSeance_Issues() { return g_state.issues; }

unsigned long long RecoveredArenaSeance_ExtendedIssues() {
  return g_state.extendedIssues;
}

const char* RecoveredArenaSeance_LastError() { return g_state.lastError; }
