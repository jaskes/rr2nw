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

add_library(rr2nw_arena_kernel STATIC
  nw/ARENA/KERNEL/SRC/Object.cpp
  nw/ARENA/KERNEL/SRC/S_DEBUG.CPP
  nw/ARENA/KERNEL/SRC/SETOFID.CPP
  nw/ARENA/KERNEL/SRC/Context.cpp
  nw/ARENA/KERNEL/SRC/ContextSave.cpp
  nw/ARENA/KERNEL/SRC/Session.cpp
  nw/ARENA/KERNEL/SRC/Echo.cpp
  nw/ARENA/KERNEL/SRC/S_EVDATA.CPP
  nw/ARENA/KERNEL/SRC/Active.cpp
)
target_link_libraries(rr2nw_arena_kernel PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_arena_kernel PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_kernel PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_kernel PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  # HARDWARE.H contains Watcom's `#pragma warning 004` syntax. MSVC diagnoses
  # the foreign pragma before it can otherwise ignore it.
  target_compile_options(rr2nw_arena_kernel PRIVATE /W3 /wd4081)
endif()

set_source_files_properties(
  nw/ARENA/KERNEL/SRC/Context.cpp
  PROPERTIES COMPILE_DEFINITIONS RR2NW_CONTEXT_CORE_ONLY
)

add_library(rr2nw_arena_storage STATIC
  nw/ARENA/STORAGE/Attr.cpp
  nw/ARENA/STORAGE/Classtab.cpp
  nw/ARENA/STORAGE/Subject.cpp
  nw/ARENA/STORAGE/SaveFile.cpp
  nw/ARENA/STORAGE/GameSave.cpp
)
target_link_libraries(rr2nw_arena_storage PUBLIC rr2nw_arena_kernel)
target_include_directories(rr2nw_arena_storage PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_storage PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_storage PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_arena_storage PRIVATE /W3 /wd4081)
endif()

add_library(rr2nw_arena_obase_route STATIC
  nw/ARENA/OBASE/ROUTE/Route.cpp
  nw/ARENA/OBASE/ROUTE/ROUTE_I.CPP
)
target_link_libraries(rr2nw_arena_obase_route PUBLIC rr2nw_arena_storage)
target_include_directories(rr2nw_arena_obase_route PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_route PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_route PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_arena_obase_route PRIVATE /W3 /wd4081)
endif()

add_library(rr2nw_arena_obase_fountain_state STATIC
  nw/ARENA/OBASE/Fountain/FountainState.cpp
  nw/ARENA/OBASE/Fountain/FountainState.inl
)
target_link_libraries(rr2nw_arena_obase_fountain_state
  PUBLIC rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_fountain_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_fountain_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_fountain_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_fountain_state PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_fountain STATIC
  nw/ARENA/OBASE/Fountain/Fountain.cpp
)
target_link_libraries(
  rr2nw_arena_obase_fountain PUBLIC rr2nw_arena_obase_fountain_state
)
target_include_directories(rr2nw_arena_obase_fountain PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_fountain PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_fountain PRIVATE
  __NT__
  RR2NW_FOUNTAIN_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_fountain PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_vehicle_state STATIC
  nw/ARENA/OBASE/Vehicle/VehicleState.cpp
  nw/ARENA/OBASE/Vehicle/VehicleStateData.inl
  nw/ARENA/OBASE/Vehicle/VehicleStateIO.inl
)
target_link_libraries(rr2nw_arena_obase_vehicle_state
  PUBLIC rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_vehicle_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_vehicle_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_vehicle_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_vehicle_state PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_vehicle STATIC
  nw/ARENA/OBASE/Vehicle/VEHICLE.cpp
  nw/ARENA/OBASE/Vehicle/Vh_dyn.cpp
  nw/ARENA/OBASE/Vehicle/vh_vessel.CPP
  nw/ARENA/OBASE/Vehicle/VS_ZAV.CPP
)
target_link_libraries(
  rr2nw_arena_obase_vehicle PUBLIC rr2nw_arena_obase_vehicle_state
)
target_include_directories(rr2nw_arena_obase_vehicle PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_vehicle PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_vehicle PRIVATE
  __NT__
  RR2NW_VEHICLE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_vehicle PRIVATE /W3 /permissive- /wd4081
  )
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

add_executable(rr2nw_kernel_state_smoke
  src/portability/KernelStateSmoke.cpp
)
target_link_libraries(rr2nw_kernel_state_smoke PRIVATE rr2nw_arena_kernel)
target_compile_features(rr2nw_kernel_state_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_storage_save_smoke
  src/portability/StorageSaveSmoke.cpp
)
target_link_libraries(rr2nw_storage_save_smoke PRIVATE rr2nw_arena_storage)
target_compile_features(rr2nw_storage_save_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_context_route_smoke
  src/portability/ContextRouteSmoke.cpp
)
target_link_libraries(rr2nw_context_route_smoke PRIVATE rr2nw_arena_obase_route)
target_compile_features(rr2nw_context_route_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_fountain_state_smoke
  src/portability/FountainStateSmoke.cpp
)
target_link_libraries(
  rr2nw_fountain_state_smoke PRIVATE rr2nw_arena_obase_fountain_state
)
target_compile_features(rr2nw_fountain_state_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_vehicle_state_smoke
  src/portability/VehicleStateSmoke.cpp
)
target_link_libraries(
  rr2nw_vehicle_state_smoke PRIVATE rr2nw_arena_obase_vehicle_state
)
target_compile_features(rr2nw_vehicle_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_vehicle_state_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_math_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_tagged_file_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_script_vm_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_kernel_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_storage_save_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_context_route_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_fountain_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_vehicle_state_smoke PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
  add_test(NAME legacy-math-smoke COMMAND rr2nw_math_smoke)
  add_test(
    NAME legacy-tagged-file-smoke
    COMMAND rr2nw_tagged_file_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/tagged-file-smoke.tfb"
  )
  add_test(NAME legacy-script-vm-smoke COMMAND rr2nw_script_vm_smoke)
  add_test(NAME legacy-kernel-state-smoke COMMAND rr2nw_kernel_state_smoke)
  add_test(
    NAME legacy-storage-save-smoke
    COMMAND rr2nw_storage_save_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/storage-save-smoke.sav"
  )
  add_test(
    NAME legacy-context-route-smoke
    COMMAND rr2nw_context_route_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/context-route-smoke.sav"
  )
  add_test(
    NAME legacy-fountain-state-smoke
    COMMAND rr2nw_fountain_state_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/fountain-state-smoke.sav"
  )
  add_test(
    NAME legacy-vehicle-state-smoke
    COMMAND rr2nw_vehicle_state_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/vehicle-state-smoke.sav"
  )
endif()
