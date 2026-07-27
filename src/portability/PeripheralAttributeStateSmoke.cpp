#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "obase/corpse/CorpseAttributeState.h"
#include "obase/farter/FarterAttributeState.h"
#include "obase/lamp/LampAttributeState.h"

namespace {

int Fail(const char* message) {
  std::fprintf(stderr, "peripheral-attribute-state-smoke: %s\n", message);
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  AttributeFarter farter;
  if (farter.m_wav != nullptr || farter.m_ctsndID != ct_NULLID ||
      std::strcmp(farter.m_soundName, "") != 0) {
    return Fail("Farter pre-update state is not deterministic");
  }

  AttributeCorpse corpse;
  if (corpse.m_cacheSkin != nullptr || !corpse.m_skinID.isNUL() ||
      !corpse.m_smokerAttrID.isNUL() || !corpse.m_fireAttrID.isNUL() ||
      corpse.m_smokerTableID != ct_NULLID ||
      std::strcmp(corpse.m_skinName, "sk.corpse.default") != 0 ||
      std::strcmp(corpse.m_smokerTable, "DynSmoker") != 0 ||
      corpse.m_minLifeTime != 30.0 || corpse.m_corpseOffsetY != -1.0) {
    return Fail("Corpse pre-update state is not deterministic");
  }

  AttributeLamp lamp;
  if (lamp.m_coronaHText != nullptr || lamp.m_coronaColor != 0 ||
      lamp.m_coronaFadeCoeff != 0.0 || lamp.m_lightColor != 7 ||
      lamp.m_coronaRGB != 0xFFFFFF || lamp.m_onLand != 0 ||
      lamp.searchItem("m_onLand") != nullptr ||
      lamp.searchItem("m_onLand  ") == nullptr) {
    return Fail("Lamp retail serializer identity was not preserved");
  }
  lamp.set_int("m_onLand", 1);
  if (lamp.m_onLand != 0) {
    return Fail("Lamp accepted the retail script spelling unexpectedly");
  }
  lamp.set_int("m_onLand  ", 1);
  if (lamp.m_onLand != 1) {
    return Fail("Lamp rejected the executable attribute spelling");
  }

  std::printf("peripheral attribute owners=farter,lamp,corpse "
              "caches=pre-update lamp_on_land=retail-two-spaces\n");
  return EXIT_SUCCESS;
}
