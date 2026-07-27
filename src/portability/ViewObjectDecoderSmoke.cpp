#include <cstdio>
#include <cstdlib>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "brend.h"
#include "filesys.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "ZavShutdownState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "view-object-decoder-smoke: %s\n", message);
  return EXIT_FAILURE;
}

struct SDecodedModels {
  int models;
  int baseSets;
  int bases;
  int bushes;
};

bool AddModel(CTaggedFile& file, bool sky, SDecodedModels& decoded) {
  CViewObjectModel model;
  model.Read(file, false);
  if (!file.IsOK() || model.NumBaseSets() <= 0 ||
      model.Radius() < 0.0 || model.Height() < 0.0) {
    return false;
  }

  ++decoded.models;
  decoded.baseSets += model.NumBaseSets();
  for (int setIndex = 0; setIndex < model.NumBaseSets(); ++setIndex) {
    CViewObjectBaseSet& set = model.BaseSet(setIndex);
    decoded.bases += set.NumBases();
    for (int baseIndex = 0; baseIndex < set.NumBases(); ++baseIndex) {
      decoded.bushes += set.Base(baseIndex).NBushes();
    }
  }
  model.Split(sky);
  return true;
}

bool ReadModelFile(const char* path, bool sky, SDecodedModels& decoded) {
  std::printf("decoding %s\n", path);
  std::fflush(stdout);
  CTaggedFile file(false);
  if (!file.Open(path, false)) return false;
  const bool valid = AddModel(file, sky, decoded);
  return file.Close(false) && valid;
}

bool ReadLevelModels(SDecodedModels& decoded) {
  if (!ReadModelFile("sky.vbc", true, decoded)) return false;

  const char* scenePath = RecoveredLevelRuntime_SceneFile();
  if (scenePath == nullptr) return false;
  CTaggedFile scene(false);
  if (!scene.Open(scenePath, false) ||
      scene.Descend("SCEN", false, 0, false) != 0 ||
      scene.Descend("SCEH", true, 0, false) != 0) {
    return false;
  }

  int namedBases = 0;
  int directBases = 0;
  int references = 0;
  int reductions = 0;
  double cellSize = 0.0;
  double heightRatio = 0.0;
  bool valid = scene.ReadInt(namedBases) && scene.ReadInt(directBases) &&
               scene.ReadInt(references) && scene.ReadDouble(cellSize) &&
               scene.ReadDouble(heightRatio) && scene.ReadInt(reductions) &&
               namedBases >= 0 && directBases >= 0 && references >= 0 &&
               reductions >= 0 && cellSize > 0.0 && heightRatio > 0.0 &&
               scene.Ascend();

  if (valid && namedBases > 0) {
    valid = scene.Descend("OBJN", true, 0, false) == 0;
    for (int index = 0; valid && index < namedBases; ++index) {
      char name[80] = {};
      valid = scene.Read(name, sizeof(name)) == sizeof(name) &&
              std::memchr(name, '\0', sizeof(name)) != nullptr &&
              ReadModelFile(name, false, decoded);
    }
    valid = valid && scene.Ascend();
  }

  if (valid && directBases > 0) {
    valid = scene.Descend("OBJD", true, 0, false) == 0;
    for (int index = 0; valid && index < directBases; ++index) {
      valid = AddModel(scene, false, decoded);
    }
    valid = valid && scene.Ascend();
  }

  return scene.Close(false) && valid;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }

  {
    CViewObjectModel empty;
    if (empty.NumBaseSets() != 0) {
      return Fail("default object model was not empty");
    }
  }

  if (argc == 1) return EXIT_SUCCESS;

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("headless graph initialization failed");
  }
  GameEntry_UseRecoveredRuntime();
  if (!RecoveredLevelRuntime_Prepare(argv[1]) ||
      !RecoveredLevelAssets_Initialize()) {
    ZAV_Deinit();
    return Fail("retail level assets did not initialize");
  }
  std::printf("initializing bush decoder cache\n");
  std::fflush(stdout);
  b_CreateCacheBush();
  std::printf("bush decoder cache ready\n");
  std::fflush(stdout);

  SDecodedModels decoded = {};
  const bool loaded = ReadLevelModels(decoded);
  b_DeleteCacheBush();
  ZAV_Deinit();
  if (!loaded || decoded.models <= 1 || decoded.baseSets < decoded.models ||
      decoded.bases < decoded.baseSets ||
      RecoveredSoftwareGraph_IsReady() ||
      RecoveredLevelAssets_IsReady() ||
      RecoveredLevelRuntime_IsPrepared()) {
    return Fail("retail object models did not load and release cleanly");
  }
  std::printf("decoded models=%d basesets=%d bases=%d bushes=%d\n",
              decoded.models, decoded.baseSets, decoded.bases,
              decoded.bushes);
  return EXIT_SUCCESS;
}
