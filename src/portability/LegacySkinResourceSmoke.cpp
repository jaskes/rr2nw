#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "RecoveredSkinResourceCatalog.h"
#include "SKIN.H"
#include "SkinResourceState.h"
#include "../../nw/OUTPUT/animconst.h"
#include "enum/spaceEnum.h"
#include "message/peopmsg.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "legacy-skin-resource-smoke: %s\n", message);
  return EXIT_FAILURE;
}

bool Near(double left, double right) {
  return std::fabs(left - right) <= 1.0e-12;
}

void WriteAxis(s_EventData& data, double x, double y, double z) {
  data.descend(VECTOR3D_F, 0)
      .putDouble(x)
      .putDouble(y)
      .putDouble(z)
      .ascend();
}

bool DecodeRock(AnimateInfo* info, int type) {
  KR_Event event;
  event.label = pe_EV_SETANIM;
  event.data.open(EDO_WRITE).putInt(type);
  WriteAxis(event.data, 1.0, 2.0, 3.0);
  event.data.putDouble(2.0)
      .putDouble(1.0)
      .putDouble(0.0)
      .putDouble(-0.5)
      .putDouble(0.75)
      .putDouble(0.1)
      .close();
  event.data.open(EDO_READ);
  const bool decoded = info->setAnim(event, 7) != 0;
  event.data.close();
  return decoded;
}

bool ProbeMayAnimationAbi() {
  AnimateInfo rock;
  rock.startInitialize(nullptr);
  if (!DecodeRock(&rock, anim_ROCKOX) || rock.m_acellCnt != 1) return false;
  const AnimateCell& cell = rock.cur(0);
  const double pi = 3.14159265358979323846;
  if (cell.m_type != anim_ROCKOX || cell.m_animNum != 7 ||
      !Near(cell.m_axis.x, 1.0) || !Near(cell.m_axis.y, 2.0) ||
      !Near(cell.m_axis.z, 3.0) || !Near(cell.A, 2.0) ||
      !Near(cell.w, 1.0) || !Near(cell.F, 0.0) ||
      !Near(cell.split, -0.5) || !Near(cell.asplit, 0.75) ||
      !Near(cell.offset, 0.1) || !Near(cell.angle(0.0), 0.1) ||
      !Near(cell.angle(pi * 0.5), 0.75) ||
      !Near(cell.angle(pi * 1.5), -0.5)) {
    return false;
  }

  AnimateInfo out;
  out.startInitialize(nullptr);
  KR_Event event;
  event.label = pe_EV_SETANIM;
  event.data.open(EDO_WRITE).putInt(anim_ROTATEOYOut);
  WriteAxis(event.data, -1.99, 0.0, -1.85);
  event.data.putDouble(0.25).putDouble(1.5).close();
  event.data.open(EDO_READ);
  const bool decoded = out.setAnim(event, 3) != 0;
  event.data.close();
  if (!decoded || out.m_acellCnt != 1 ||
      out.cur(0).m_type != anim_ROTATEOYOut ||
      !Near(out.cur(0).angle(0.0), 1.75) ||
      !Near(out.cur(0).angle(1234.0), 1.75)) {
    return false;
  }

  AnimateInfo unsupported;
  unsupported.startInitialize(nullptr);
  event.label = pe_EV_SETANIM;
  event.data.open(EDO_WRITE).putInt(anim_ROTATE).close();
  event.data.open(EDO_READ);
  const bool rejected = unsupported.setAnim(event, 0) == 0;
  event.data.close();
  return rejected && unsupported.m_acellCnt == 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }
  SkinResourceState_Link();
  if (!ProbeMayAnimationAbi()) {
    return Fail("May animation event ABI or clamp math regressed");
  }
  Skin skin;
  if (skin.m_loaded || skin.m_array != nullptr ||
      skin.m_array0 != nullptr || skin.m_animProg != nullptr ||
      skin.m_aniCnt != 0 || skin.m_aniCnt0 != 0 ||
      skin.m_animProgCnt != 0 || skin.m_animProgSP != 0 ||
      skin.queryInterface(ISkinIID) == nullptr) {
    return Fail("default recovered resource state is not deterministic");
  }
  skin.createAniSets(2, 3, 4);
  if (skin.m_array != nullptr || skin.m_array0 != nullptr ||
      skin.m_animProg != nullptr || skin.m_aniCnt != 0 ||
      skin.m_aniCnt0 != 0 || skin.m_animProgCnt != 0 ||
      skin.m_animProgSP != 0) {
    return Fail("unloaded Skin admitted an animation allocation");
  }
  skin.removeAniSets();
  skin.removeAniSets();
  if (skin.m_array != nullptr || skin.m_array0 != nullptr ||
      skin.m_animProg != nullptr || skin.m_animProgCnt != 0 ||
      skin.m_animProgSP != 0) {
    return Fail("animation cleanup is not idempotent");
  }
  if (argc == 1) return EXIT_SUCCESS;

  SRecoveredSkinResourceCatalog catalog = {};
  SRecoveredSkinResourceCatalogResult result = {};
  if (!RecoveredSkinResourceCatalog_Load(argv[1], &catalog, &result) ||
      catalog.modelCount > catalog.modelCapacity ||
      catalog.spriteCount > catalog.spriteCapacity ||
      catalog.fingerprint == 0 || catalog.sourceFingerprint == 0) {
    std::fprintf(stderr, "skin catalog issue=%u error=%s\n", result.issues,
                 result.error);
    return Fail("retail Skin catalog did not validate");
  }
  std::string animationProgram;
  int animationEntryCalls = -1;
  unsigned long long animationFingerprint = 0;
  if (!RecoveredSkinResourceCatalog_BuildAnimationProgram(
          argv[1], &animationProgram, &animationEntryCalls,
          &animationFingerprint, &result) || animationProgram.empty() ||
      animationEntryCalls < 0 || animationFingerprint == 0) {
    std::fprintf(stderr, "skin animation issue=%u error=%s\n", result.issues,
                 result.error);
    return Fail("retail Skin animation program did not validate");
  }
  std::printf("skin-resource-catalog models=%d/%d sprites=%d/%d "
              "entries=%d source_fingerprint=%llu fingerprint=%llu "
              "animation_entries=%d animation_fingerprint=%llu\n",
              catalog.modelCount, catalog.modelCapacity,
              catalog.spriteCount, catalog.spriteCapacity,
              catalog.entryCount, catalog.sourceFingerprint,
              catalog.fingerprint, animationEntryCalls,
              animationFingerprint);
  return EXIT_SUCCESS;
}
