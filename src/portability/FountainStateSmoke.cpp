#include <cstdio>
#include <cstdlib>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LAST_H__VIEW
#include "game.h"
#include "obase/fountain/fountain.h"
#include "storage/h/savefile.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-fountain-state-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool ExerciseFreeList() {
  Fountain::m_freeList = nullptr;
  Fountain::m_branch[7].m_prev = &Fountain::m_branch[6];
  Fountain::m_branch[7].deleteCommand = &Fountain::m_branch[8];
  Fountain::createFreeList();

  FountBranch* branch = Fountain::m_freeList;
  for (int index = 0; index < Fountain::MAX_BRANCH; ++index) {
    if (branch != &Fountain::m_branch[index] || branch->m_prev != nullptr ||
        branch->deleteCommand != nullptr) {
      return false;
    }
    branch = branch->m_next;
  }
  return branch == nullptr;
}

bool ExerciseBranchSave(const char* path) {
  FountBranch source;
  source.m_phase = 1.25;
  source.xT = -2.5;
  source.zT = 3.75;
  source.yT = 4.5;
  source.rA = -0.125;
  source.rB = 0.25;
  source.rC = 0.75;
  source.m_timeOfLife = 8.0;
  source.m_color = 0xA1B2C3D4UL;

  PIN_SaveFile output;
  if (!output.OpenWrite(const_cast<char*>(path)) || !source.dump(output)) {
    output.Close();
    return false;
  }
  output.Close();

  FountBranch next_sentinel;
  FountBranch previous_sentinel;
  FountBranch delete_sentinel;
  FountBranch restored;
  restored.m_next = &next_sentinel;
  restored.m_prev = &previous_sentinel;
  restored.deleteCommand = &delete_sentinel;

  PIN_SaveFile input;
  if (!input.OpenRead(const_cast<char*>(path)) || !restored.load(input)) {
    input.Close();
    return false;
  }
  const bool at_end = input.GetCurrentData() == nullptr;
  input.Close();

  return restored.m_phase == source.m_phase && restored.xT == source.xT &&
         restored.zT == source.zT && restored.yT == source.yT &&
         restored.rA == source.rA && restored.rB == source.rB &&
         restored.rC == source.rC &&
         restored.m_timeOfLife == source.m_timeOfLife &&
         restored.m_color == source.m_color &&
         restored.m_next == &next_sentinel &&
         restored.m_prev == &previous_sentinel &&
         restored.deleteCommand == &delete_sentinel && at_end;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4, "fountain state requires Win32");
  static_assert(sizeof(unsigned long) == 4,
                "fountain color serialization requires 32-bit long");
  static_assert(sizeof(FountBranchData) == 72,
                "fountain branch save layout changed");
  if (argc != 2) {
    return Fail("expected a temporary save path");
  }

  std::remove(argv[1]);
  if (!ExerciseFreeList()) {
    return Fail("free-list reconstruction diverged");
  }
  if (!ExerciseBranchSave(argv[1])) {
    return Fail("branch save round-trip diverged");
  }

  std::remove(argv[1]);
  std::cout << "legacy-fountain-state-smoke: OK\n";
  return EXIT_SUCCESS;
}
