#include "RecoveredArenaSeanceRuntime.h"

#include <cctype>
#include <cstdio>
#include <new>
#include <string>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/artefact/ArtefactAttributeState.h"
#include "obase/bird/BirdAttributeState.h"
#include "obase/corpse/CorpseAttributeState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/farter/FarterSubjectState.h"
#include "obase/lamp/LampAttributeState.h"
#include "obase/orphan/OrphanAttributeState.h"
#include "obase/portal/PortalClassTableState.h"
#include "obase/spark/SparkAttributeState.h"
#include "obase/taxi/TaxiAttributeState.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "obase/smoke/SmokeSubjectState.h"
#include "obase/smoke/SmokeVisualState.h"
#include "obase/smoke/SmokerAttributeState.h"
#include "obase/smoke/SmokerSubjectState.h"
#include "obase/sound/SoundObjectState.h"
#include "obase/sound/WAVResourceState.h"
#include "obase/skin/SkinResourceState.h"
#include "storage/h/subject.h"
#include "message/skinmsg.h"
#include "sound.h"

#include "RecoveredLegacyScriptHost.h"
#include "RecoveredLegacyScriptRunner.h"
#include "RecoveredSkinResourceCatalog.h"
#include "RecoveredWavMetadataCatalog.h"

namespace {

constexpr double kSceneWidth = 5120.0;
constexpr double kSceneDepth = 5120.0;
constexpr int kDynSmokerCapacity = 50 + 12;
constexpr int kSmokeCapacity = 300;
constexpr int kSoundObjectCapacity = 250;
constexpr int kFarterSubjectCapacity = 25;
constexpr double kDeviceFreeSoundDistance = 300.0;
constexpr int kSourceOnlySmokerAttributeCount = 11;
constexpr const char kBootstrapProgramName[] =
    "recovered_common_attribute_vehicle_bootstrap";
constexpr const char kSmokeAttributeProgramName[] =
    "recovered_retail_smoke_attribute_bootstrap";
constexpr const char kExplosionAttributeProgramName[] =
    "recovered_retail_explosion_attribute_bootstrap";
constexpr const char kTaxiAttributeProgramName[] =
    "recovered_retail_taxi_attribute_bootstrap";
constexpr const char kFarterAttributeProgramName[] =
    "recovered_retail_farter_attribute_bootstrap";
constexpr const char kFarterSubjectProgramName[] =
    "recovered_retail_farter_subject_bootstrap";
constexpr const char kLampAttributeProgramName[] =
    "recovered_retail_lamp_attribute_bootstrap";
constexpr const char kCorpseAttributeProgramName[] =
    "recovered_retail_corpse_attribute_bootstrap";
constexpr const char kSmokerAttributeProgramName[] =
    "recovered_retail_smoker_attribute_bootstrap";
constexpr const char kWavMetadataProgramName[] =
    "recovered_retail_wav_metadata_bootstrap";

// This deliberately uses the original script-facing storage and event
// protocol. It is a bounded bridge to the real Vehicle tables, not a second
// gameplay implementation. The complete retail LEVEL0.SC will replace it as
// the remaining OBASE class-table archives are connected.
const char kAttributeVehicleBootstrapScript[] = R"RR2NW_SCRIPT(const int EDO_WRITE = 1;
const int KR_SET_ATTR = 1;
const int sp_EV_SET_PHASE_COUNT extern;
const int sp_EV_SET_PHASE extern;
const int RECT2D_I extern;
const int LIGHT_COLOR_YELLOW extern;
const int s_ATTR_MSG_SET_INT extern;
const int s_ATTR_MSG_SET_DOUBLE extern;
const int s_ATTR_MSG_SET_STR extern;

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
var int sparkAttrTable, routeTable, attrTable, birdAttrTable, portalTable;
var int orphanAttrTable, artefactAttrTable, vehicleTable, objectID, cachePos;
{
  sparkAttrTable := s_AddClassTable("SparkAttr", 3);
  routeTable := s_AddClassTable("Route", 100);

  attrTable := s_AddClassTable("VehicleAttr", 2);
  s_New(attrTable, "Vehicle.Attr.default", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_dynamic", "TankGenn0");
  s_New(attrTable, "Vehicle.Attr.dead", objectID, cachePos);
  SetAttributeStr(objectID, cachePos, "m_dynamic", "Dead");

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

  vehicleTable := s_AddClassTable("Vehicle", 1);
  s_New(vehicleTable, "Vehicle.Default", objectID, cachePos);
  ChangeObjectAttrN("Vehicle.Attr.default", "Vehicle.Default");
}
)RR2NW_SCRIPT";

const char kRetailAttributeBootstrapPrefix[] = R"RR2NW_SCRIPT(
const int EDO_WRITE = 1;
const int KR_SET_ATTR = 1;
const int NO_LAND = 0;
const int ON_LAND = 1;
const int ON_WATER = 2;
const float INFINITY_TIME = -1;
const int LIGHT_COLOR_RED = 1;
const int LIGHT_COLOR_GREEN = 2;
const int LIGHT_COLOR_YELLOW = 3;
const int LIGHT_COLOR_BLUE = 4;
const int LIGHT_COLOR_CYAN = 6;
const int LIGHT_COLOR_WHITE = 7;
const int s_ATTR_MSG_SET_INT extern;
const int s_ATTR_MSG_SET_DOUBLE extern;
const int s_ATTR_MSG_SET_STR extern;
const int fou_EVCMD_START extern;
const int START_FARTING extern;
const int lmp_EV_START extern;
const int lmp_EV_SETENDPOS extern;

func int s_OpenEventData(int style) extern;
func void s_CloseEventData(int event) extern;
func void s_WriteInt(int event, int value) extern;
func void s_WriteFloat(int event, float value) extern;
func void s_WriteStr(int event, str value) extern;
func void s_WriteObjectID(int event, int objectID, int cachePos) extern;
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func int s_AddClassTable(str className, int maxTableSize) extern;
func void s_New(int classTableID, str name,
                var int objectID, var int cachePos) extern;
func void s_NewObject(int classTableID, str name) extern;
func void s_NewObjectN(str className, str name) extern;

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

  FILE* file = std::fopen(relativePath, "rb");
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

struct RecoveredArenaSeanceState {
  unsigned long long issues;
  unsigned long long extendedIssues;
  bool arenaOpen;
  bool scriptCompleted;
  bool birdAttributesReady;
  bool portalReady;
  bool orphanAttributesReady;
  bool artefactAttributesReady;
  bool smokeAttributesReady;
  bool smokeSubjectReady;
  bool smokeVisualResourcesReady;
  bool explosionAttributesReady;
  bool taxiAttributesReady;
  bool farterAttributesReady;
  bool farterReferencesReady;
  bool farterRuntimeReady;
  bool farterSubjectReady;
  bool lampAttributesReady;
  bool corpseAttributesReady;
  bool corpseReferencesReady;
  bool corpseRuntimeReady;
  bool smokerAttributesReady;
  bool smokerReferencesReady;
  bool smokerRuntimeReady;
  bool dynSmokerReady;
  bool wavMetadataReady;
  bool soundObjectReady;
  bool soundDistanceReady;
  bool skinResourcesReady;
  bool sparkAttributesReady;
  bool routeReady;
  bool vehicleReady;
  int skinModelCount;
  int skinSpriteCount;
  unsigned long long skinCatalogFingerprint;
  unsigned long long skinResourceFingerprint;
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

bool RunAttributeVehicleBootstrap(SimulationContext* context,
                                  double startTime) {
  RecoveredLegacyScriptHost host(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  if (!RecoveredLegacyScript_RunMemory(
          kAttributeVehicleBootstrapScript, kBootstrapProgramName, profile,
          context, startTime, &host, &result)) {
    Report(IssueForScriptStatus(result.status), result.error);
    return false;
  }
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

  g_state.vehicleReady = true;
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

  g_state.sparkAttributesReady = true;
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

bool PublishDependentAttributeReferences(SimulationContext* context) {
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

  BirdAttributeState_Link();
  ArtefactAttributeState_Link();
  OrphanAttributeState_Link();
  PortalClassTable_Link();
  SparkAttributeState_Link();
  SmokeAttributeState_Link();
  SmokeSubjectState_Link();
  SmokeVisualState_Link();
  ExplosionAttributeState_Link();
  TaxiAttributeState_Link();
  FarterAttributeState_Link();
  FarterSubjectState_Link();
  LampAttributeState_Link();
  CorpseAttributeState_Link();
  SmokerAttributeState_Link();
  SmokerSubjectState_Link();
  WAVResourceState_Link();
  SoundObjectState_Link();
  SkinResourceState_Link();
  if (!InitializeDeviceFreeSoundDistance()) return FALSE;
  if (!OpenArena(context)) return FALSE;

  try {
    SRecoveredWavMetadataCatalog wavCatalog = {};
    // local_createTables() owns WAVObj before LEVEL0.SC creates attributes.
    if (!RunWavMetadataBootstrap(context, startTime, &wavCatalog)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunAttributeVehicleBootstrap(context, startTime)) {
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
    if (!RunTaxiAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!RunSmokerAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    // Preserve LEVEL0.SC ownership order for this attribute-only tranche.
    if (!RunFarterAttributeBootstrap(context, startTime) ||
        !RunLampAttributeBootstrap(context, startTime) ||
        !RunCorpseAttributeBootstrap(context, startTime)) {
      RecoveredArenaSeance_Release();
      return FALSE;
    }
    if (!PublishSkinResources(context)) {
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

    if (!PublishBirdAttributes(context) || !PublishPortalTable() ||
        !PublishOrphanAttributes(context) ||
        !PublishArtefactAttributes(context) ||
        !PublishSmokeAttributes(context) ||
        !PublishExplosionAttributes(context) ||
        !PublishTaxiAttributes(context) ||
        !PublishSmokerAttributes(context) ||
        !PublishFarterAttributes(context) ||
        !PublishLampAttributes(context) ||
        !PublishCorpseAttributes(context)) {
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

    if (!PublishSmokeVisualResources(context)) {
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
  SmokeVisualState_Release();
  g_state.vehicleReady = false;
  g_state.routeReady = false;
  g_state.sparkAttributesReady = false;
  g_state.smokeAttributesReady = false;
  g_state.smokeSubjectReady = false;
  g_state.smokeSubjectCapacity = 0;
  g_state.smokeSubjectFingerprint = 0;
  g_state.smokeVisualResourcesReady = false;
  g_state.smokeVisualResourceFingerprint = 0;
  g_state.explosionAttributesReady = false;
  g_state.taxiAttributesReady = false;
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
  g_state.skinModelCount = 0;
  g_state.skinSpriteCount = 0;
  g_state.skinCatalogFingerprint = 0;
  g_state.skinResourceFingerprint = 0;
  g_state.artefactAttributesReady = false;
  g_state.orphanAttributesReady = false;
  g_state.portalReady = false;
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

bool RecoveredArenaSeance_BirdAttributesReady() {
  return g_state.birdAttributesReady;
}

bool RecoveredArenaSeance_PortalReady() { return g_state.portalReady; }

bool RecoveredArenaSeance_OrphanAttributesReady() {
  return g_state.orphanAttributesReady;
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

bool RecoveredArenaSeance_TaxiAttributesReady() {
  return g_state.taxiAttributesReady;
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

bool RecoveredArenaSeance_SparkAttributesReady() {
  return g_state.sparkAttributesReady;
}

bool RecoveredArenaSeance_VehicleReady() { return g_state.vehicleReady; }

unsigned long long RecoveredArenaSeance_Issues() { return g_state.issues; }

unsigned long long RecoveredArenaSeance_ExtendedIssues() {
  return g_state.extendedIssues;
}

const char* RecoveredArenaSeance_LastError() { return g_state.lastError; }
