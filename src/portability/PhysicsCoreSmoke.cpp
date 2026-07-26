#include <cmath>
#include <cstdlib>
#include <iostream>

#define LAST_H__VIEW
#include "game.h"
#include "kernel/h/krtypes.h"
#include "storage/h/classtab.h"
#include "h/phisics.h"

extern int g_godMode;

namespace {

constexpr double kTolerance = 1e-9;

bool Near(double left, double right) {
  return std::fabs(left - right) < kTolerance;
}

int Fail(const char* message) {
  std::cerr << "legacy-physics-core-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

SBumpDef MakeBump(const CFVector3& position,
                  const CFVector3& velocity,
                  double radius,
                  double mass,
                  double time) {
  SBumpDef bump;
  bump.start = position;
  bump.vel = velocity;
  bump.vel1 = velocity;
  bump.fTime = time;
  bump.fRadius = radius;
  bump.fMass = mass;
  bump.nBumpFlags = BF_NONE;
  bump.pBonus = nullptr;
  bump.pBumpRef = nullptr;
  return bump;
}

}  // namespace

int main() {
  static_assert(sizeof(CFVector3) == 24,
                "physics vector ABI changed");
#ifdef _DEBUG
  static_assert(sizeof(SBumpDef) == 128,
                "debug dynamic collision ABI changed");
#else
  static_assert(sizeof(SBumpDef) == 120,
                "release dynamic collision ABI changed");
#endif
  if (g_godMode != 0) {
    return Fail("god-mode state default changed");
  }
  g_godMode = 1;
  if (g_godMode != 1) {
    return Fail("god-mode state is not writable");
  }
  g_godMode = 0;

  if (!Near(interpolateAngle(0.0, 1.0, 0.25, 2.0), 0.5) ||
      !Near(interpolateAngle(0.0, 0.1, 1.0, 1.0), 0.1) ||
      !Near(interpolateAngle(3.0, -3.0, 0.2, 1.0), -3.083185307179586)) {
    return Fail("angle interpolation diverged");
  }
  if (!Near(interpolateAngleInert(0.0, 2.0, 10.0, 0.25, 1.0), 0.25)) {
    return Fail("inertial angle clamp diverged");
  }

  CFMatrix3x4 identity;
  identity.LoadIdentity();
  if (!Near(calcLocalAngle(identity, 0.0), -M_PI / 2)) {
    return Fail("local angle conversion diverged");
  }

  if (!Near(checkCollision(CFVector3(5.0, 0.0, 0.0),
                           CFVector3(1.0, 0.0, 0.0), 1.0),
            4.0) ||
      checkCollision(CFVector3(5.0, 0.0, 0.0),
                     CFVector3(-1.0, 0.0, 0.0), 1.0) >= 0.0 ||
      !Near(checkCollision(CFVector3(0.5, 0.0, 0.0),
                           CFVector3(1.0, 0.0, 0.0), 1.0),
            0.0)) {
    return Fail("sphere collision timing diverged");
  }

  double a0 = 0;
  double a1 = 0;
  double a2 = 0;
  double a3 = 0;
  calcCoef(1.0, 2.0, 5.0, 8.0, 11.0, a0, a1, a2, a3);
  if (!Near(a0, 2.0) || !Near(a1, 3.0) || !Near(a2, 0.0) ||
      !Near(a3, 0.0)) {
    return Fail("cubic coefficients diverged");
  }

  SBumpDef left = MakeBump(CFVector3(-2.0, 0.0, 0.0),
                           CFVector3(1.0, 0.0, 0.0), 0.5, 1.0, 5.0);
  SBumpDef right = MakeBump(CFVector3(2.0, 0.0, 0.0),
                            CFVector3(-1.0, 0.0, 0.0), 0.5, 1.0, 5.0);
  if (!Bump(left, right) || !Near(left.fTime, 1.5) ||
      !Near(right.fTime, 1.5) || left.nBumpFlags != BF_BUMPDYNAMIC ||
      right.nBumpFlags != BF_BUMPDYNAMIC || !Near(left.vel1.x, -1.0) ||
      !Near(right.vel1.x, 1.0)) {
    return Fail("head-on dynamic collision diverged");
  }

  SBumpDef departing = MakeBump(CFVector3(-2.0, 0.0, 0.0),
                                CFVector3(-1.0, 0.0, 0.0), 0.5, 1.0, 5.0);
  SBumpDef stationary = MakeBump(CFVector3(2.0, 0.0, 0.0),
                                 CFVector3(0.0, 0.0, 0.0), 0.5, 1.0, 5.0);
  if (Bump(departing, stationary)) {
    return Fail("departing bodies reported a collision");
  }

  std::cout << "legacy-physics-core-smoke: OK\n";
  return EXIT_SUCCESS;
}
