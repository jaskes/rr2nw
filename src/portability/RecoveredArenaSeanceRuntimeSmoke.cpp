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
#include "obase/artefact/ArtefactAttributeState.h"
#include "obase/bird/BirdAttributeState.h"
#include "obase/explosion/ExplosionAttributeState.h"
#include "obase/orphan/OrphanAttributeState.h"
#include "obase/route/route.h"
#include "obase/smoke/SmokeAttributeState.h"
#include "storage/h/subject.h"

#include "RecoveredArenaSeanceRuntime.h"

namespace {

unsigned long long g_explosionFixtureFingerprint = 0;
unsigned long long g_skinCatalogFixtureFingerprint = 0;

int Fail(const char* message) {
  RecoveredArenaSeance_Release();
  std::fprintf(stderr,
               "recovered-arena-seance-runtime-smoke: %s "
               "(open=%d script=%d bird=%d portal=%d orphan=%d artefact=%d "
               "smoke=%d explosion=%d skin=%d spark=%d route=%d vehicle=%d "
               "issues=%llu error=%s)\n",
               message, RecoveredArenaSeance_IsOpen() ? 1 : 0,
               RecoveredArenaSeance_ScriptCompleted() ? 1 : 0,
               RecoveredArenaSeance_BirdAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_PortalReady() ? 1 : 0,
               RecoveredArenaSeance_OrphanAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_ArtefactAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_SmokeAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_ExplosionAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_SkinResourcesReady() ? 1 : 0,
               RecoveredArenaSeance_SparkAttributesReady() ? 1 : 0,
               RecoveredArenaSeance_RouteReady() ? 1 : 0,
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
         !RecoveredArenaSeance_BirdAttributesReady() &&
         !RecoveredArenaSeance_PortalReady() &&
         !RecoveredArenaSeance_OrphanAttributesReady() &&
         !RecoveredArenaSeance_ArtefactAttributesReady() &&
         !RecoveredArenaSeance_SmokeAttributesReady() &&
         !RecoveredArenaSeance_ExplosionAttributesReady() &&
         !RecoveredArenaSeance_SkinResourcesReady() &&
         !RecoveredArenaSeance_SparkAttributesReady() &&
         !RecoveredArenaSeance_RouteReady() &&
         !RecoveredArenaSeance_VehicleReady() && g_vehicle == nullptr &&
         !context.isExist("Storage") && !context.isExist("Bird.Attr.0") &&
         !context.isExist("Orphan.Attr.Default") &&
         !context.isExist("Artefact.Attr.0") &&
         !context.isExist("Smoke.Attr.Small") &&
         !context.isExist("Smoke.Attr.Fire.Corpse") &&
         !context.isExist("Expl.Test.0") &&
         !context.isExist("Spark.Flash") &&
         !context.isExist("Vehicle.Default") &&
         Route::m_totalNodePos == 0;
}

bool RunCycle() {
  SimulationContext context(64, 128);
  if (!RecoveredArenaSeance_Initialize(&context, Session::m_moment) ||
      !RecoveredArenaSeance_IsOpen() ||
      !RecoveredArenaSeance_ScriptCompleted() ||
      !RecoveredArenaSeance_BirdAttributesReady() ||
      !RecoveredArenaSeance_PortalReady() ||
      !RecoveredArenaSeance_OrphanAttributesReady() ||
      !RecoveredArenaSeance_ArtefactAttributesReady() ||
      !RecoveredArenaSeance_SmokeAttributesReady() ||
      !RecoveredArenaSeance_ExplosionAttributesReady() ||
      !RecoveredArenaSeance_SkinResourcesReady() ||
      !RecoveredArenaSeance_SparkAttributesReady() ||
      !RecoveredArenaSeance_RouteReady() ||
      !RecoveredArenaSeance_VehicleReady() ||
      RecoveredArenaSeance_Issues() != 0 ||
      g_arena.searchSeanceClassTable("BirdAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Portal") == ct_NULLID ||
      g_arena.searchSeanceClassTable("OrphanAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("ArtefactAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("SmokeAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("ExplosionAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Explosion") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Skin") == ct_NULLID ||
      g_arena.searchSeanceClassTable("SkinSpr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("SparkAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Route") == ct_NULLID ||
      g_arena.searchSeanceClassTable("VehicleAttr") == ct_NULLID ||
      g_arena.searchSeanceClassTable("Vehicle") == ct_NULLID) {
    RecoveredArenaSeance_Release();
    return false;
  }

  KR_ObjectID storage = context.searchObject("Storage");
  KR_ObjectID bird = context.searchObject("Bird.Attr.0");
  KR_ObjectID orphan = context.searchObject("Orphan.Attr.Default");
  KR_ObjectID artefact = context.searchObject("Artefact.Attr.0");
  KR_ObjectID flash = context.searchObject("Spark.Flash");
  KR_ObjectID smoke = context.searchObject("Smoke.Attr.Small");
  KR_ObjectID explosion = context.searchObject("Expl.Test.0");
  AttributeExplosion* explosionAttribute =
      explosion.isNUL()
          ? nullptr
          : static_cast<AttributeExplosion*>(
                __attrExplosionTable.searchAttribute(explosion));
  KR_ObjectID vehicle = context.searchObject("Vehicle.Default");
  const bool vehiclePublished =
      !storage.isNUL() && !bird.isNUL() && !orphan.isNUL() &&
      !artefact.isNUL() && !flash.isNUL() && !vehicle.isNUL() &&
      !smoke.isNUL() && !explosion.isNUL() &&
      BirdAttributeState_IsRetailDefault(bird) &&
      OrphanAttributeState_IsRetailDefault(orphan) &&
      ArtefactAttributeState_IsRetailDefault(artefact) &&
      SmokeAttributeState_IsRetailRoster(&context) &&
      ExplosionAttributeState_IsKnownRoster(&context) &&
      ExplosionAttributeState_RosterSize(&context) == 10 &&
      explosionAttribute != nullptr &&
      explosionAttribute->m_useLight == 0 &&
      explosionAttribute->m_impulseCoeff == 1234 &&
      explosionAttribute->m_hTexture == nullptr &&
      explosionAttribute->m_cacheSkin == nullptr &&
      explosionAttribute->m_wav == nullptr &&
      g_vehicle != nullptr &&
      context.queryInterface(vehicle, IVehicleIID) == g_vehicle;
  g_explosionFixtureFingerprint =
      ExplosionAttributeState_Fingerprint(&context);
  g_skinCatalogFixtureFingerprint =
      RecoveredArenaSeance_SkinCatalogFingerprint();

  RecoveredArenaSeance_Release();
  RecoveredArenaSeance_Release();
  return vehiclePublished && IsReleased(context);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    return Fail("expected a fixture directory and retail SMOKE.SCI");
  }

  RecoveredArenaSeance_Release();
  if (RecoveredArenaSeance_Initialize(nullptr, 0.0) != FALSE ||
      RecoveredArenaSeance_Issues() !=
          RECOVERED_ARENA_SEANCE_INVALID_CONTEXT ||
      RecoveredArenaSeance_IsOpen() || g_vehicle != nullptr) {
    return Fail("null context did not fail transactionally");
  }

  std::string originalDirectory;
  std::string fixtureDirectory;
  std::string levelDirectory;
  if (!CurrentDirectory(originalDirectory) ||
      !FullPath(argv[1], fixtureDirectory) ||
      !EnsureDirectory(fixtureDirectory)) {
    return Fail("could not establish the fixture directory");
  }
  levelDirectory = JoinPath(fixtureDirectory, "Level.Fixture");
  if (!EnsureDirectory(levelDirectory)) {
    return Fail("could not establish the fixture Level directory");
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
  const char explosionRootFixture[] =
      "func void ExplosionFixtureRoot()\r\n"
      "{\r\n"
      "}\r\n";
  const char explosionLevelFixture[] =
      "func void main_CreateExplosionAttr()\r\n"
      " var int ctID, objectID, cachePos;\r\n"
      "{\r\n"
      "  s_AddClassTable(\"Explosion\", 2);\r\n"
      "  ctID := s_AddClassTable(\"ExplosionAttr\", 10);\r\n"
      "  New(ctID, \"Expl.Test.0\", objectID, cachePos);\r\n"
      "  SetAttribute_i(objectID, cachePos, \"m_useLight\", 0);\r\n"
      "  SetAttribute_f(objectID, cachePos, \"m_impulseCoeff\", 1234);\r\n"
      "  New(ctID, \"Expl.Test.1\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.2\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.3\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.4\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.5\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.6\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.7\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.8\", objectID, cachePos);\r\n"
      "  New(ctID, \"Expl.Test.9\", objectID, cachePos);\r\n"
      "}\r\n";
  const char skinLevelFixture[] =
      "func void main_LoadSkin()\r\n"
      " var int ctIDSkin;\r\n"
      "{\r\n"
      " ctIDSkin := s_AddClassTable(\"Skin\",1);\r\n"
      " ctIDSkin := s_AddClassTable(\"SkinSpr\",1);\r\n"
      "}\r\n";
  const char invalidSkinLevelFixture[] =
      "func void main_LoadSkin()\r\n"
      " var int ctIDSkin;\r\n"
      "{\r\n"
      " ctIDSkin := s_AddClassTable(\"Skin\",2);\r\n"
      " ctIDSkin := s_AddClassTable(\"SkinSpr\",1);\r\n"
      "}\r\n";
  const std::string config = JoinPath(levelDirectory, "vessels.cfg");
  const std::string smokeCopy = JoinPath(fixtureDirectory, "SMOKE.SCI");
  const std::string explosionCopy =
      JoinPath(fixtureDirectory, "EXPLOSION.SCI");
  const std::string scincDirectory = JoinPath(levelDirectory, "SCINC");
  const std::string explosionLocalCopy =
      JoinPath(scincDirectory, "EXPLOSION_LOC.SCI");
  const std::string skinCopy = JoinPath(scincDirectory, "SKIN.SCI");
  DeleteFileA(smokeCopy.c_str());
  DeleteFileA(explosionCopy.c_str());
  if (!EnsureDirectory(scincDirectory)) {
    return Fail("could not establish the fixture SCINC directory");
  }
  DeleteFileA(explosionLocalCopy.c_str());
  DeleteFileA(skinCopy.c_str());
  if (!WriteFile(config, fixture) ||
      SetCurrentDirectoryA(levelDirectory.c_str()) == FALSE) {
    return Fail("could not prepare retail-script Arena fixture");
  }

  Session::m_moment = 0.0;
  SimulationContext missingSourceContext(64, 128);
  const bool missingSourceRejected =
      RecoveredArenaSeance_Initialize(&missingSourceContext, 0.0) == FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SMOKE_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingSourceContext);
  if (CopyFileA(argv[2], smokeCopy.c_str(), FALSE) == FALSE) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not copy retail SMOKE.SCI into Arena fixture");
  }

  SimulationContext missingExplosionRootContext(64, 128);
  const bool missingExplosionRootRejected =
      RecoveredArenaSeance_Initialize(&missingExplosionRootContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingExplosionRootContext);
  if (!WriteFile(explosionCopy, explosionRootFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write bounded Explosion root fixture");
  }

  SimulationContext missingExplosionLocalContext(64, 128);
  const bool missingExplosionLocalRejected =
      RecoveredArenaSeance_Initialize(&missingExplosionLocalContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_SOURCE_UNAVAILABLE) != 0 &&
      IsReleased(missingExplosionLocalContext);
  if (!WriteFile(explosionLocalCopy, explosionLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write bounded Explosion Level fixture");
  }

  SimulationContext missingSkinCatalogContext(64, 128);
  const bool missingSkinCatalogRejected =
      RecoveredArenaSeance_Initialize(&missingSkinCatalogContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SKIN_CATALOG_INVALID) != 0 &&
      IsReleased(missingSkinCatalogContext);
  if (!WriteFile(skinCopy, invalidSkinLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Skin catalog fixture");
  }
  SimulationContext invalidSkinCatalogContext(64, 128);
  const bool invalidSkinCatalogRejected =
      RecoveredArenaSeance_Initialize(&invalidSkinCatalogContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_SKIN_RESOURCE_ROSTER_INVALID) != 0 &&
      IsReleased(invalidSkinCatalogContext);
  if (!WriteFile(skinCopy, skinLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore bounded Skin catalog fixture");
  }

  std::string invalidExplosionLevelFixture = explosionLevelFixture;
  const std::size_t impulse = invalidExplosionLevelFixture.find("1234");
  if (impulse == std::string::npos) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not corrupt Explosion fixture deterministically");
  }
  invalidExplosionLevelFixture.replace(impulse, 4, "1235");
  if (!WriteFile(explosionLocalCopy,
                 invalidExplosionLevelFixture.c_str())) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not write invalid Explosion Level fixture");
  }
  SimulationContext invalidExplosionRosterContext(64, 128);
  const bool invalidExplosionRosterRejected =
      RecoveredArenaSeance_Initialize(&invalidExplosionRosterContext, 0.0) ==
          FALSE &&
      (RecoveredArenaSeance_Issues() &
       RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_ROSTER_INVALID) != 0 &&
      IsReleased(invalidExplosionRosterContext);
  if (!WriteFile(explosionLocalCopy, explosionLevelFixture)) {
    SetCurrentDirectoryA(originalDirectory.c_str());
    return Fail("could not restore valid Explosion Level fixture");
  }

  const bool firstCycle = RunCycle();
  const bool secondCycle = firstCycle && RunCycle();
  const bool restored =
      SetCurrentDirectoryA(originalDirectory.c_str()) != FALSE;
  DeleteFileA(config.c_str());
  DeleteFileA(smokeCopy.c_str());
  DeleteFileA(explosionCopy.c_str());
  DeleteFileA(explosionLocalCopy.c_str());
  DeleteFileA(skinCopy.c_str());
  RemoveDirectoryA(scincDirectory.c_str());
  RemoveDirectoryA(levelDirectory.c_str());
  RemoveDirectoryA(fixtureDirectory.c_str());

  if (!missingSourceRejected) {
    return Fail("missing retail SMOKE.SCI was not rejected transactionally");
  }
  if (!missingExplosionRootRejected || !missingExplosionLocalRejected) {
    return Fail("missing retail Explosion fragments were not rejected "
                "transactionally");
  }
  if (!invalidExplosionRosterRejected) {
    return Fail("invalid Explosion attribute roster was not rejected "
                "transactionally");
  }
  if (!missingSkinCatalogRejected) {
    return Fail("missing retail Skin catalog was not rejected transactionally");
  }
  if (!invalidSkinCatalogRejected) {
    return Fail("invalid Skin catalog was not rejected transactionally");
  }
  if (!firstCycle) return Fail("first real Vehicle seance failed");
  if (!secondCycle) return Fail("Vehicle seance reconstruction failed");
  if (!restored) return Fail("working directory was not restored");

  std::printf("bounded arena seance cycles=2 missing-smoke=rollback "
              "missing-explosion-root-local=rollback "
              "invalid-explosion-roster=rollback "
              "missing-skin-catalog=rollback "
              "invalid-skin-catalog=rollback "
              "script=legacy-vm "
              "common_attrs=bird,orphan,artefact portal=table "
              "skin_resources=preflight-empty-fixture "
              "smoke_attrs=retail-18 explosion_attrs=level-aware-90-field "
              "spark=Spark.Flash route=table "
              "vehicle=Vehicle.Default "
              "explosion_fingerprint=%llu skin_catalog_fingerprint=%llu "
              "rollback=idempotent\n",
              g_explosionFixtureFingerprint,
              g_skinCatalogFixtureFingerprint);
  return EXIT_SUCCESS;
}
