#include <cstdlib>
#include <cstring>
#include <iostream>

#include "kernel/h/s_evdata.h"

namespace {

constexpr int kEventLabel = 31415;
constexpr int kEventVersion = 2;
constexpr int kExpectedInt = -2718;
constexpr double kExpectedDouble = 27.05;
constexpr char kExpectedText[] = "kernel-state";

int Fail(const char* message) {
  std::cerr << "legacy-kernel-state-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

}  // namespace

int main() {
  static_assert(sizeof(long) == 4, "kernel state requires a Win32 long");
  static_assert(sizeof(KR_ObjectID) == 8,
                "object IDs require two 32-bit fields");

  const s_ELN labels[] = {
      s_ELN(kEventLabel, "SMOKE_EVENT"),
      s_ELN(7, "SECOND_EVENT"),
      s_ELN(),
  };
  s_ELNTable label_table("kernel-state-smoke", labels);
  if (s_FindLabelName(kEventLabel) == nullptr ||
      std::strcmp(s_FindLabelName(kEventLabel), "SMOKE_EVENT") != 0 ||
      std::strcmp(s_FindLabelName(-1), "?") != 0 ||
      std::strcmp(ed_Tag2Msg(EDI_OBJECTID), "KR_ObjectID") != 0) {
    return Fail("event label registry lookup diverged");
  }

  const KR_ObjectID expected_id(0x12345678L, 9);
  s_EventData written;
  written.open(EDO_WRITE)
      .descend(kEventLabel, kEventVersion)
      .putInt(kExpectedInt)
      .putDouble(kExpectedDouble)
      .putStr(kExpectedText)
      .putObjectID(expected_id)
      .ascend();
  written.close();

  s_EventData copied;
  copied.getCopy(written);
  int actual_int = 0;
  double actual_double = 0.0;
  char actual_text[32] = {};
  KR_ObjectID actual_id = KR_ObjectID::NUL();
  copied.open(EDO_READ)
      .descend(kEventLabel, kEventVersion)
      .getInt(actual_int)
      .getDouble(actual_double)
      .getStr(actual_text, static_cast<int>(sizeof(actual_text)))
      .getObjectID(actual_id)
      .ascend();
  copied.close();

  if (actual_int != kExpectedInt || actual_double != kExpectedDouble ||
      std::strcmp(actual_text, kExpectedText) != 0 ||
      actual_id != expected_id || actual_id.getCachePos() != 9) {
    return Fail("event payload round-trip diverged");
  }

  std::cout << "legacy-kernel-state-smoke: OK\n";
  return EXIT_SUCCESS;
}
