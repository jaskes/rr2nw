cmake_minimum_required(VERSION 3.24)

project(
  RR2NW
  VERSION 0.1.0
  DESCRIPTION "Modern continuation of Russian Roulette II: The Next Worlds"
  LANGUAGES C CXX
)

if(NOT WIN32)
  message(FATAL_ERROR "RR2NW milestone M1 currently supports Windows only")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "RR2NW milestone M1 requires a 32-bit (Win32) toolchain")
endif()

include(CTest)

add_library(rr2nw_legacy_design STATIC
  nw/DESIGN.LIB/SRC/DEBUGEXT/ASSERT.CPP
  nw/DESIGN.LIB/SRC/FILESYS/FILESYS.CPP
  nw/DESIGN.LIB/SRC/MATHLIB/MATHLIB.CPP
)
target_include_directories(rr2nw_legacy_design PUBLIC
  nw/DESIGN.LIB/H
)
target_compile_features(rr2nw_legacy_design PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_legacy_design PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_legacy_design PRIVATE /W3)
endif()

add_library(rr2nw_script_compiler STATIC
  nw/ARENA/SC/SRC/PCGEN.CPP
  nw/ARENA/SC/SRC/PHEAP.CPP
  nw/ARENA/SC/SRC/PINSTRM.CPP
  nw/ARENA/SC/SRC/PLEX.CPP
  nw/ARENA/SC/SRC/PLEXDESC.CPP
  nw/ARENA/SC/SRC/POUTSTRM.CPP
  nw/ARENA/SC/SRC/PPARSER.CPP
  nw/ARENA/SC/SRC/PSTREQU.CPP
  nw/ARENA/SC/SRC/PTR_CODE.CPP
  nw/ARENA/SC/SRC/PTR_DATA.CPP
  nw/ARENA/SC/SRC/PUSESTK.CPP
)
target_include_directories(rr2nw_script_compiler PUBLIC
  nw/ARENA/SC/H
)
target_compile_features(rr2nw_script_compiler PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_script_compiler PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_script_runtime STATIC
  nw/ARENA/SC/ENV/PCINTER.CPP
  nw/ARENA/SC/ENV/PLANGER.CPP
  nw/ARENA/SC/ENV/PSC.CPP
)
target_link_libraries(rr2nw_script_runtime PUBLIC
  rr2nw_script_compiler
  rr2nw_legacy_design
)
target_include_directories(rr2nw_script_runtime PUBLIC
  nw/ARENA/SC/H
  nw/DESIGN.LIB/H
)
target_compile_features(rr2nw_script_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_script_runtime PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_script_compiler PRIVATE /W3)
  target_compile_options(rr2nw_script_runtime PRIVATE /W3)
endif()

add_executable(rr2nw_math_smoke
  src/portability/LegacyMathSmoke.cpp
)
target_link_libraries(rr2nw_math_smoke PRIVATE rr2nw_legacy_design)
target_compile_features(rr2nw_math_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_tagged_file_smoke
  src/portability/TaggedFileSmoke.cpp
)
target_link_libraries(rr2nw_tagged_file_smoke PRIVATE rr2nw_legacy_design)
target_compile_features(rr2nw_tagged_file_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_script_vm_smoke
  src/portability/ScriptVmSmoke.cpp
)
target_link_libraries(rr2nw_script_vm_smoke PRIVATE rr2nw_script_runtime)
target_compile_features(rr2nw_script_vm_smoke PRIVATE cxx_std_14)

if(MSVC)
  target_compile_options(rr2nw_math_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_tagged_file_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_script_vm_smoke PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
  add_test(NAME legacy-math-smoke COMMAND rr2nw_math_smoke)
  add_test(
    NAME legacy-tagged-file-smoke
    COMMAND rr2nw_tagged_file_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/tagged-file-smoke.tfb"
  )
  add_test(NAME legacy-script-vm-smoke COMMAND rr2nw_script_vm_smoke)
endif()
