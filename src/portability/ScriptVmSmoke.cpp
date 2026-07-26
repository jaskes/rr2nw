#include <cstdlib>
#include <iostream>

#include "sc.h"

namespace {

int Fail(const char* message) {
  std::cerr << "legacy-script-vm-smoke: " << message << '\n';
  return EXIT_FAILURE;
}

}  // namespace

#ifdef _MSC_VER
// The legacy compiler reports errors through setjmp/longjmp. This smoke keeps
// only POD state across that boundary and suppresses MSVC's generic C++ warning.
#pragma warning(push)
#pragma warning(disable : 4611)
#endif

int main() {
  static_assert(sizeof(void*) == 4, "script VM requires a Win32 process");
  static_assert(sizeof(TInt) == 4, "script bytecode requires a 32-bit TInt");
  static_assert(sizeof(TFloat) == 8,
                "script bytecode requires a 64-bit TFloat");

  TSuaCript* const compiler =
      static_cast<TSuaCript*>(std::calloc(1, sizeof(TSuaCript)));
  if (compiler == nullptr) {
    return Fail("could not allocate compiler state");
  }
  if (!sc_InitTSuaCript(compiler,
                        1024,
                        1024,
                        128,
                        1024,
                        2048,
                        1024,
                        128,
                        1)) {
    std::free(compiler);
    return Fail("could not initialize compiler buffers");
  }

  if (SUACRIPT_REGISTER_ERROR_HANDLE(*compiler)) {
    std::cerr << "legacy-script-vm-smoke: compiler error: "
              << compiler->m_heap.m_error.m_msg << '\n';
    sc_DeleteTSuaCript(compiler);
    std::free(compiler);
    return EXIT_FAILURE;
  }

  TLinkExtern external_functions[] = {{nullptr, nullptr, nullptr}};
  TLinkConstExtern external_constants[] = {{nullptr, nullptr, 0}};
  char source[] = "func void main() { }";
  sc_InitScannerFromMem(compiler, source);
  sc_Compile(compiler, "minimal", external_functions, external_constants);

  const int program_id = sc_ProgrammId(compiler, "minimal");
  if (program_id != 0 || compiler->m_programmCnt != 1 ||
      compiler->m_programm[program_id].m_entry < 0 ||
      compiler->m_programm[program_id].m_code.m_pos <= 0) {
    sc_DeleteTSuaCript(compiler);
    std::free(compiler);
    return Fail("minimal program did not produce runnable bytecode");
  }

  TSetOfProcess processes = {};
  if (!sc_InitTSetOfProcess(&processes, 256, 1) ||
      !sc_CreateProcess(&processes,
                        compiler,
                        program_id,
                        256,
                        64,
                        external_constants)) {
    std::free(processes.m_stacks);
    std::free(processes.m_process);
    sc_DeleteTSuaCript(compiler);
    std::free(compiler);
    return Fail("could not create a VM process");
  }

  const bool completed = sc_RunProcess(&processes, 1, nullptr) != 0;
  std::free(processes.m_stacks);
  std::free(processes.m_process);
  sc_DeleteTSuaCript(compiler);
  std::free(compiler);
  if (!completed) {
    return Fail("minimal program did not return within one VM slice");
  }

  std::cout << "legacy-script-vm-smoke: OK\n";
  return EXIT_SUCCESS;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
