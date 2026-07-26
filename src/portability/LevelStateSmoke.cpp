#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "h/olevel.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-level-state-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool NearlyEqual(double left, double right) {
  return std::fabs(left - right) < 1e-12;
}

bool DefaultsMatch() {
  return NearlyEqual(g_levelAttr.m_minDesire, -1.0) &&
         NearlyEqual(g_levelAttr.m_maxDesire, 1.0) &&
         NearlyEqual(g_levelAttr.m_addDesire, 0.03) &&
         NearlyEqual(g_levelAttr.m_subDesire, 0.55) &&
         NearlyEqual(g_levelAttr.m_desireAttackThreshold, 0.5) &&
         NearlyEqual(g_levelAttr.m_desireForgetThreshold, 0.0) &&
         NearlyEqual(g_levelAttr.m_levelAddRatingScale, 0.5) &&
         NearlyEqual(g_levelAttr.m_levelSubRatingScale, 1.0) &&
         NearlyEqual(g_levelAttr.m_minRating, -2.4) &&
         NearlyEqual(g_levelAttr.m_maxRating, 2.4) &&
         NearlyEqual(g_levelAttr.m_renegatThreshold, -0.4) &&
         NearlyEqual(g_levelAttr.m_unitDamageScale, 0.8) &&
         NearlyEqual(g_levelAttr.m_unitDamageScaleFromPlayer, 0.6) &&
         g_levelAttr.m_minSecBulletCnt == 10 &&
         g_levelAttr.m_maxSecBulletCnt == 30 &&
         NearlyEqual(g_levelAttr.m_minDamage, 0.5) &&
         NearlyEqual(g_levelAttr.m_fromFriendDamageScale, 0.2) &&
         NearlyEqual(g_levelAttr.m_666Time, 6.0) &&
         g_levelAttr.msInvX == 0 && g_levelAttr.msInvY == 0 &&
         NearlyEqual(g_levelAttr.msSensX, 0.5) &&
         NearlyEqual(g_levelAttr.msSensY, 0.5) && g_levelAttr.msUse == 0 &&
         g_levelAttr.jsInvX == 0 && g_levelAttr.jsInvY == 0 &&
         NearlyEqual(g_levelAttr.jsSensX, 0.5) &&
         NearlyEqual(g_levelAttr.jsSensY, 0.5) && g_levelAttr.jsUse == 0 &&
         g_levelAttr.m_reloadLevel == 0 &&
         NearlyEqual(g_levelAttr.keySens, 1.0);
}

bool AttributeTableMatches() {
  g_levelAttr.set_double("m_levelSubRatingScale", 1.75);
  g_levelAttr.set_int("m_reloadLevel", 6);
  const bool matches =
      NearlyEqual(g_levelAttr.m_levelSubRatingScale, 1.75) &&
      NearlyEqual(g_levelAttr.get_double("m_levelSubRatingScale"), 1.75) &&
      g_levelAttr.m_reloadLevel == 6 &&
      g_levelAttr.get_int("m_reloadLevel") == 6 &&
      g_levelAttr.searchItem("missing.attribute") == nullptr;
  g_levelAttr.set_double("m_levelSubRatingScale", 1.0);
  g_levelAttr.set_int("m_reloadLevel", 0);
  return matches;
}

bool SelectionStateMatches() {
  if (ol_Level::m_levelNumber != 0 || ol_Level::m_levelToLoad != 0 ||
      ol_Level::m_saveGameStatus != 0 || ol_Level::m_saveGameName[0] != 0) {
    return false;
  }
  ol_Level::m_levelNumber = 3;
  ol_Level::m_levelToLoad = 7;
  ol_Level::m_saveGameStatus = ol_Level::MST_LOAD;
  std::strcpy(ol_Level::m_saveGameName, "Save7.sav");
  return ol_Level::m_levelNumber == 3 && ol_Level::m_levelToLoad == 7 &&
         ol_Level::m_saveGameStatus == ol_Level::MST_LOAD &&
         std::strcmp(ol_Level::m_saveGameName, "Save7.sav") == 0;
}

}  // namespace

int main() {
  static_assert(sizeof(void*) == 4, "level state contract requires Win32");
  if (!DefaultsMatch()) {
    return Fail("AttributeLevel defaults diverged");
  }
  if (!AttributeTableMatches()) {
    return Fail("AttributeLevel name binding diverged");
  }
  if (!SelectionStateMatches()) {
    return Fail("level selection state ownership diverged");
  }

  std::cout << "legacy-level-state-smoke: OK\n";
  return EXIT_SUCCESS;
}
