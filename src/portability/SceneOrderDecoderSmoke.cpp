#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "filesys.h"

#include "GameEntryRuntimeState.h"
#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredSceneOrderRuntime.h"
#include "RecoveredSoftwareGraph.h"
#include "RecoveredTerrainRuntime.h"
#include "ZavShutdownState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "scene-order-decoder-smoke: %s\n", message);
  return EXIT_FAILURE;
}

class CTemporarySceneFile {
 public:
  bool Create() {
    char temporaryRoot[MAX_PATH] = {};
    char uniquePath[MAX_PATH] = {};
    const DWORD rootLength = GetTempPathA(MAX_PATH, temporaryRoot);
    if (rootLength == 0 || rootLength >= MAX_PATH ||
        GetTempFileNameA(temporaryRoot, "r2s", 0, uniquePath) == 0) {
      return false;
    }
    path_ = uniquePath;
    return true;
  }

  ~CTemporarySceneFile() {
    if (!path_.empty()) DeleteFileA(path_.c_str());
  }

  const std::string& Path() const { return path_; }

 private:
  std::string path_;
};

bool BeginOrder(CTaggedFile& file, CViewOrdered::VORDTYPE type,
                int declaration = -1, int index = -1) {
  return file.Descend("ORDR", false, 1, false) == 1 &&
         file.Descend("ORDH", true, 1, false) == 1 &&
         file.WriteInt(static_cast<int>(type)) &&
         file.WriteInt(declaration) && file.WriteInt(index);
}

bool WriteTransform(CTaggedFile& file) {
  CFMatrix3x4 transform;
  transform.LoadIdentity();
  return transform.Write(file);
}

bool WriteEmptyOrder(CTaggedFile& file) {
  return BeginOrder(file, CViewOrdered::VOT_EMPTY) && file.Ascend() &&
         file.Ascend();
}

bool WriteObjectOrder(CTaggedFile& file) {
  return BeginOrder(file, CViewOrdered::VOT_OBJECT, 0, 0) &&
         file.WriteInt(0) && WriteTransform(file) && file.Ascend() &&
         file.Ascend();
}

bool WriteMapPrimary(CTaggedFile& file, int secondary, bool withOrder) {
  bool valid = file.Descend("M1PE", false, 0, false) == 0 &&
               file.Descend("M1PH", true, 0, false) == 0 &&
               file.WriteInt(1) && file.Ascend() &&
               file.Descend("M1SE", false, 0, false) == 0 &&
               file.Descend("M1SH", true, 0, false) == 0 &&
               file.WriteInt(secondary) && file.Ascend();
  if (valid && withOrder) valid = WriteObjectOrder(file);
  return valid && file.Ascend() && file.Ascend();
}

bool WriteEmptyMapPrimary(CTaggedFile& file) {
  return file.Descend("M1PE", false, 0, false) == 0 &&
         file.Descend("M1PH", true, 0, false) == 0 &&
         file.WriteInt(0) && file.Ascend() && file.Ascend();
}

bool WriteLandOrder(CTaggedFile& file, bool corruptCopyReference) {
  CRect2 rect(0, 0, 1, 1);
  return BeginOrder(file, CViewOrdered::VOT_LANDPIECE) && file.Ascend() &&
         file.Descend("LNDP", false, 0, false) == 0 &&
         file.Descend("LDPH", true, 0, false) == 0 && rect.Write(file) &&
         file.WriteBool(true) && file.Ascend() &&
         file.Descend("MAP2", false, 0, false) == 0 &&
         file.Descend("MAP1", false, 0, false) == 0 &&
         WriteMapPrimary(file, 0, true) && file.Ascend() &&
         file.Descend("MAP1", false, 0, false) == 0 &&
         WriteMapPrimary(file, corruptCopyReference ? 1 : 0, false) &&
         WriteEmptyMapPrimary(file) && file.Ascend() && file.Ascend() &&
         file.Ascend() && file.Ascend();
}

bool WriteShelterOrder(CTaggedFile& file) {
  return BeginOrder(file, CViewOrdered::VOT_SHELTERORDER) &&
         file.WriteInt(0) && WriteTransform(file) && file.Ascend() &&
         WriteEmptyOrder(file) && file.Ascend();
}

bool WriteRootOrder(CTaggedFile& file, bool corruptCopyReference) {
  const CFVector3 point(0.0, 0.0, 0.0);
  const CFVector3 normal(0.0, 1.0, 0.0);
  return BeginOrder(file, CViewOrdered::VOT_ORDER) && point.Write(file) &&
         file.WriteInt(1) && normal.Write(file) && file.WriteDouble(0.0) &&
         file.Ascend() && WriteLandOrder(file, corruptCopyReference) &&
         WriteShelterOrder(file) && file.Ascend();
}

bool WriteScene(const std::string& path, bool corruptCopyReference) {
  CTaggedFile file(false);
  if (!file.Create(path.c_str(), false)) return false;

  char modelName[80] = {};
  std::strcpy(modelName, "model.vbc");
  char declarationName[40] = {};
  std::strcpy(declarationName, "objects");

  const bool valid =
      file.Descend("SCEN", false, 0, false) == 0 &&
      file.Descend("SCEH", true, 0, false) == 0 && file.WriteInt(1) &&
      file.WriteInt(0) && file.WriteInt(2) && file.WriteDouble(4.5) &&
      file.WriteDouble(0.25) && file.WriteInt(4) && file.Ascend() &&
      file.Descend("OBJN", true, 0, false) == 0 &&
      file.Write(modelName, sizeof(modelName)) == sizeof(modelName) &&
      file.Ascend() && file.Descend("NRMS", true, 0, false) == 0 &&
      file.Ascend() && file.Descend("NAMS", false, 0, false) == 0 &&
      file.Descend("NAME", true, 0, false) == 0 && file.WriteInt(1) &&
      file.WriteInt(1) &&
      file.Write(declarationName, sizeof(declarationName)) ==
          sizeof(declarationName) &&
      file.Ascend() && file.Ascend() &&
      WriteRootOrder(file, corruptCopyReference) && file.Ascend();
  const bool closed = file.Close(false);
  return valid && closed;
}

bool RunSyntheticContract() {
  RecoveredSceneOrder_Release();
  RecoveredSceneOrder_Release();
  if (RecoveredSceneOrder_Initialize() != FALSE ||
      RecoveredSceneOrder_IsReady() ||
      RecoveredSceneOrder_Issues() !=
          RECOVERED_SCENE_ORDER_MISSING_DEPENDENCY) {
    return false;
  }

  SRecoveredSceneHeader header = {1, 0, 2, 4, 4.5, 0.25};
  CTemporarySceneFile scene;
  if (!scene.Create() || !WriteScene(scene.Path(), false)) return false;

  SRecoveredSceneOrderSummary summary = {};
  if (RecoveredSceneOrder_ValidateFile(scene.Path().c_str(), &header,
                                       &summary) != 0 ||
      summary.namedDeclarations != 1 || summary.namedSlots != 1 ||
      summary.nodes != 5 || summary.branchOrders != 1 ||
      summary.objectReferences != 1 || summary.landPieces != 1 ||
      summary.shelterOrders != 1 || summary.emptyOrders != 1 ||
      summary.mapPrimaries != 2 || summary.mapEntries != 2 ||
      summary.mapCopySentinels != 1 || summary.maximumDepth != 3) {
    return false;
  }

  if (!WriteScene(scene.Path(), true) ||
      RecoveredSceneOrder_ValidateFile(scene.Path().c_str(), &header,
                                       nullptr) !=
          RECOVERED_SCENE_ORDER_INVALID_SCENE) {
    return false;
  }
  return RecoveredSceneOrder_ValidateFile(nullptr, &header, nullptr) ==
         RECOVERED_SCENE_ORDER_MISSING_SCENE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }
  if (!RunSyntheticContract()) {
    return Fail("synthetic scene-order contract failed");
  }
  if (argc == 1) return EXIT_SUCCESS;

  if (!RecoveredSoftwareGraph_Initialize(nullptr)) {
    return Fail("headless graph initialization failed");
  }
  GameEntry_UseRecoveredRuntime();
  if (!RecoveredLevelRuntime_Prepare(argv[1]) ||
      !RecoveredLevelAssets_Initialize() || !RecoveredTerrain_Initialize() ||
      !RecoveredSceneOrder_Initialize()) {
    std::fprintf(stderr, "scene-order issues=%u\n",
                 RecoveredSceneOrder_Issues());
    std::fprintf(stderr, "validation stage=%s\n",
                 RecoveredSceneOrder_LastValidationStage());
    RecoveredSceneOrder_Release();
    RecoveredTerrain_Release();
    ZAV_Deinit();
    return Fail("retail scene order did not initialize");
  }

  const SRecoveredSceneOrderSummary* summary =
      RecoveredSceneOrder_Summary();
  const bool decoded = summary != nullptr && summary->nodes > 0 &&
                       summary->landPieces > 0 && summary->mapPrimaries > 0;
  SRecoveredSceneOrderSummary result =
      summary != nullptr ? *summary : SRecoveredSceneOrderSummary{};

  RecoveredSceneOrder_Release();
  bool valid = decoded && !RecoveredSceneOrder_IsReady() &&
               RecoveredTerrain_IsReady() && RecoveredLevelAssets_IsReady() &&
               RecoveredLevelRuntime_IsPrepared() &&
               RecoveredSoftwareGraph_IsReady();
  RecoveredTerrain_Release();
  valid = valid && !RecoveredTerrain_IsReady() &&
          RecoveredLevelAssets_IsReady() &&
          RecoveredLevelRuntime_IsPrepared();
  ZAV_Deinit();
  if (!valid || RecoveredSoftwareGraph_IsReady() ||
      RecoveredLevelAssets_IsReady() ||
      RecoveredLevelRuntime_IsPrepared()) {
    return Fail("retail scene order did not release cleanly");
  }

  std::printf(
      "scene-order nodes=%d branches=%d objects=%d land=%d shelters=%d "
      "empty=%d primaries=%d entries=%d sentinels=%d depth=%d names=%d/%d\n",
      result.nodes, result.branchOrders, result.objectReferences,
      result.landPieces, result.shelterOrders, result.emptyOrders,
      result.mapPrimaries, result.mapEntries, result.mapCopySentinels,
      result.maximumDepth, result.namedDeclarations, result.namedSlots);
  return EXIT_SUCCESS;
}
