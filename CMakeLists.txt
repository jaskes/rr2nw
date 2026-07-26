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

add_executable(rr2nw_math_smoke
  src/portability/LegacyMathSmoke.cpp
)
target_link_libraries(rr2nw_math_smoke PRIVATE rr2nw_legacy_design)
target_compile_features(rr2nw_math_smoke PRIVATE cxx_std_14)

if(MSVC)
  target_compile_options(rr2nw_math_smoke PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
  add_test(NAME legacy-math-smoke COMMAND rr2nw_math_smoke)
endif()
