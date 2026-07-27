#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "obase/artefact/ArtefactAttributeState.h"
#include "obase/bird/BirdAttributeState.h"
#include "obase/orphan/OrphanAttributeState.h"
#include "obase/route/route.h"
#include "obase/spark/SparkAttributeState.h"
#include "storage/h/subject.h"

#include "RecoveredLegacyScriptHost.h"
#include "RecoveredLegacyScriptRunner.h"

namespace {

int Fail(const char* message,
         const SRecoveredLegacyScriptRunResult* result = nullptr) {
  std::fprintf(stderr, "recovered-legacy-script-runtime-smoke: %s", message);
  if (result != nullptr) {
    std::fprintf(stderr, " (status=%d host=%u error=%s)",
                 static_cast<int>(result->status), result->hostIssues,
                 result->error);
  }
  std::fputc('\n', stderr);
  return EXIT_FAILURE;
}

std::string JoinPath(const std::string& directory, const char* name) {
  return directory + "\\" + name;
}

bool EnsureDirectory(const std::string& path) {
  if (CreateDirectoryA(path.c_str(), nullptr) != FALSE) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool WriteFile(const std::string& path, const char* contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(contents, static_cast<std::streamsize>(std::strlen(contents)));
  return output.good();
}

bool CurrentDirectory(std::string& result) {
  const DWORD required = GetCurrentDirectoryA(0, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied = GetCurrentDirectoryA(required, buffer.data());
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool FullPath(const char* path, std::string& result) {
  const DWORD required = GetFullPathNameA(path, 0, nullptr, nullptr);
  if (required == 0) return false;
  std::vector<char> buffer(required);
  const DWORD copied =
      GetFullPathNameA(path, required, buffer.data(), nullptr);
  if (copied == 0 || copied >= required) return false;
  result.assign(buffer.data(), copied);
  return true;
}

bool NearlyEqual(double lhs, double rhs) {
  const double difference = lhs > rhs ? lhs - rhs : rhs - lhs;
  return difference < 1.0e-9;
}

bool RunCase(const char* source, const char* name,
             ERecoveredLegacyScriptRunStatus expectedStatus,
             unsigned int expectedHostIssues,
             SRecoveredLegacyScriptRunResult* observed,
             bool expectRoute = false,
             bool expectUnloadedRoutes = false,
             bool expectTruncatedRoute = false,
             bool expectSpark = false,
             bool expectCommonAttributes = false) {
  SimulationContext context(16, 32);
  g_arena.openSeance(&context, 64.0, 64.0);
  RecoveredLegacyScriptHost host(&g_arena);
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  const bool succeeded = RecoveredLegacyScript_RunMemory(
      source, name, profile, &context, 0.0, &host, observed);
  const bool matched =
      succeeded == (expectedStatus == RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS) &&
      observed->status == expectedStatus &&
      (observed->hostIssues & expectedHostIssues) == expectedHostIssues;
  bool routePublished = !expectRoute && !expectTruncatedRoute;
  if (expectRoute) {
    KR_ObjectID routeID = context.searchObject("Route.fixture");
    IRouteObject* route = routeID.isNUL()
                              ? nullptr
                              : static_cast<IRouteObject*>(
                                    context.queryInterface(routeID,
                                                           IRouteObjectIID));
    if (route != nullptr) {
      const CFVector3 midpoint = route->GetPos(0.75);
      routePublished = route->GetNodeCnt() == 3 && route->GetLen() == 20 &&
                       NearlyEqual(midpoint.x, 10.0) &&
                       NearlyEqual(midpoint.z, 5.0);
    }
  }
  if (expectUnloadedRoutes) {
    KR_ObjectID byTable = context.searchObject("Route.by-table");
    KR_ObjectID byClass = context.searchObject("Route.by-class");
    routePublished = !byTable.isNUL() && !byClass.isNUL() &&
                     context.queryInterface(byTable, IRouteObjectIID) !=
                         nullptr &&
                     context.queryInterface(byClass, IRouteObjectIID) !=
                          nullptr;
  }
  if (expectTruncatedRoute) {
    KR_ObjectID routeID = context.searchObject("Route.truncated");
    IRouteObject* route = routeID.isNUL()
                              ? nullptr
                              : static_cast<IRouteObject*>(
                                    context.queryInterface(routeID,
                                                           IRouteObjectIID));
    if (route != nullptr) {
      const CFVector3 midpoint = route->GetPos(0.5);
      routePublished = route->GetNodeCnt() == 2 && route->GetLen() == 10 &&
                       NearlyEqual(midpoint.x, 5.0) &&
                       NearlyEqual(midpoint.z, 0.0);
    }
  }
  bool sparkPublished = !expectSpark;
  if (expectSpark) {
    KR_ObjectID flash = context.searchObject("Spark.Flash");
    sparkPublished =
        !flash.isNUL() && SparkAttributeState_IsRetailFlash(flash);
  }
  bool commonAttributesPublished = !expectCommonAttributes;
  if (expectCommonAttributes) {
    KR_ObjectID bird = context.searchObject("Bird.Attr.0");
    KR_ObjectID orphan = context.searchObject("Orphan.Attr.Default");
    KR_ObjectID artefact = context.searchObject("Artefact.Attr.0");
    commonAttributesPublished =
        !bird.isNUL() && !orphan.isNUL() && !artefact.isNUL() &&
        BirdAttributeState_IsRetailDefault(bird) &&
        OrphanAttributeState_IsRetailDefault(orphan) &&
        ArtefactAttributeState_IsRetailDefault(artefact);
  }
  g_arena.closeSeance();
  return matched && routePublished && sparkPublished &&
         commonAttributesPublished && !context.isExist("Storage") &&
         !context.isExist("Bird.Attr.0") &&
         !context.isExist("Orphan.Attr.Default") &&
         !context.isExist("Artefact.Attr.0") &&
         !context.isExist("Spark.Flash") &&
         !context.isExist("Route.fixture") &&
         !context.isExist("Route.by-table") &&
         !context.isExist("Route.by-class") &&
         !context.isExist("Route.truncated") &&
         !context.isExist("Route.missing") &&
         !context.isExist("Route.malformed") &&
         !context.isExist("Route.overlong") &&
         !context.isExist("Route.oversized") && Route::m_totalNodePos == 0;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4,
                "recovered script host requires a Win32 process");
  if (argc != 2) return Fail("expected a fixture directory");

  BirdAttributeState_Link();
  ArtefactAttributeState_Link();
  OrphanAttributeState_Link();
  SparkAttributeState_Link();

  std::string originalDirectory;
  std::string fixtureDirectory;
  if (!CurrentDirectory(originalDirectory) ||
      !FullPath(argv[1], fixtureDirectory) ||
      !EnsureDirectory(fixtureDirectory)) {
    return Fail("could not establish the fixture directory");
  }
  const char routeFixture[] =
      "Route.fixture\r\n"
      "3\r\n"
      "[0,0,0]\r\n"
      "[10,0,0]\r\n"
      "[10,0,10]\r\n";
  const std::string routePath =
      JoinPath(fixtureDirectory, "route-fixture.rt");
  const char malformedRouteFixture[] =
      "Route.malformed\r\n"
      "1\r\n"
      "[not-a-number,0,0]\r\n";
  const std::string malformedRoutePath =
      JoinPath(fixtureDirectory, "route-malformed.rt");
  const char truncatedRouteFixture[] =
      "Route.truncated\r\n"
      "3\r\n"
      "[0,0,0]\r\n"
      "[10,0,0]\r\n";
  const std::string truncatedRoutePath =
      JoinPath(fixtureDirectory, "route-truncated.rt");
  const std::string overlongRouteFixture =
      "Route.overlong\r\n2\r\n[0,0,0]\r\n[" +
      std::string(256, '1') + ",0,0]\r\n";
  const std::string overlongRoutePath =
      JoinPath(fixtureDirectory, "route-overlong.rt");
  if (!WriteFile(routePath, routeFixture) ||
      !WriteFile(malformedRoutePath, malformedRouteFixture) ||
      !WriteFile(truncatedRoutePath, truncatedRouteFixture) ||
      !WriteFile(overlongRoutePath, overlongRouteFixture.c_str()) ||
      SetCurrentDirectoryA(fixtureDirectory.c_str()) == FALSE) {
    return Fail("could not prepare the route fixture");
  }

  Session::m_moment = 0.0;

  RecoveredLegacyScriptHost detachedHost(&g_arena);
  SRecoveredLegacyScriptRunResult result = {};
  const SRecoveredLegacyScriptProfile profile =
      RecoveredLegacyScript_BootstrapProfile();
  if (RecoveredLegacyScript_RunMemory(nullptr, "invalid", profile, nullptr,
                                      0.0, &detachedHost, &result) ||
      result.status != RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT) {
    return Fail("invalid runner input did not fail closed", &result);
  }

  TStackCell stack[1] = {};
  TProcessContext invalidReference = {};
  invalidReference.m_stack = stack;
  invalidReference.m_stackSize = 1;
  if (detachedHost.WriteScriptInteger(&invalidReference, 1, 7) ||
      detachedHost.Issues() !=
          RECOVERED_LEGACY_SCRIPT_HOST_INVALID_STACK_REFERENCE) {
    return Fail("invalid script variable reference was not rejected");
  }
  detachedHost.Reset();

  const char validLfSource[] =
      "func void main()\n"
      "{\n"
      "}\n";
  SimulationContext invalidProfileContext(16, 32);
  SRecoveredLegacyScriptProfile invalidProfile = profile;
  invalidProfile.processCount = 2;
  if (RecoveredLegacyScript_RunMemory(
          validLfSource, "invalid_profile", invalidProfile,
          &invalidProfileContext, 0.0, &detachedHost, &result) ||
      result.status != RECOVERED_LEGACY_SCRIPT_RUN_INVALID_ARGUMENT ||
      invalidProfileContext.isExist("Storage")) {
    return Fail("invalid runner profile did not fail closed", &result);
  }

  if (!RunCase(validLfSource, "valid_lf",
               RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, 0, &result)) {
    return Fail("LF source was not normalized and executed", &result);
  }

  const char sparkAttributeSource[] = R"RR2NW_SCRIPT(
const int EDO_WRITE = 1;
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
func void s_SendEventNow(int event, int label, int objectID, int cachePos) extern;
func void s_SearchObjectID(var int objectID, var int cachePos, str name) extern;
func int s_AddClassTable(str className, int capacity) extern;
func void s_NewObject(int table, str name) extern;
func void SetF(int objectID, int cachePos, str name, float value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE); s_WriteStr(event,name);
  s_WriteFloat(event,value); s_CloseEventData(event);
  s_SendEventNow(event,s_ATTR_MSG_SET_DOUBLE,objectID,cachePos);
}
func void SetI(int objectID, int cachePos, str name, int value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE); s_WriteStr(event,name);
  s_WriteInt(event,value); s_CloseEventData(event);
  s_SendEventNow(event,s_ATTR_MSG_SET_INT,objectID,cachePos);
}
func void SetS(int objectID, int cachePos, str name, str value)
var int event;
{
  event := s_OpenEventData(EDO_WRITE); s_WriteStr(event,name);
  s_WriteStr(event,value); s_CloseEventData(event);
  s_SendEventNow(event,s_ATTR_MSG_SET_STR,objectID,cachePos);
}
func void Phase(int objectID, int cachePos, int num,
                int x, int y, int w, int h, float time,
                int brightness, int color, float radius)
var int event;
{
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event,num);
  s_Descend(event,RECT2D_I,0);
  s_WriteInt(event,x-w); s_WriteInt(event,y-h);
  s_WriteInt(event,x+w); s_WriteInt(event,y+h);
  s_Ascend(event);
  s_WriteFloat(event,time); s_WriteInt(event,brightness);
  s_WriteInt(event,color); s_WriteFloat(event,radius);
  s_CloseEventData(event);
  s_SendEventNow(event,sp_EV_SET_PHASE,objectID,cachePos);
}
func void main()
var int table, birdTable, orphanTable, artefactTable;
var int objectID, cachePos, event;
{
  table := s_AddClassTable("SparkAttr",3);
  s_NewObject(table,"Spark.Flash");
  s_SearchObjectID(objectID,cachePos,"Spark.Flash");
  event := s_OpenEventData(EDO_WRITE);
  s_WriteInt(event,6); s_CloseEventData(event);
  s_SendEventNow(event,sp_EV_SET_PHASE_COUNT,objectID,cachePos);
  Phase(objectID,cachePos,0, 25,45,20,40,0.04,100,LIGHT_COLOR_YELLOW, 7);
  Phase(objectID,cachePos,1, 78,46,20,40,0.03,200,LIGHT_COLOR_YELLOW,14);
  Phase(objectID,cachePos,2,126,47,20,40,0.03,100,LIGHT_COLOR_YELLOW,10);
  Phase(objectID,cachePos,3,180,44,20,40,0.10, 20,LIGHT_COLOR_YELLOW, 7);
  Phase(objectID,cachePos,4,230,44,20,40,0.10,  0,LIGHT_COLOR_YELLOW, 4);
  Phase(objectID,cachePos,5,230,44,20,40,0.00,  0,LIGHT_COLOR_YELLOW, 0);

  birdTable := s_AddClassTable("BirdAttr",1);
  s_NewObject(birdTable,"Bird.Attr.0");
  s_SearchObjectID(objectID,cachePos,"Bird.Attr.0");
  SetF(objectID,cachePos,"m_speed",2.0);
  SetS(objectID,cachePos,"m_skinName","sk.Bird.0");
  SetF(objectID,cachePos,"m_calcPosIncrement",0.2);

  orphanTable := s_AddClassTable("OrphanAttr",3);
  s_NewObject(orphanTable,"Orphan.Attr.Default");

  artefactTable := s_AddClassTable("ArtefactAttr",2);
  s_NewObject(artefactTable,"Artefact.Attr.0");
  s_SearchObjectID(objectID,cachePos,"Artefact.Attr.0");
  SetS(objectID,cachePos,"m_skinName","sk.Artefact.0");
  SetF(objectID,cachePos,"m_maxCoronaR",20.0);
  SetF(objectID,cachePos,"m_coronaR",0.4);
  SetI(objectID,cachePos,"m_coronaRGB",16711935);
  SetI(objectID,cachePos,"m_coronaAlpha",150);
}
)RR2NW_SCRIPT";
  if (!RunCase(sparkAttributeSource, "spark_attribute",
               RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, 0, &result,
               false, false, false, true, true)) {
    return Fail("retail common attribute script did not execute", &result);
  }

  const char routeSource[] =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_LoadRoute(int table, str fileName, str routeName) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 2);\n"
      "  s_LoadRoute(routeTable, \"route-fixture.rt\", \"Route.fixture\");\n"
      "}\n";
  if (!RunCase(routeSource, "route_load",
                RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, 0, &result, true)) {
    return Fail("Route table/object binding did not load and roll back",
                &result);
  }

  const char truncatedRouteSource[] =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_LoadRoute(int table, str fileName, str routeName) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 1);\n"
      "  s_LoadRoute(routeTable, \"route-truncated.rt\", "
      "\"Route.truncated\");\n"
      "}\n";
  if (!RunCase(truncatedRouteSource, "truncated_route",
               RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, 0, &result, false, false,
               true)) {
    return Fail("retail-style truncated Route was not safely clamped", &result);
  }

  const char routeObjectSource[] =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_NewObject(int table, str name) extern;\n"
      "func void s_NewObjectN(str className, str name) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 2);\n"
      "  s_NewObject(routeTable, \"Route.by-table\");\n"
      "  s_NewObjectN(\"Route\", \"Route.by-class\");\n"
      "}\n";
  if (!RunCase(routeObjectSource, "route_object_creation",
               RECOVERED_LEGACY_SCRIPT_RUN_SUCCESS, 0, &result, false,
               true)) {
    return Fail("Route object-creation bindings did not publish interfaces",
                &result);
  }

  const char missingRouteSource[] =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_LoadRoute(int table, str fileName, str routeName) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 1);\n"
      "  s_LoadRoute(routeTable, \"missing-route.rt\", \"Route.missing\");\n"
      "}\n";
  if (!RunCase(missingRouteSource, "missing_route",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE, &result)) {
    return Fail("missing Route input did not fail closed", &result);
  }

  const char malformedRouteSource[] =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_LoadRoute(int table, str fileName, str routeName) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 1);\n"
      "  s_LoadRoute(routeTable, \"route-malformed.rt\", "
      "\"Route.malformed\");\n"
      "}\n";
  if (!RunCase(malformedRouteSource, "malformed_route",
                RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
                RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE, &result)) {
    return Fail("malformed Route input did not fail closed", &result);
  }

  const char overlongRouteSource[] =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_LoadRoute(int table, str fileName, str routeName) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 1);\n"
      "  s_LoadRoute(routeTable, \"route-overlong.rt\", \"Route.overlong\");\n"
      "}\n";
  if (!RunCase(overlongRouteSource, "overlong_route_record",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE, &result)) {
    return Fail("overlong Route record did not fail closed", &result);
  }

  const std::string oversizedRouteSource =
      "func int s_AddClassTable(str className, int capacity) extern;\n"
      "func void s_LoadRoute(int table, str fileName, str routeName) extern;\n"
      "func void main()\n"
      "var int routeTable;\n"
      "{\n"
      "  routeTable := s_AddClassTable(\"Route\", 1);\n"
      "  s_LoadRoute(routeTable, \"" +
      std::string(s_EventData::BUFF_SIZE, 'x') +
      "\", \"Route.oversized\");\n"
      "}\n";
  if (!RunCase(oversizedRouteSource.c_str(), "oversized_route_path",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_ROUTE_LOAD_FAILURE, &result)) {
    return Fail("oversized Route path did not fail closed", &result);
  }

  const char invalidEventSource[] =
      "func void s_CloseEventData(int event) extern;\n"
      "func void main()\n"
      "{\n"
      "  s_CloseEventData(99);\n"
      "}\n";
  if (!RunCase(invalidEventSource, "invalid_event",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_INVALID_EVENT, &result)) {
    return Fail("invalid event handle was not diagnosed", &result);
  }

  const char exhaustedEventsSource[] =
      "func int s_OpenEventData(int style) extern;\n"
      "func void main()\n"
      "var int e0,e1,e2,e3,e4,e5,e6,e7,e8;\n"
      "{\n"
      "  e0 := s_OpenEventData(1);\n"
      "  e1 := s_OpenEventData(1);\n"
      "  e2 := s_OpenEventData(1);\n"
      "  e3 := s_OpenEventData(1);\n"
      "  e4 := s_OpenEventData(1);\n"
      "  e5 := s_OpenEventData(1);\n"
      "  e6 := s_OpenEventData(1);\n"
      "  e7 := s_OpenEventData(1);\n"
      "  e8 := s_OpenEventData(1);\n"
      "}\n";
  if (!RunCase(exhaustedEventsSource, "exhausted_events",
               RECOVERED_LEGACY_SCRIPT_RUN_HOST_FAILURE,
               RECOVERED_LEGACY_SCRIPT_HOST_EVENT_POOL_EXHAUSTED, &result)) {
    return Fail("event-pool exhaustion was not diagnosed", &result);
  }

  const char invalidSource[] = "func void main( { }";
  if (!RunCase(invalidSource, "invalid_source",
               RECOVERED_LEGACY_SCRIPT_RUN_COMPILE_FAILURE, 0, &result)) {
    return Fail("compiler longjmp was not contained", &result);
  }

  const bool restored =
      SetCurrentDirectoryA(originalDirectory.c_str()) != FALSE;
  DeleteFileA(routePath.c_str());
  DeleteFileA(malformedRoutePath.c_str());
  DeleteFileA(truncatedRoutePath.c_str());
  DeleteFileA(overlongRoutePath.c_str());
  RemoveDirectoryA(fixtureDirectory.c_str());
  if (!restored) return Fail("working directory was not restored");

  std::printf("legacy script host bindings=15 constants=7 lf=normalized "
              "spark=retail-phases common_attrs=bird,orphan,artefact "
              "route=loaded route_eof=clamped "
              "errors=fail-closed rollback=clean\n");
  return EXIT_SUCCESS;
}
