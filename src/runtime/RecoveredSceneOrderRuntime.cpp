#include "RecoveredSceneOrderRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#define LAST_H__VIEW
#include "game.h"
#include "filesys.h"

#include "RecoveredLevelAssets.h"
#include "RecoveredLevelRuntime.h"
#include "RecoveredTerrainRuntime.h"

namespace {

const int kMaximumOrderNodes = 1000000;
const int kMaximumOrderDepth = 1024;
const int kMaximumMapExtent = 4096;
const int kMaximumMapEntries = 1000000;
const int kMaximumMapPrimaries = 1000000;
const int kMaximumNameDeclarations = 100000;
const int kMaximumObjectBases = 100000;
const long kMaximumCoordinate = 1024L * 1024L;
std::string g_lastValidationStage = "not started";

struct SLandRect {
  long left;
  long top;
  long right;
  long bottom;
};

bool IsFinite(double value) { return std::isfinite(value) != 0; }

class CSceneOrderValidator {
 public:
  CSceneOrderValidator(CTaggedFile& file,
                       const SRecoveredSceneHeader& expected)
      : file_(file), expected_(expected), depth_(0), mapEntries_(0) {
    summary_ = SRecoveredSceneOrderSummary{};
    stage_ = "scene header";
  }

  bool Validate() {
    int namedBases = 0;
    int directBases = 0;
    int totalReferences = 0;
    int reductions = 0;
    double cellSize = 0.0;
    double heightRatio = 0.0;

    if (expected_.namedBases < 0 ||
        expected_.namedBases > kMaximumObjectBases ||
        expected_.directBases < 0 ||
        expected_.directBases > kMaximumObjectBases - expected_.namedBases ||
        expected_.totalReferences < 0 ||
        expected_.totalReferences > kMaximumOrderNodes ||
        expected_.reductions < 0 || expected_.reductions > 30 ||
        !IsFinite(expected_.cellSize) || expected_.cellSize <= 0.0 ||
        !IsFinite(expected_.heightRatio) || expected_.heightRatio <= 0.0 ||
        !Descend("SCEN", false, 0) || !Descend("SCEH", true, 0) ||
        !file_.ReadInt(namedBases) || !file_.ReadInt(directBases) ||
        !file_.ReadInt(totalReferences) || !file_.ReadDouble(cellSize) ||
        !file_.ReadDouble(heightRatio) || !file_.ReadInt(reductions) ||
        !FinishChunk() || namedBases != expected_.namedBases ||
        directBases != expected_.directBases ||
        totalReferences != expected_.totalReferences ||
        reductions != expected_.reductions || cellSize != expected_.cellSize ||
        heightRatio != expected_.heightRatio) {
      return false;
    }

    stage_ = "named object chunk";
    if (namedBases > 0 && !SkipChunk("OBJN", true, 0)) return false;
    stage_ = "direct object chunk";
    if (directBases > 0 && !SkipChunk("OBJD", true, 0)) return false;
    stage_ = "normal chunk";
    if (!SkipChunk("NRMS", true, 0)) return false;
    stage_ = "name declarations";
    if (!ReadNames()) return false;
    stage_ = "root order";
    if (!ReadOrder()) return false;

    for (const std::vector<unsigned char>& declaration : resolvedNames_) {
      if ((std::find)(declaration.begin(), declaration.end(), 0) !=
          declaration.end()) {
        stage_ = "unresolved name declaration";
        return false;
      }
    }
    stage_ = "scene end";
    return FinishChunk();
  }

  const SRecoveredSceneOrderSummary& Summary() const { return summary_; }
  const std::string& Stage() const { return stage_; }

 private:
  bool Descend(const char* tag, bool terminal, int version) {
    return file_.Descend(tag, terminal, version, false) == version;
  }

  bool FinishChunk() {
    return file_.BytesLeft() == 0 && file_.Ascend();
  }

  bool SkipChunk(const char* tag, bool terminal, int version) {
    return Descend(tag, terminal, version) && file_.Ascend();
  }

  bool ReadFiniteValues(int count) {
    for (int index = 0; index < count; ++index) {
      double value = 0.0;
      if (!file_.ReadDouble(value) || !IsFinite(value)) return false;
    }
    return true;
  }

  bool ReadNames() {
    if (!Descend("NAMS", false, 0) || !Descend("NAME", true, 0)) {
      return false;
    }

    int declarationCount = 0;
    if (!file_.ReadInt(declarationCount) || declarationCount < 0 ||
        declarationCount > kMaximumNameDeclarations) {
      return false;
    }
    resolvedNames_.resize(static_cast<std::size_t>(declarationCount));
    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(declarationCount));
    summary_.namedDeclarations = declarationCount;

    for (int index = 0; index < declarationCount; ++index) {
      int dimensions = 0;
      char name[40] = {};
      if (!file_.ReadInt(dimensions) || dimensions <= 0 ||
          dimensions > kMaximumOrderNodes ||
          file_.Read(name, sizeof(name)) != sizeof(name) ||
          std::memchr(name, '\0', sizeof(name)) == nullptr ||
          summary_.namedSlots > kMaximumOrderNodes - dimensions) {
        return false;
      }
      const std::string parsedName(name);
      if ((std::find)(names.begin(), names.end(), parsedName) != names.end()) {
        return false;
      }
      names.push_back(parsedName);
      resolvedNames_[static_cast<std::size_t>(index)].resize(
          static_cast<std::size_t>(dimensions), 0);
      summary_.namedSlots += dimensions;
    }
    return FinishChunk() && FinishChunk();
  }

  bool ReadNameDefinition(bool allowed) {
    int declaration = 0;
    int index = 0;
    if (!file_.ReadInt(declaration) || !file_.ReadInt(index)) return false;
    if (declaration == -1 || index == -1) {
      return declaration == -1 && index == -1;
    }
    if (!allowed || declaration < 0 ||
        declaration >= static_cast<int>(resolvedNames_.size())) {
      return false;
    }
    std::vector<unsigned char>& slots =
        resolvedNames_[static_cast<std::size_t>(declaration)];
    if (index < 0 || index >= static_cast<int>(slots.size()) ||
        slots[static_cast<std::size_t>(index)] != 0) {
      return false;
    }
    slots[static_cast<std::size_t>(index)] = 1;
    return true;
  }

  bool ReadOrder() {
    if (depth_ >= kMaximumOrderDepth ||
        summary_.nodes >= kMaximumOrderNodes) {
      return false;
    }
    ++depth_;
    summary_.maximumDepth = (std::max)(summary_.maximumDepth, depth_);
    const bool valid = ReadOrderAtDepth();
    --depth_;
    return valid;
  }

  bool ReadOrderAtDepth() {
    stage_ = "order chunk";
    if (!Descend("ORDR", false, 1) || !Descend("ORDH", true, 1)) {
      return false;
    }
    ++summary_.nodes;

    int type = -1;
    if (!file_.ReadInt(type)) return false;
    stage_ = "order header";
    switch (type) {
      case CViewOrdered::VOT_ORDER: {
        int normalIndex = 0;
        ++summary_.branchOrders;
        return ReadNameDefinition(false) && ReadFiniteValues(3) &&
               file_.ReadInt(normalIndex) && ReadFiniteValues(3) &&
               ReadFiniteValues(1) && FinishChunk() && ReadOrder() &&
               ReadOrder() && FinishChunk();
      }
      case CViewOrdered::VOT_OBJECT: {
        int base = 0;
        ++summary_.objectReferences;
        return ReadNameDefinition(true) && file_.ReadInt(base) && base >= 0 &&
               base < expected_.namedBases + expected_.directBases &&
               ReadFiniteValues(12) && FinishChunk() && FinishChunk();
      }
      case CViewOrdered::VOT_LANDPIECE:
        ++summary_.landPieces;
        return ReadNameDefinition(false) && FinishChunk() && ReadLand() &&
               FinishChunk();
      case CViewOrdered::VOT_SHELTERORDER: {
        int base = 0;
        ++summary_.shelterOrders;
        return ReadNameDefinition(false) && file_.ReadInt(base) && base >= 0 &&
               base < expected_.namedBases + expected_.directBases &&
               ReadFiniteValues(12) && FinishChunk() && ReadOrder() &&
               FinishChunk();
      }
      case CViewOrdered::VOT_EMPTY:
        ++summary_.emptyOrders;
        return ReadNameDefinition(false) && FinishChunk() && FinishChunk();
      default:
        return false;
    }
  }

  bool ReadLand() {
    stage_ = "land header";
    SLandRect rect = {};
    if (!Descend("LNDP", false, 0) || !Descend("LDPH", true, 0) ||
        !file_.ReadLong(rect.left) || !file_.ReadLong(rect.top) ||
        !file_.ReadLong(rect.right) || !file_.ReadLong(rect.bottom) ||
        rect.left >= rect.right || rect.top >= rect.bottom ||
        rect.left < -kMaximumCoordinate || rect.top < -kMaximumCoordinate ||
        rect.right > kMaximumCoordinate || rect.bottom > kMaximumCoordinate ||
        rect.right - rect.left > kMaximumMapExtent ||
        rect.bottom - rect.top > kMaximumMapExtent) {
      return false;
    }
    if (file_.BytesLeft() == 4) {
      bool bumpable = false;
      if (!file_.ReadBool(bumpable)) return false;
    } else if (file_.BytesLeft() != 0) {
      return false;
    }
    if (!FinishChunk()) return false;
    stage_ = "land map";
    if (!ReadMap2(rect)) return false;
    return FinishChunk();
  }

  bool ReadMap2(const SLandRect& rect) {
    stage_ = "MAP2 chunk";
    if (!Descend("MAP2", false, 0)) return false;
    std::vector<std::vector<int>> firstEntries(
        static_cast<std::size_t>(rect.right - rect.left));
    return ReadMap1(rect.left, rect.right, rect.top, rect.bottom, false,
                    rect, firstEntries) &&
           ReadMap1(rect.top, rect.bottom, rect.left, rect.right, true, rect,
                    firstEntries) &&
           FinishChunk();
  }

  bool ReadMap1(long primaryLow, long primaryHigh, long secondaryLow,
                long secondaryHigh, bool copy, const SLandRect& rect,
                std::vector<std::vector<int>>& firstEntries) {
    stage_ = copy ? "MAP1 copy chunk" : "MAP1 owner chunk";
    if (!Descend("MAP1", false, 0)) return false;
    for (long primary = primaryLow; primary < primaryHigh; ++primary) {
      stage_ = std::string(copy ? "MAP1 copy primary " :
                                  "MAP1 owner primary ") +
               std::to_string(primary) + " of [" +
               std::to_string(primaryLow) + "," +
               std::to_string(primaryHigh) + ") bytes=" +
               std::to_string(file_.BytesLeft());
      if (!Descend("M1PE", false, 0)) {
        stage_ += " missing M1PE";
        return false;
      }
      if (!Descend("M1PH", true, 0)) {
        stage_ += " missing M1PH";
        return false;
      }
      int entries = 0;
      if (!file_.ReadInt(entries)) {
        stage_ += " unreadable count";
        return false;
      }
      if (entries < 0 || entries > kMaximumMapEntries - mapEntries_) {
        stage_ += " invalid count=" + std::to_string(entries);
        return false;
      }
      if (!FinishChunk()) {
        stage_ += " unfinished M1PH count=" + std::to_string(entries);
        return false;
      }
      if (summary_.mapPrimaries >= kMaximumMapPrimaries) {
        stage_ += " too many primaries";
        return false;
      }
      ++summary_.mapPrimaries;
      summary_.mapEntries += entries;
      mapEntries_ += entries;

      long previous = secondaryLow - 1;
      for (int entry = 0; entry < entries; ++entry) {
        int secondary = 0;
        stage_ = copy ? "MAP1 copy secondary" : "MAP1 owner secondary";
        if (!Descend("M1SE", false, 0) ||
            !Descend("M1SH", true, 0) || !file_.ReadInt(secondary) ||
            secondary < secondaryLow || secondary >= secondaryHigh ||
            secondary <= previous || !FinishChunk()) {
          return false;
        }
        previous = secondary;

        if (copy) {
          const std::vector<int>& source = firstEntries[static_cast<std::size_t>(
              secondary - rect.left)];
          if (!(std::binary_search)(source.begin(), source.end(),
                                    static_cast<int>(primary))) {
            stage_ = "MAP2 copy reference";
            return false;
          }
        } else {
          if (!ReadOrder()) return false;
          firstEntries[static_cast<std::size_t>(primary - rect.left)]
              .push_back(secondary);
        }
        if (!FinishChunk()) return false;
      }
      if (!FinishChunk()) return false;
    }
    if (copy && file_.BytesLeft() != 0) {
      stage_ = "MAP1 copy sentinel";
      int sentinelEntries = -1;
      if (!Descend("M1PE", false, 0) ||
          !Descend("M1PH", true, 0) ||
          !file_.ReadInt(sentinelEntries) || sentinelEntries != 0 ||
          !FinishChunk() || !FinishChunk()) {
        return false;
      }
      ++summary_.mapCopySentinels;
    }
    if (file_.BytesLeft() != 0) {
      char tag[CTaggedFile::TF_MAXNAME] = {};
      bool terminal = false;
      int version = -1;
      if (file_.QueryChunk(tag, terminal, version, false)) {
        stage_ = std::string(copy ? "MAP1 copy trailing " :
                                    "MAP1 owner trailing ") +
                 tag + " version=" + std::to_string(version) +
                 " bytes=" + std::to_string(file_.BytesLeft());
      } else {
        stage_ = std::string(copy ? "MAP1 copy trailing data bytes=" :
                                    "MAP1 owner trailing data bytes=") +
                 std::to_string(file_.BytesLeft());
      }
      return false;
    }
    return file_.Ascend();
  }

  CTaggedFile& file_;
  const SRecoveredSceneHeader& expected_;
  SRecoveredSceneOrderSummary summary_;
  std::vector<std::vector<unsigned char>> resolvedNames_;
  int depth_;
  int mapEntries_;
  std::string stage_;
};

class CRecoveredSceneOrderNode : public CViewOrdered {
 public:
  explicit CRecoveredSceneOrderNode(VORDTYPE type)
      : type_(type), first_(nullptr), second_(nullptr), land_(nullptr) {}

  ~CRecoveredSceneOrderNode() override {
    delete first_;
    delete second_;
    delete land_;
  }

  void SetFirst(CRecoveredSceneOrderNode* node) { first_ = node; }
  void SetSecond(CRecoveredSceneOrderNode* node) { second_ = node; }
  void SetLand(CLandscapeRect* land) { land_ = land; }

  void Draw() override { ASSERTMSG(0, "decoded scene order is not drawable"); }
  void PromoteDynamic() override {
    ASSERTMSG(0, "decoded scene order has no dynamics");
  }
  bool Bump(SBumpDef0& def) override {
    (void)def;
    ASSERTMSG(0, "decoded scene order is not a bump owner");
    return false;
  }
  void RemoveDynamics() override {
    ASSERT(!m_dynamicList.First() && !m_afterList.First() &&
           !m_shotList.First());
    m_bDynamics = false;
  }
  VORDTYPE Type() const override { return type_; }
  void operator%=(TCCFMatrix3x4& dir) override {
    (void)dir;
    ASSERTMSG(0, "decoded scene order cannot be transformed");
  }
  void ApplyModelUsage() override {}
  void ComputeMaxHeight() override { m_fMaxHeight = 0.0; }
  double ComputeBSPError() override { return 1e10; }
  double ComputeBSPError(TCCFVector3& pt, TCCFVector3& normal,
                         double distance) override {
    (void)pt;
    (void)normal;
    (void)distance;
    return 1e10;
  }

 private:
  VORDTYPE type_;
  CRecoveredSceneOrderNode* first_;
  CRecoveredSceneOrderNode* second_;
  CLandscapeRect* land_;
};

class CSceneOrderDecoder;
CSceneOrderDecoder* g_decoder = nullptr;
CViewOrdered* ReadRecoveredOrder(CTaggedFile& file);

void Require(bool condition) {
  if (!condition) throw std::runtime_error("validated scene order diverged");
}

class CSceneOrderDecoder {
 public:
  CSceneOrderDecoder(const SRecoveredSceneHeader& header, byte* heights)
      : header_(header), heights_(heights) {}

  CRecoveredSceneOrderNode* Decode(const char* path) {
    CTaggedFile file(false);
    Require(file.Open(path, false));
    Require(file.Descend("SCEN", false, 0, false) == 0);
    Require(Skip(file, "SCEH", true, 0));
    if (header_.namedBases > 0) Require(Skip(file, "OBJN", true, 0));
    if (header_.directBases > 0) Require(Skip(file, "OBJD", true, 0));
    Require(Skip(file, "NRMS", true, 0));
    Require(Skip(file, "NAMS", false, 0));

    CSceneOrderDecoder* previousDecoder = g_decoder;
    CViewOrdered* previousTop = CViewOrdered::SetCurrentTop(nullptr);
    g_decoder = this;
    std::unique_ptr<CRecoveredSceneOrderNode> root;
    try {
      root.reset(ReadOrder(file));
    } catch (...) {
      g_decoder = previousDecoder;
      CViewOrdered::SetCurrentTop(previousTop);
      file.Close(false);
      throw;
    }
    g_decoder = previousDecoder;
    CViewOrdered::SetCurrentTop(previousTop);
    Require(file.BytesLeft() == 0 && file.Ascend());
    Require(file.Close(false));
    return root.release();
  }

  CRecoveredSceneOrderNode* ReadOrder(CTaggedFile& file) {
    Require(file.Descend("ORDR", false, 1, false) == 1);
    Require(file.Descend("ORDH", true, 1, false) == 1);

    int typeValue = -1;
    int nameDeclaration = -1;
    int nameIndex = -1;
    Require(file.ReadInt(typeValue) && file.ReadInt(nameDeclaration) &&
            file.ReadInt(nameIndex));
    const CViewOrdered::VORDTYPE type =
        static_cast<CViewOrdered::VORDTYPE>(typeValue);
    std::unique_ptr<CRecoveredSceneOrderNode> node(
        new CRecoveredSceneOrderNode(type));

    switch (type) {
      case CViewOrdered::VOT_ORDER: {
        CFVector3 point;
        CFVector3 normal;
        int normalIndex = 0;
        double distance = 0.0;
        Require(point.Read(file) && file.ReadInt(normalIndex) &&
                normal.Read(file) && file.ReadDouble(distance));
        Require(file.BytesLeft() == 0 && file.Ascend());
        node->SetFirst(ReadOrder(file));
        node->SetSecond(ReadOrder(file));
        break;
      }
      case CViewOrdered::VOT_OBJECT: {
        int base = 0;
        CFMatrix3x4 transform;
        Require(file.ReadInt(base) && transform.Read(file));
        Require(file.BytesLeft() == 0 && file.Ascend());
        break;
      }
      case CViewOrdered::VOT_LANDPIECE: {
        Require(file.BytesLeft() == 0 && file.Ascend());
        ReadLand(file, *node);
        break;
      }
      case CViewOrdered::VOT_SHELTERORDER: {
        int base = 0;
        CFMatrix3x4 transform;
        Require(file.ReadInt(base) && transform.Read(file));
        Require(file.BytesLeft() == 0 && file.Ascend());
        CViewOrdered* previousTop = CViewOrdered::SetCurrentTop(node.get());
        try {
          node->SetFirst(ReadOrder(file));
        } catch (...) {
          CViewOrdered::SetCurrentTop(previousTop);
          throw;
        }
        CViewOrdered::SetCurrentTop(previousTop);
        break;
      }
      case CViewOrdered::VOT_EMPTY:
        Require(file.BytesLeft() == 0 && file.Ascend());
        break;
      default:
        throw std::runtime_error("unexpected validated order type");
    }
    Require(file.BytesLeft() == 0 && file.Ascend());
    return node.release();
  }

 private:
  static bool Skip(CTaggedFile& file, const char* tag, bool terminal,
                   int version) {
    return file.Descend(tag, terminal, version, false) == version &&
           file.Ascend();
  }

  void ReadLand(CTaggedFile& file, CRecoveredSceneOrderNode& node) {
    Require(file.Descend("LNDP", false, 0, false) == 0);
    Require(file.Descend("LDPH", true, 0, false) == 0);
    CRect2 rect;
    Require(rect.Read(file));
    if (file.BytesLeft() == 4) {
      bool bumpable = false;
      Require(file.ReadBool(bumpable));
    }
    Require(file.BytesLeft() == 0 && file.Ascend());

    std::unique_ptr<CLandscapeRect> land(new CLandscapeRect());
    land->Read(file, rect, ReadRecoveredOrder);
    Require(file.IsOK());
    land->Setup(heights_, 9, header_.cellSize, header_.heightRatio);
    Require(file.BytesLeft() == 0 && file.Ascend());
    node.SetLand(land.release());
  }

  const SRecoveredSceneHeader& header_;
  byte* heights_;
};

CViewOrdered* ReadRecoveredOrder(CTaggedFile& file) {
  Require(g_decoder != nullptr);
  return g_decoder->ReadOrder(file);
}

CRecoveredSceneOrderNode* g_root = nullptr;
SRecoveredSceneOrderSummary g_summary = {};
unsigned int g_issues = 0;

int Fail(unsigned int issue) {
  g_issues = issue;
  RecoveredSceneOrder_Release();
  return FALSE;
}

}  // namespace

unsigned int RecoveredSceneOrder_ValidateFile(
    const char* path, const SRecoveredSceneHeader* expectedHeader,
    SRecoveredSceneOrderSummary* summary) {
  g_lastValidationStage = "validation start";
  if (summary != nullptr) *summary = SRecoveredSceneOrderSummary{};
  if (path == nullptr || *path == '\0') {
    return RECOVERED_SCENE_ORDER_MISSING_SCENE;
  }
  if (expectedHeader == nullptr) {
    return RECOVERED_SCENE_ORDER_INVALID_SCENE;
  }

  try {
    CTaggedFile file(false);
    if (!file.Open(path, false)) return RECOVERED_SCENE_ORDER_MISSING_SCENE;
    CSceneOrderValidator validator(file, *expectedHeader);
    const bool valid = validator.Validate();
    const bool closed = file.Close(false);
    g_lastValidationStage = validator.Stage();
    if (!valid || !closed) return RECOVERED_SCENE_ORDER_INVALID_SCENE;
    if (summary != nullptr) *summary = validator.Summary();
    return 0;
  } catch (const std::bad_alloc&) {
    g_lastValidationStage = "allocation failure";
    return RECOVERED_SCENE_ORDER_ALLOCATION_FAILURE;
  } catch (...) {
    g_lastValidationStage = "validation exception";
    return RECOVERED_SCENE_ORDER_INVALID_SCENE;
  }
}

int RecoveredSceneOrder_Initialize() {
  RecoveredSceneOrder_Release();
  g_issues = 0;
  if (!RecoveredLevelAssets_IsReady() || !RecoveredTerrain_IsReady()) {
    return Fail(RECOVERED_SCENE_ORDER_MISSING_DEPENDENCY);
  }

  const SRecoveredSceneHeader* header = RecoveredLevelAssets_SceneHeader();
  const char* scene = RecoveredLevelRuntime_SceneFile();
  _CViewTerrain* terrain = RecoveredTerrain_Get();
  if (header == nullptr || scene == nullptr || terrain == nullptr ||
      terrain->GetHeightMap() == nullptr) {
    return Fail(RECOVERED_SCENE_ORDER_MISSING_DEPENDENCY);
  }

  const unsigned int validation =
      RecoveredSceneOrder_ValidateFile(scene, header, &g_summary);
  if (validation != 0) return Fail(validation);

  try {
    CSceneOrderDecoder decoder(
        *header, const_cast<byte*>(terrain->GetHeightMap()));
    g_root = decoder.Decode(scene);
  } catch (const std::bad_alloc&) {
    return Fail(RECOVERED_SCENE_ORDER_ALLOCATION_FAILURE);
  } catch (...) {
    return Fail(RECOVERED_SCENE_ORDER_DECODE_FAILURE);
  }
  if (g_root == nullptr) return Fail(RECOVERED_SCENE_ORDER_DECODE_FAILURE);
  return TRUE;
}

void RecoveredSceneOrder_Release() {
  delete g_root;
  g_root = nullptr;
  g_summary = SRecoveredSceneOrderSummary{};
}

bool RecoveredSceneOrder_IsReady() { return g_root != nullptr; }

unsigned int RecoveredSceneOrder_Issues() { return g_issues; }

const SRecoveredSceneOrderSummary* RecoveredSceneOrder_Summary() {
  return g_root != nullptr ? &g_summary : nullptr;
}

const char* RecoveredSceneOrder_LastValidationStage() {
  return g_lastValidationStage.c_str();
}
