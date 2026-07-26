#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "kernel/h/context.h"
#include "mproj/h/mproj.h"
#include "storage/h/savefile.h"
#include "defs.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-mproj-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

bool ExerciseProjectDataFile(const char* path) {
  mp_Project source;
  source.m_treeNode = 17;
  source.m_permanent = 1;

  PIN_SaveFile output;
  if (!output.OpenWrite(const_cast<char*>(path)) || !source.dump(output)) {
    output.Close();
    return false;
  }
  output.Close();

  mp_Project restored;
  PIN_SaveFile input;
  if (!input.OpenRead(const_cast<char*>(path)) || !restored.load(input) ||
      input.GetCurrentData() != nullptr) {
    input.Close();
    return false;
  }
  input.Close();

  return restored.m_treeNode == source.m_treeNode &&
         restored.m_permanent == source.m_permanent;
}

bool ExerciseProjectTree() {
  SimulationContext context(16, 16);
  ct_Arena storage;
  storage.openSeance(&context, 5120.0, 5120.0);
  constexpr int kRootDataSize =
      1 + sizeof(int) + 1 + sizeof(double) + 1 +
      sizeof("mission.commander");
  constexpr int kCommanderDataSize = 1 + sizeof("mission.commander");
  projectTable.create(4, &context, storage, 8,
                      kRootDataSize + kCommanderDataSize);

  const mp_NodeNum leaf = mp_New(projectTable, 22, mp_NodeNULL(), mp_NodeNULL());
  const mp_NodeNum commander =
      mp_New(projectTable, COM_0COMMANDER, mp_NodeNULL(), mp_NodeNULL());
  const mp_NodeNum root = mp_New(projectTable, 11, leaf, commander);

  mp_OpenData(projectTable, root, EDO_WRITE);
  mp_WriteInt(projectTable, root, 0x12345678);
  mp_WriteFloat(projectTable, root, 12.5);
  mp_WriteStr(projectTable, root, "mission.commander");
  mp_CloseData(projectTable, root);

  int integer_value = 0;
  double float_value = 0.0;
  char string_value[64] = {};
  mp_OpenData(projectTable, root, EDO_READ);
  mp_ReadInt(projectTable, root, integer_value);
  mp_ReadFloat(projectTable, root, float_value);
  mp_ReadStr(projectTable, root, string_value);
  mp_CloseData(projectTable, root);

  mp_OpenData(projectTable, commander, EDO_WRITE);
  mp_WriteStr(projectTable, commander, "mission.commander");
  mp_CloseData(projectTable, commander);

  KR_ObjectID invalid_project =
      projectTable.newProject("mproj.invalid", mp_NodeNULL(), 0);

  KR_ObjectID project_id = projectTable.newProject("mproj.smoke", root, 1);
  mp_Project* project = projectTable.searchProject(project_id);
  const bool matches =
      !project_id.isNUL() && project != nullptr && project->m_permanent == 1 &&
      projectTable.getProjectRoot(project_id) == root &&
      projectTable.getCommand(root) == 11 && projectTable.getLeft(root) == leaf &&
      projectTable.getRight(root) == commander &&
      projectTable.getCommand(leaf) == 22 && integer_value == 0x12345678 &&
      float_value == 12.5 &&
      std::strcmp(string_value, "mission.commander") == 0 &&
      invalid_project.isNUL() && !context.isExist("mproj.invalid") &&
      mp_IsCommanderEqu(projectTable, root, "mission.commander") &&
      !mp_IsCommanderEqu(projectTable, root, "other.commander");

  storage.closeSeance();
  return matches;
}

}  // namespace

int main(int argc, char** argv) {
  static_assert(sizeof(void*) == 4, "MPROJ contract requires Win32");
  static_assert(sizeof(int) == 4, "MPROJ node format requires 4-byte int");
  static_assert(sizeof(double) == 8,
                "MPROJ data format requires 8-byte double");
  static_assert(sizeof(mp_ProjectData) == 8,
                "MPROJ project save layout changed");
  if (argc != 2) {
    return Fail("expected a temporary save path");
  }

  std::remove(argv[1]);
  if (!ExerciseProjectDataFile(argv[1])) {
    return Fail("project save layout round-trip diverged");
  }
  if (!ExerciseProjectTree()) {
    return Fail("project tree, heap or storage lifecycle diverged");
  }

  std::remove(argv[1]);
  std::cout << "legacy-mproj-smoke: OK\n";
  return EXIT_SUCCESS;
}
