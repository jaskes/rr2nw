#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

class CGRPanel;
#include "h/vehicle.h"
#include "kernel/h/context.h"
#include "kernel/h/session.h"
#include "storage/h/subject.h"

#include "RecoveredArenaSeanceRuntime.h"

namespace {

int Fail(const char* message) {
  RecoveredArenaSeance_Release();
  std::fprintf(stderr,
               "recovered-arena-seance-runtime-smoke: %s "
               "(open=%d script=%d vehicle=%d issues=%u error=%s)\n",
               message, RecoveredArenaSeance_IsOpen() ? 1 : 0,
               RecoveredArenaSeance_ScriptCompleted() ? 1 : 0,
               RecoveredArenaSeance_VehicleReady() ? 1 : 0,
               RecoveredArenaSeance_Issues(),
               RecoveredArenaSeance_LastError());
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

bool IsReleased(SimulationContext& context) {
  return !RecoveredArenaSeance_IsOpen() &&
         !RecoveredArenaSeance_ScriptCompleted() &&
         !RecoveredArenaSeance_VehicleReady() && g_vehicle == nullptr &&
         !context.isExist("Storage") && !context.isExist("Vehicle.Default");
}

bool RunCycle() {
  SimulationContext context(64, 128);
  if (!RecoveredArenaSeance_Initialize(&context, Session::m_moment) ||
      !RecoveredArenaSeance_IsOpen() ||
      !RecoveredArenaSeance_ScriptCompleted() ||
      !RecoveredArenaSeance_VehicleReady() ||
      RecoveredArenaSeance_Issues() != 0 ||
      g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID) {
    RecoveredArenaSeance_Release();
    return false;
  }

  KR_ObjectID storage = context.searchObject("Storage");
  KR_ObjectID vehicle = context.searchObject("Vehicle.Default");
  const bool vehiclePublished =
      !storage.isNUL() && !vehicle.isNUL() && g_vehicle != nullptr &&
      context.queryInterface(vehicle, IVehicleIID) == g_vehicle;

  RecoveredArenaSeance_Release();
  RecoveredArenaSeance_Release();
  return vehiclePublished && IsReleased(context);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return Fail("expected a fixture directory");

  RecoveredArenaSeance_Release();
  if (RecoveredArenaSeance_Initialize(nullptr, 0.0) != FALSE ||
      RecoveredArenaSeance_Issues() !=
          RECOVERED_ARENA_SEANCE_INVALID_CONTEXT ||
      RecoveredArenaSeance_IsOpen() || g_vehicle != nullptr) {
    return Fail("null context did not fail transactionally");
  }

  std::string originalDirectory;
  std::string fixtureDirectory;
  if (!CurrentDirectory(originalDirectory) ||
      !FullPath(argv[1], fixtureDirectory) ||
      !EnsureDirectory(fixtureDirectory)) {
    return Fail("could not establish the fixture directory");
  }

  // These legacy single-byte comment characters reproduce the exact ctype
  // edge case present in retail vessels.cfg while the empty sections retain
  // the recovered vehicle dynamics defaults.
  const char fixture[] =
      "# legacy \xAC\xA8\xE0\r\n"
      "[Dragon]\r\n"
      "[Emv0]\r\n"
      "[Walk0]\r\n"
      "[Tank1]\r\n"
      "[Tank2]\r\n"
      "[Tank3]\r\n"
      "[Dead]\r\n";
  const std::string config = JoinPath(fixtureDirectory, "vessels.cfg");
  if (!WriteFile(config, fixture) ||
      SetCurrentDirectoryA(fixtureDirectory.c_str()) == FALSE) {
    return Fail("could not prepare vessels.cfg");
  }

  Session::m_moment = 0.0;
  const bool firstCycle = RunCycle();
  const bool secondCycle = firstCycle && RunCycle();
  const bool restored =
      SetCurrentDirectoryA(originalDirectory.c_str()) != FALSE;
  DeleteFileA(config.c_str());
  RemoveDirectoryA(fixtureDirectory.c_str());

  if (!firstCycle) return Fail("first real Vehicle seance failed");
  if (!secondCycle) return Fail("Vehicle seance reconstruction failed");
  if (!restored) return Fail("working directory was not restored");

  std::printf("bounded arena seance cycles=2 script=legacy-vm "
              "vehicle=Vehicle.Default rollback=idempotent\n");
  return EXIT_SUCCESS;
}
