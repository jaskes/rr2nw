#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "ActorCadence.h"
#include "mathlib.h"

namespace {

bool NearlyEqual(double left, double right) {
  return std::fabs(left - right) < 1.0e-12;
}

int Fail(const char* message) {
  std::cerr << "legacy-math-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  static_assert(sizeof(void*) == 4, "M1 requires a Win32 process");
  static_assert(sizeof(long) == 4, "legacy disk fields require a 32-bit long");
  static_assert(sizeof(byte) == 1, "legacy byte width changed");
  static_assert(sizeof(word) == 2, "legacy word width changed");
  static_assert(sizeof(dword) == 4, "legacy dword width changed");
  static_assert(sizeof(SVector2) == 8, "legacy packed vector layout changed");
  static_assert(sizeof(SFVector3) == 24, "legacy packed vector layout changed");
  static_assert(sizeof(SFMatrix3x4) == 96, "legacy packed matrix layout changed");

  const CVector2 first_vector(7, -2);
  const CVector2 second_vector(5, 9);
  const CVector2 vector_sum = first_vector + second_vector;
  if (vector_sum.x != 12 || vector_sum.y != 7) {
    return Fail("integer vector arithmetic diverged");
  }

  CRandomGenerator random;
  const double first_random = random();
  if (!NearlyEqual(first_random, 346.0 / 32768.0)) {
    return Fail("first legacy PRNG sample diverged");
  }

  CFMatrix3x4 identity;
  identity.LoadIdentity();
  const CFVector3 input(1.25, -2.5, 8.0);
  const CFVector3 output = identity * input;
  if (!NearlyEqual(output.x, input.x) || !NearlyEqual(output.y, input.y) ||
      !NearlyEqual(output.z, input.z)) {
    return Fail("identity matrix transform diverged");
  }

  if (!NearlyEqual(rr2nw::ActorCadenceScale(0.0, 300.0), 0.2) ||
      !NearlyEqual(rr2nw::ActorCadenceScale(300.0, 300.0), 1.2) ||
      !NearlyEqual(rr2nw::ActorCadenceScale(540.0, 300.0), 2.0) ||
      !NearlyEqual(rr2nw::ActorCadenceScale(3000.0, 300.0), 2.0) ||
      !NearlyEqual(rr2nw::ActorCadenceScale(
                       std::numeric_limits<double>::quiet_NaN(), 300.0),
                   1.0) ||
      !NearlyEqual(rr2nw::ActorCadenceScale(100.0, 0.0), 1.0)) {
    return Fail("actor near/far cadence bounds diverged");
  }

  if (!NearlyEqual(rr2nw::ActorPresentationRatio(10.05, 10.0, 0.1), 0.5) ||
      !NearlyEqual(rr2nw::ActorPresentationRatio(10.5, 10.0, 0.1), 1.0) ||
      !NearlyEqual(rr2nw::ActorPresentationRatio(9.9, 10.0, 0.1), 0.0) ||
      !NearlyEqual(rr2nw::ActorPresentationRatio(
                       std::numeric_limits<double>::quiet_NaN(), 10.0, 0.1),
                   0.0) ||
      !NearlyEqual(rr2nw::ActorPresentationRatio(10.1, 10.0, 0.2, 0.2),
                   0.0)) {
    return Fail("actor presentation phase bounds diverged");
  }

  if (!NearlyEqual(
          rr2nw::ActorPresentationPreviousWeight(10.0, 10.0, 0.1), 1.0) ||
      !NearlyEqual(
          rr2nw::ActorPresentationPreviousWeight(10.05, 10.0, 0.1), 0.5) ||
      !NearlyEqual(
          rr2nw::ActorPresentationPreviousWeight(10.1, 10.0, 0.1), 0.0) ||
      !NearlyEqual(
          rr2nw::ActorPresentationPreviousWeight(9.9, 10.0, 0.1), 1.0) ||
      !NearlyEqual(rr2nw::ActorPresentationPreviousWeight(
                       std::numeric_limits<double>::quiet_NaN(), 10.0, 0.1),
                   0.0) ||
      !NearlyEqual(
          rr2nw::ActorPresentationPreviousWeight(10.1, 10.0, 0.2, 0.2),
          0.0)) {
    return Fail("actor presentation interpolation bounds diverged");
  }

  // A newly accepted simulation sample begins exactly where the previous
  // sample finished. Extrapolation would put the first value at 2.0 here.
  const double oldSampleEnd =
      1.0 + (0.0 - 1.0) *
          rr2nw::ActorPresentationPreviousWeight(10.1, 10.0, 0.1);
  const double newSampleBegin =
      2.0 + (1.0 - 2.0) *
          rr2nw::ActorPresentationPreviousWeight(10.1, 10.1, 0.1);
  if (!NearlyEqual(oldSampleEnd, 1.0) ||
      !NearlyEqual(newSampleBegin, oldSampleEnd)) {
    return Fail("actor presentation sample boundary is discontinuous");
  }

  std::cout << "legacy-math-smoke: OK\n";
  return EXIT_SUCCESS;
}
