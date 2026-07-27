#include <cstdio>
#include <cstdlib>

#include "RecoveredSkinResourceCatalog.h"
#include "SKIN.H"
#include "SkinResourceState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "legacy-skin-resource-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 1 && argc != 2) {
    return Fail("expected an optional retail level directory");
  }
  SkinResourceState_Link();
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
  std::printf("skin-resource-catalog models=%d/%d sprites=%d/%d "
              "entries=%d source_fingerprint=%llu fingerprint=%llu\n",
              catalog.modelCount, catalog.modelCapacity,
              catalog.spriteCount, catalog.spriteCapacity,
              catalog.entryCount, catalog.sourceFingerprint,
              catalog.fingerprint);
  return EXIT_SUCCESS;
}
