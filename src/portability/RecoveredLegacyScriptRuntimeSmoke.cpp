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
#include "obase/route/route.h"
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
             bool expectTruncatedRoute = false) {
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
  g_arena.closeSeance();
  return matched && routePublished && !context.isExist("Storage") &&
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

  std::printf("legacy script host bindings=11 lf=normalized route=loaded "
              "route_eof=clamped errors=fail-closed rollback=clean\n");
  return EXIT_SUCCESS;
}
