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
  nw/DESIGN.LIB/SRC/FILESYS/CONFIG.CPP
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

add_library(rr2nw_mod_runtime STATIC
  src/runtime/RecoveredModRuntime.cpp
  src/runtime/RecoveredModRuntime.h
)
target_link_libraries(rr2nw_mod_runtime PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_mod_runtime PUBLIC
  src/runtime
  nw/DESIGN.LIB/H
)
target_compile_features(rr2nw_mod_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_mod_runtime PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_mod_runtime PRIVATE /W4 /permissive-)
endif()

add_library(rr2nw_simulation_random STATIC
  src/runtime/SimulationRandom.cpp
  src/runtime/SimulationRandom.h
)
target_include_directories(rr2nw_simulation_random PUBLIC src/runtime)
target_compile_features(rr2nw_simulation_random PUBLIC cxx_std_14)
if(MSVC)
  target_compile_options(
    rr2nw_simulation_random PRIVATE /W4 /permissive-
  )
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
  rr2nw_simulation_random
)
target_include_directories(rr2nw_script_runtime PUBLIC
  src/runtime
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

add_library(rr2nw_sound_state STATIC
  nw/SOUND/SoundState.cpp
  nw/SOUND/SoundStateData.inl
)
target_include_directories(rr2nw_sound_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
)
target_compile_features(rr2nw_sound_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_sound_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_sound_state PRIVATE /W3 /permissive-)
endif()

add_library(rr2nw_arena_kernel STATIC
  nw/ARENA/KERNEL/SRC/Object.cpp
  nw/ARENA/KERNEL/SRC/S_DEBUG.CPP
  nw/ARENA/KERNEL/SRC/SETOFID.CPP
  nw/ARENA/KERNEL/SRC/Context.cpp
  nw/ARENA/KERNEL/SRC/ContextEventQueue.cpp
  nw/ARENA/KERNEL/SRC/SimulationContextRandom.cpp
  nw/ARENA/KERNEL/SRC/ContextSave.cpp
  nw/ARENA/KERNEL/SRC/Session.cpp
  nw/ARENA/KERNEL/SRC/Echo.cpp
  nw/ARENA/KERNEL/SRC/S_EVDATA.CPP
  nw/ARENA/KERNEL/SRC/Active.cpp
)
target_link_libraries(rr2nw_arena_kernel PUBLIC
  rr2nw_legacy_design
  rr2nw_simulation_random
)
target_include_directories(rr2nw_arena_kernel PUBLIC
  src/runtime
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
target_link_libraries(rr2nw_arena_storage PUBLIC
  rr2nw_arena_kernel
  rr2nw_sound_state
)
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

add_library(rr2nw_legacy_rsx_ids STATIC
  nw/SOUND/LegacyRsxIds.cpp
)
target_include_directories(rr2nw_legacy_rsx_ids PUBLIC
  nw/DESIGN.LIB/H
)
target_compile_features(rr2nw_legacy_rsx_ids PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_legacy_rsx_ids PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_time_runtime_state STATIC
  nw/ARENA/TimeRuntimeState.cpp
  nw/ARENA/TimeRuntimeState.h
  nw/ARENA/TimeRuntimeState.inl
)
target_link_libraries(rr2nw_arena_time_runtime_state PUBLIC
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_time_runtime_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_time_runtime_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_time_runtime_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_taxi_attribute_state STATIC
  nw/ARENA/OBASE/Taxi/TaxiAttributeState.cpp
  nw/ARENA/OBASE/Taxi/TaxiAttributeState.h
)
target_link_libraries(rr2nw_arena_taxi_attribute_state PUBLIC
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_taxi_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Taxi
)
target_compile_features(rr2nw_arena_taxi_attribute_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_taxi_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_legacy_rsx_ids PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_arena_time_runtime_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_taxi_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_spark_attribute_state STATIC
  nw/ARENA/OBASE/Spark/SparkAttributeState.cpp
  nw/ARENA/OBASE/Spark/SparkAttributeState.h
  nw/ARENA/OBASE/Spark/SparkAttributeState.inl
)
target_link_libraries(rr2nw_arena_spark_attribute_state PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_skin_resource_state
)
target_include_directories(rr2nw_arena_spark_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Spark
)
target_compile_features(
  rr2nw_arena_spark_attribute_state PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_arena_spark_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_spark_full STATIC
  nw/ARENA/OBASE/DynObj/DynSpr.cpp
  nw/ARENA/OBASE/Spark/SparkActiveWorldState.h
  nw/ARENA/OBASE/Spark/Spark.cpp
  nw/ARENA/OBASE/Spark/SparkSubjectState.h
)
target_link_libraries(rr2nw_arena_obase_spark_full PUBLIC
  rr2nw_arena_spark_attribute_state
)
target_include_directories(rr2nw_arena_obase_spark_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Spark
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_spark_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_spark_full PRIVATE
  __NT__
  RR2NW_SPARK_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_spark_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_obase_spark_full PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_bird_attribute_state STATIC
  nw/ARENA/OBASE/Bird/BirdAttributeState.cpp
  nw/ARENA/OBASE/Bird/BirdAttributeState.h
  nw/ARENA/OBASE/Bird/BirdAttributeState.inl
)
target_link_libraries(rr2nw_arena_bird_attribute_state PUBLIC
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_bird_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Bird
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_bird_attribute_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_bird_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_bird_full STATIC
  nw/ARENA/OBASE/Bird/Bird.cpp
)
target_link_libraries(rr2nw_arena_obase_bird_full PUBLIC
  rr2nw_arena_bird_attribute_state
)
target_include_directories(rr2nw_arena_obase_bird_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Bird
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_bird_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_bird_full PRIVATE
  __NT__
  RR2NW_BIRD_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_orphan_attribute_state STATIC
  nw/ARENA/OBASE/Orphan/OrphanAttributeState.cpp
  nw/ARENA/OBASE/Orphan/OrphanAttributeState.h
  nw/ARENA/OBASE/Orphan/OrphanAttributeState.inl
)
target_link_libraries(rr2nw_arena_orphan_attribute_state PUBLIC
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_orphan_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Orphan
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_orphan_attribute_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_orphan_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_orphan_full STATIC
  nw/ARENA/OBASE/Orphan/Orphan.cpp
  nw/ARENA/OBASE/Orphan/OrphanSubjectState.h
)
target_link_libraries(rr2nw_arena_obase_orphan_full PUBLIC
  rr2nw_arena_orphan_attribute_state
)
target_include_directories(rr2nw_arena_obase_orphan_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Orphan
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_orphan_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_orphan_full PRIVATE
  __NT__
  RR2NW_ORPHAN_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_artefact_attribute_state STATIC
  nw/ARENA/OBASE/Artefact/ArtefactAttributeState.cpp
  nw/ARENA/OBASE/Artefact/ArtefactAttributeState.h
  nw/ARENA/OBASE/Artefact/ArtefactAttributeState.inl
)
target_link_libraries(rr2nw_arena_artefact_attribute_state PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_smoke_texture_cache
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_artefact_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Artefact
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_arena_artefact_attribute_state PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_arena_artefact_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_portal STATIC
  nw/ARENA/OBASE/Portal/portal.cpp
  nw/ARENA/OBASE/Portal/PortalClassTableState.h
)
target_link_libraries(rr2nw_arena_obase_portal PUBLIC rr2nw_arena_storage)
target_include_directories(rr2nw_arena_obase_portal PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Portal
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_portal PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_portal PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_bird_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_obase_bird_full PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_orphan_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_obase_orphan_full PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_artefact_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_obase_portal PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_physics_math STATIC
  nw/ARENA/PHISICS.CPP
)
target_link_libraries(rr2nw_arena_physics_math PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_arena_physics_math PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_physics_math PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_physics_math PRIVATE
  __NT__
  RR2NW_PHYSICS_MATH_ONLY
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_dynamic_collision STATIC
  nw/VIEW/DYNAMIC.CPP
)
target_link_libraries(rr2nw_view_dynamic_collision PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_view_dynamic_collision PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_dynamic_collision PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_dynamic_collision PRIVATE
  __NT__
  RR2NW_DYNAMIC_COLLISION_ONLY
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_physics STATIC
  nw/ARENA/PHISICS.CPP
)
target_link_libraries(rr2nw_arena_physics PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_physics_math
  rr2nw_view_dynamic_collision
)
target_include_directories(rr2nw_arena_physics PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_physics PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_physics PRIVATE
  __NT__
  RR2NW_PHYSICS_MATH_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_briefing_god_mode_state STATIC
  nw/BRIEFING/GodModeState.cpp
  nw/BRIEFING/GodModeState.inl
)
target_compile_features(rr2nw_briefing_god_mode_state PUBLIC cxx_std_14)

add_library(rr2nw_view_object_state STATIC
  nw/VIEW/OBJECT.CPP
)
target_link_libraries(rr2nw_view_object_state PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_view_object_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_object_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_object_state PRIVATE
  __NT__
  RR2NW_VIEW_OBJECT_STATE_ONLY
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_figure_state STATIC
  nw/VIEW/FIGURE.CPP
)
target_link_libraries(rr2nw_view_figure_state PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_view_figure_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_figure_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_figure_state PRIVATE
  __NT__
  RR2NW_VIEW_FIGURE_STATE_ONLY
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_patterns_palette_frame_state STATIC
  nw/PATTERNS/PaletteFrameState.cpp
)
target_link_libraries(rr2nw_patterns_palette_frame_state PUBLIC
  rr2nw_legacy_design
)
target_include_directories(rr2nw_patterns_palette_frame_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/PATTERNS
  nw/VIEW
)
target_compile_features(
  rr2nw_patterns_palette_frame_state PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_patterns_palette_frame_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_frame_state STATIC
  nw/VIEW/ViewFrameState.cpp
  nw/VIEW/ViewHazeFrameState.cpp
)
target_link_libraries(rr2nw_view_frame_state PUBLIC
  rr2nw_graph_runtime_state
  rr2nw_patterns_palette_frame_state
  rr2nw_view_figure_state
  rr2nw_view_object_state
)
target_include_directories(rr2nw_view_frame_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_frame_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_frame_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_terrain_waterline_state STATIC
  nw/VIEW/TerrainWaterlineState.cpp
  nw/VIEW/TerrainWaterlineState.inl
)
target_link_libraries(
  rr2nw_view_terrain_waterline_state PUBLIC rr2nw_legacy_design
)
target_include_directories(rr2nw_view_terrain_waterline_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_terrain_waterline_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_terrain_waterline_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_terrain_view_state STATIC
  nw/VIEW/TerrainViewState.cpp
  nw/VIEW/TerrainViewState.h
)
target_link_libraries(rr2nw_view_terrain_view_state PUBLIC
  rr2nw_legacy_design
  rr2nw_view_figure_state
  rr2nw_view_object_state
  rr2nw_view_terrain_waterline_state
)
target_include_directories(rr2nw_view_terrain_view_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_terrain_view_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_terrain_view_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_terrain_full STATIC
  nw/VIEW/TERRAIN.CPP
)
target_link_libraries(rr2nw_view_terrain_full PUBLIC
  rr2nw_graph_fixed_font_runtime_state
  rr2nw_legacy_design
  rr2nw_view_terrain_view_state
)
target_include_directories(rr2nw_view_terrain_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_terrain_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_terrain_full PRIVATE
  __NT__
  RR2NW_TERRAIN_FRAME_COUNTERS_EXTERNAL
  RR2NW_TERRAIN_FRAME_END_EXTERNAL
  RR2NW_TERRAIN_VIEW_STATE_EXTERNAL
  RR2NW_TERRAIN_WATERLINE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_view_terrain_full PRIVATE /Gy)
endif()

add_library(rr2nw_main_zav_scene_state STATIC
  nw/MAIN/ZavSceneState.cpp
  nw/MAIN/ZavSceneState.h
  nw/MAIN/ZavSceneState.inl
)
target_include_directories(rr2nw_main_zav_scene_state PUBLIC nw/MAIN)
target_compile_features(rr2nw_main_zav_scene_state PUBLIC cxx_std_14)

add_library(rr2nw_frame_runtime STATIC
  nw/MAIN/FrameRuntimeState.cpp
  nw/MAIN/FrameRuntimeState.h
)
target_link_libraries(rr2nw_frame_runtime PUBLIC rr2nw_graph_runtime_state)
target_include_directories(rr2nw_frame_runtime PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
  nw/MAIN
)
target_compile_features(rr2nw_frame_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_frame_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_light_state STATIC
  nw/VIEW/ViewLightState.cpp
)
target_link_libraries(rr2nw_view_light_state PUBLIC
  rr2nw_graph_runtime_state
  rr2nw_view_object_state
)
target_include_directories(rr2nw_view_light_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_light_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_light_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_light_chain_core STATIC
  nw/ARENA/OBASE/LIGHT/LIGHTOBJ.CPP
)
target_link_libraries(rr2nw_arena_light_chain_core PUBLIC
  rr2nw_arena_kernel
)
target_include_directories(rr2nw_arena_light_chain_core PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_arena_light_chain_core PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_light_chain_core PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_light_chain INTERFACE)
target_link_libraries(rr2nw_arena_light_chain INTERFACE
  rr2nw_arena_light_chain_core
  rr2nw_view_light_state
)

add_library(rr2nw_view_land_dynamic_frame_state STATIC
  nw/VIEW/LandDynamicFrameState.cpp
  nw/VIEW/LandDynamicTerrainFrameState.cpp
  nw/VIEW/TerrainFrameCounters.cpp
)

add_library(rr2nw_view_dynamic_waste_state STATIC
  nw/VIEW/ViewDynamicWasteState.cpp
)
target_link_libraries(rr2nw_view_dynamic_waste_state PUBLIC
  rr2nw_legacy_design
)
target_include_directories(rr2nw_view_dynamic_waste_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/VIEW
)
target_compile_features(rr2nw_view_dynamic_waste_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_dynamic_waste_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
target_link_libraries(rr2nw_view_land_dynamic_frame_state PUBLIC
  rr2nw_legacy_design
)
target_include_directories(rr2nw_view_land_dynamic_frame_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/VIEW
)
target_compile_features(rr2nw_view_land_dynamic_frame_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_land_dynamic_frame_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_scene_full STATIC
  nw/SCENE/SCENE.CPP
)
target_link_libraries(rr2nw_scene_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_scene_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_scene_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_scene_full PRIVATE /Gy)
endif()

add_library(rr2nw_scene_software_draw STATIC
  nw/SCENE/SceneSoftwareDraw.cpp
  nw/SCENE/SceneSoftwareDraw.h
)
target_link_libraries(rr2nw_scene_software_draw PUBLIC
  rr2nw_graph_runtime_state
  rr2nw_legacy_design
  rr2nw_patterns_palette_frame_state
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
  rr2nw_view_figure_state
  rr2nw_view_frame_state
  rr2nw_view_land_dynamic_frame_state
  rr2nw_view_object_state
  rr2nw_view_terrain_view_state
  rr2nw_view_terrain_waterline_state
  rr2nw_view_dynamic_waste_state
)
target_include_directories(rr2nw_scene_software_draw PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_scene_software_draw PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_software_draw PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_recovered_software_frame STATIC
  nw/MAIN/RecoveredSoftwareFrame.cpp
  nw/MAIN/RecoveredSoftwareFrame.h
)
target_link_libraries(rr2nw_recovered_software_frame PUBLIC
  rr2nw_arena_light_chain
  rr2nw_arena_storage
  rr2nw_frame_runtime
  rr2nw_main_zav_scene_state
  rr2nw_scene_draw_state
  rr2nw_scene_software_draw
  rr2nw_scene_vessel_runtime_state
  rr2nw_view_figure_state
  rr2nw_view_land_dynamic_frame_state
  rr2nw_view_object_state
)
target_include_directories(rr2nw_recovered_software_frame PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_recovered_software_frame PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_recovered_software_frame PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_main_zav_shutdown_state STATIC
  nw/MAIN/ZavShutdownState.cpp
  nw/MAIN/ZavShutdownState.h
  nw/MAIN/ZavShutdownState.inl
  nw/MAIN/ZavOverallInfoState.cpp
  nw/MAIN/ZavOverallInfoState.h
  nw/MAIN/ZavOverallInfoState.inl
)
target_link_libraries(
  rr2nw_main_zav_shutdown_state PUBLIC rr2nw_main_zav_scene_state
)
target_include_directories(rr2nw_main_zav_shutdown_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/MAIN
)
target_compile_features(rr2nw_main_zav_shutdown_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_main_zav_shutdown_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_sua_shutdown_state STATIC
  nw/ARENA/SupervisorShutdownState.cpp
  nw/ARENA/SupervisorShutdownState.h
  nw/ARENA/SupervisorShutdownState.inl
)
target_include_directories(rr2nw_arena_sua_shutdown_state PUBLIC nw/ARENA)
target_compile_features(rr2nw_arena_sua_shutdown_state PUBLIC cxx_std_14)

add_library(rr2nw_main_zav_full STATIC
  nw/MAIN/ZAV.CPP
)
target_link_libraries(
  rr2nw_main_zav_full PUBLIC rr2nw_main_zav_shutdown_state
)
target_include_directories(rr2nw_main_zav_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BRIEFING
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_main_zav_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_main_zav_full PRIVATE
  __NT__
  ZAV_RenderFrame=ZAV_RenderFrameRecovered
  ZAV_EndRenderFrame=ZAV_EndRenderFrameRecovered
  ZAV_PrintFrameInfo=ZAV_PrintFrameInfoRecovered
  RR2NW_ZAV_SCENE_STATE_EXTERNAL
  RR2NW_ZAV_SHUTDOWN_EXTERNAL
  RR2NW_ZAV_OVERALL_INFO_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_super_full STATIC
  nw/ARENA/super.cpp
)
target_link_libraries(rr2nw_arena_super_full PUBLIC
  rr2nw_arena_sua_shutdown_state
  rr2nw_arena_time_runtime_state
)
target_include_directories(rr2nw_arena_super_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BRIEFING
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_super_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_super_full PRIVATE
  __NT__
  SUA_BeginRender=SUA_BeginRenderRecovered
  SUA_EndRender=SUA_EndRenderRecovered
  RR2NW_SHELL_GLOBALS_EXTERNAL
  RR2NW_DEBUG_MAP_GLOBAL_EXTERNAL
  RR2NW_MENU_GLOBAL_EXTERNAL
  RR2NW_TIME_RUNTIME_EXTERNAL
  RR2NW_SUA_SHUTDOWN_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_publish_full STATIC
  nw/ARENA/OBASE/Publish/Publish.cpp
)
target_link_libraries(
  rr2nw_arena_obase_publish_full PUBLIC rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_publish_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_publish_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_publish_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_script_viewpoints STATIC
  nw/ARENA/SC/ScriptViewPointState.cpp
  nw/ARENA/SC/ScriptViewPointState.inl
)
target_link_libraries(rr2nw_arena_script_viewpoints PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_arena_script_viewpoints PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_script_viewpoints PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_script_viewpoints PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_scene_moving_object STATIC
  nw/SCENE/MOVINGOB.CPP
)
target_link_libraries(rr2nw_scene_moving_object PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_scene_moving_object PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/SCENE
)
target_compile_features(rr2nw_scene_moving_object PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_moving_object PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_scene_vessel STATIC
  nw/SCENE/VESSEL.CPP
)
target_link_libraries(rr2nw_scene_vessel PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_scene_vessel PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/SCENE
)
target_compile_features(rr2nw_scene_vessel PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_vessel PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_scene_vessel_runtime_state STATIC
  nw/SCENE/VesselBonusState.cpp
  nw/SCENE/VesselRuntimeGlobals.cpp
)
target_link_libraries(
  rr2nw_scene_vessel_runtime_state PUBLIC rr2nw_legacy_design
)
target_include_directories(rr2nw_scene_vessel_runtime_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/SCENE
)
target_compile_features(rr2nw_scene_vessel_runtime_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_vessel_runtime_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_patterns_aligned_image_runtime STATIC
  nw/PATTERNS/AlignedImageRuntime.cpp
)
target_link_libraries(
  rr2nw_patterns_aligned_image_runtime PUBLIC rr2nw_legacy_design
)
target_include_directories(rr2nw_patterns_aligned_image_runtime PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/PATTERNS
)
target_compile_features(
  rr2nw_patterns_aligned_image_runtime PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_patterns_aligned_image_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_scene_draw_state STATIC
  nw/SCENE/SceneDrawState.cpp
)
target_link_libraries(rr2nw_scene_draw_state PUBLIC
  rr2nw_legacy_design
  rr2nw_patterns_aligned_image_runtime
  rr2nw_view_dynamic_waste_state
)
target_include_directories(rr2nw_scene_draw_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/SCENE
)
target_compile_features(rr2nw_scene_draw_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_draw_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

# Taxi.h exposes its attribute table beside the moving-object/Vessel classes,
# so MSVC emits those recovered virtual boundaries with the extracted owner.
target_link_libraries(rr2nw_arena_taxi_attribute_state PUBLIC
  rr2nw_view_object_state
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
)

add_library(rr2nw_graph_palette_full STATIC
  nw/GRAPH/SD1_EPAL.CPP
)
target_link_libraries(rr2nw_graph_palette_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_palette_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_palette_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_palette_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_assert_full STATIC
  nw/GRAPH/SOBJECT.CPP
  nw/GRAPH/CTRLBRK.CPP
)
target_link_libraries(rr2nw_graph_assert_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_assert_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_assert_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_assert_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_core_full STATIC
  nw/GRAPH/graph.cpp
)
target_link_libraries(rr2nw_graph_core_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_core_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_core_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_core_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_panel_full STATIC
  nw/GRAPH/PANEL.CPP
)
target_link_libraries(rr2nw_graph_panel_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_panel_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_panel_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_panel_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_2d_full STATIC
  nw/GRAPH/2dgraph.cpp
)
target_link_libraries(rr2nw_graph_2d_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_2d_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_2d_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_2d_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_image_full STATIC
  nw/GRAPH/image.cpp
)
target_link_libraries(rr2nw_graph_image_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_image_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_image_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_image_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_fixed_font_full STATIC
  nw/GRAPH/FIXEDFNT.CPP
)
target_link_libraries(rr2nw_graph_fixed_font_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_fixed_font_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_fixed_font_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_fixed_font_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_d3d_full STATIC
  nw/GRAPH/DRAWD3D.CPP
)
target_link_libraries(rr2nw_graph_d3d_full PUBLIC rr2nw_legacy_design)
target_include_directories(rr2nw_graph_d3d_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_d3d_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_d3d_full PRIVATE
  __NT__
  D3D_DrawZList=D3D_DrawZListHardware
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_runtime_state STATIC
  nw/GRAPH/GraphRuntimeState.cpp
)
target_link_libraries(rr2nw_graph_runtime_state PUBLIC
  rr2nw_graph_palette_full
  rr2nw_graph_assert_full
  rr2nw_legacy_design
)
target_include_directories(rr2nw_graph_runtime_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_runtime_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_runtime_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_patterns_palette_runtime STATIC
  nw/PATTERNS/PALTRANS.CPP
)
target_link_libraries(rr2nw_patterns_palette_runtime PUBLIC
  rr2nw_graph_runtime_state
  rr2nw_patterns_palette_frame_state
  rr2nw_legacy_design
)
target_include_directories(rr2nw_patterns_palette_runtime PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
  nw/PATTERNS
  nw/VIEW
)
target_compile_features(rr2nw_patterns_palette_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_patterns_palette_runtime PRIVATE
  __NT__
  RR2NW_PALETTE_FRAME_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_patterns_palette_runtime PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_graph_software_texture STATIC
  nw/GRAPH/GraphSoftwareTexture.cpp
)
target_link_libraries(rr2nw_graph_software_texture PUBLIC
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_graph_software_texture PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_software_texture PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_software_texture PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_graph_software_texture PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_view_figure_library_state STATIC
  nw/VIEW/ViewFigureLibraryState.cpp
  nw/VIEW/ViewFigureLibraryState.h
)
target_link_libraries(rr2nw_view_figure_library_state PUBLIC
  rr2nw_graph_software_texture
  rr2nw_legacy_design
)
target_include_directories(rr2nw_view_figure_library_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_figure_library_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_figure_library_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_view_figure_library_state PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_view_object_decoder STATIC
  nw/VIEW/BODY.CPP
  nw/VIEW/DYNAMIC.CPP
  nw/VIEW/FIGURE.CPP
  nw/VIEW/KEYFRAME.CPP
  nw/VIEW/OBJECT.CPP
  nw/VIEW/ORDER.CPP
  nw/BUSH/BRECT.CPP
  nw/BUSH/BUSH.CPP
  nw/BUSH/BUSH_M.CPP
  nw/BUSH/Brend.cpp
)
target_link_libraries(rr2nw_view_object_decoder PUBLIC
  rr2nw_graph_software_texture
  rr2nw_legacy_design
  rr2nw_patterns_palette_runtime
  rr2nw_view_figure_library_state
  rr2nw_view_figure_state
  rr2nw_view_dynamic_collision
  rr2nw_view_object_state
  rr2nw_view_dynamic_waste_state
)
target_include_directories(rr2nw_view_object_decoder PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BUSH
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_object_decoder PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_object_decoder PRIVATE
  __NT__
  RR2NW_DYNAMIC_COLLISION_EXTERNAL
  RR2NW_VIEW_FRAME_STATE_EXTERNAL
  RR2NW_VIEW_FIGURE_LIBRARY_STATE_EXTERNAL
  RR2NW_VIEW_FIGURE_STATE_EXTERNAL
  RR2NW_VIEW_OBJECT_STATE_EXTERNAL
  RR2NW_VIEW_DYNAMIC_WASTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_view_object_decoder PRIVATE /W3 /permissive- /Gy /wd4081 /wd4244
  )
endif()

add_library(rr2nw_view_terrain_decoder STATIC
  nw/VIEW/TERRAIN.CPP
)
target_link_libraries(rr2nw_view_terrain_decoder PUBLIC
  rr2nw_graph_fixed_font_runtime_state
  rr2nw_graph_software_texture
  rr2nw_legacy_design
  rr2nw_patterns_aligned_image_runtime
  rr2nw_patterns_palette_runtime
  rr2nw_view_terrain_waterline_state
)
target_include_directories(rr2nw_view_terrain_decoder PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_terrain_decoder PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_terrain_decoder PRIVATE
  __NT__
  RR2NW_TERRAIN_DECODER_ONLY
  RR2NW_TERRAIN_FRAME_COUNTERS_EXTERNAL
  RR2NW_TERRAIN_FRAME_END_EXTERNAL
  RR2NW_TERRAIN_WATERLINE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_view_terrain_decoder PRIVATE /W3 /permissive- /Gy /wd4081 /wd4244
  )
endif()

add_library(rr2nw_view_order_decode_state STATIC
  nw/VIEW/ViewOrderedDecodeState.cpp
)
target_link_libraries(rr2nw_view_order_decode_state PUBLIC
  rr2nw_legacy_design
)
target_include_directories(rr2nw_view_order_decode_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_order_decode_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_order_decode_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_view_land_object_full STATIC
  nw/VIEW/OBJMAP.CPP
)
target_link_libraries(rr2nw_view_land_object_full PUBLIC
  rr2nw_legacy_design
  rr2nw_view_land_dynamic_frame_state
)
target_include_directories(rr2nw_view_land_object_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_land_object_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_land_object_full PRIVATE
  __NT__
  RR2NW_LAND_DYNAMIC_FRAME_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_view_land_object_full PRIVATE /W3 /permissive- /Gy /wd4081 /wd4244
  )
endif()

add_library(rr2nw_view_land_object_decoder STATIC
  nw/VIEW/OBJMAP.CPP
)
target_link_libraries(rr2nw_view_land_object_decoder PUBLIC
  rr2nw_legacy_design
  rr2nw_view_order_decode_state
)
target_include_directories(rr2nw_view_land_object_decoder PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_view_land_object_decoder PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_view_land_object_decoder PRIVATE
  __NT__
  RR2NW_LAND_OBJECT_DECODER_ONLY
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_view_land_object_decoder PRIVATE /W3 /permissive- /Gy /wd4081 /wd4244
  )
endif()

add_library(rr2nw_graph_image_runtime_state STATIC
  nw/GRAPH/ImageRuntimeState.cpp
)
target_link_libraries(rr2nw_graph_image_runtime_state PUBLIC
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_graph_image_runtime_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_image_runtime_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_image_runtime_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_fixed_font_runtime_state STATIC
  nw/GRAPH/FixedFontRuntimeState.cpp
)
target_link_libraries(rr2nw_graph_fixed_font_runtime_state PUBLIC
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_graph_fixed_font_runtime_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(
  rr2nw_graph_fixed_font_runtime_state PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_graph_fixed_font_runtime_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_graph_panel_lifecycle STATIC
  nw/GRAPH/PanelSoftwareLifecycle.cpp
)
target_link_libraries(rr2nw_graph_panel_lifecycle PUBLIC
  rr2nw_graph_image_runtime_state
  rr2nw_graph_fixed_font_runtime_state
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_graph_panel_lifecycle PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
)
target_compile_features(rr2nw_graph_panel_lifecycle PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_graph_panel_lifecycle PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_physics_math PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_view_dynamic_collision PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_physics PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(rr2nw_briefing_god_mode_state PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_view_object_state PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_view_figure_state PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_view_terrain_waterline_state
    PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(rr2nw_main_zav_scene_state PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_main_zav_shutdown_state PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_arena_sua_shutdown_state PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_frame_runtime PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_view_light_state PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_patterns_palette_frame_state PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_view_frame_state PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_view_terrain_view_state PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_arena_light_chain_core PRIVATE /W3 /permissive-)
  target_compile_options(
    rr2nw_view_land_dynamic_frame_state PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_scene_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_scene_software_draw PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_view_terrain_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_recovered_software_frame PRIVATE /W4 /permissive- /wd4459
  )
  target_compile_options(
    rr2nw_main_zav_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_super_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_obase_publish_full PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_script_viewpoints PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_scene_moving_object PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_scene_vessel PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_scene_vessel_runtime_state PRIVATE /W3 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_scene_draw_state PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_palette_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_assert_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_core_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_panel_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_2d_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_image_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_fixed_font_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_d3d_full PRIVATE /W3 /permissive- /wd4018 /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_graph_runtime_state PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_graph_image_runtime_state PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_graph_fixed_font_runtime_state PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_graph_panel_lifecycle PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_arena_mproj STATIC
  nw/ARENA/MPROJ/Mproj.cpp
)
target_link_libraries(rr2nw_arena_mproj PUBLIC rr2nw_arena_storage)
target_include_directories(rr2nw_arena_mproj PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_mproj PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_mproj PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_mproj PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_level_state STATIC
  nw/ARENA/LevelState.cpp
  nw/ARENA/LevelStateData.inl
)
target_link_libraries(rr2nw_arena_level_state PUBLIC rr2nw_arena_storage)
target_include_directories(rr2nw_arena_level_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_level_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_level_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_level_state PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_const_string STATIC
  nw/ARENA/CNSTSTR.CPP
)
target_include_directories(rr2nw_arena_const_string PUBLIC
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_const_string PUBLIC cxx_std_14)

if(MSVC)
  target_compile_options(rr2nw_arena_const_string PRIVATE
    /W3 /permissive- /wd4146 /wd4245 /wd4702
  )
endif()

add_library(rr2nw_briefing_channel_map STATIC
  nw/BRIEFING/MAPCHNNL.CPP
)
target_link_libraries(rr2nw_briefing_channel_map PUBLIC
  rr2nw_legacy_design
)
target_include_directories(rr2nw_briefing_channel_map PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BRIEFING
)
target_compile_features(rr2nw_briefing_channel_map PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_briefing_channel_map PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_briefing_channel_map PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_smoke_texture_cache STATIC
  nw/ARENA/OBASE/Smoke/SmokeTextureCache.cpp
  nw/ARENA/OBASE/Smoke/SmokeTextureCache.inl
)
target_link_libraries(rr2nw_arena_smoke_texture_cache PUBLIC
  rr2nw_graph_software_texture
  rr2nw_legacy_design
)
target_include_directories(rr2nw_arena_smoke_texture_cache PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
)
target_compile_features(rr2nw_arena_smoke_texture_cache PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_smoke_texture_cache PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_smoke_texture_cache PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_arena_explosion_attribute_state STATIC
  nw/ARENA/OBASE/Explosion/ExplosionActiveWorldState.h
  nw/ARENA/OBASE/Explosion/ExplosionAttributeState.cpp
  nw/ARENA/OBASE/Explosion/ExplosionAttributeState.h
  nw/ARENA/OBASE/Explosion/ExplosionSubjectState.cpp
  nw/ARENA/OBASE/Explosion/ExplosionSubjectState.h
)
target_link_libraries(rr2nw_arena_explosion_attribute_state PUBLIC
  rr2nw_arena_light_chain
  rr2nw_arena_level_state
  rr2nw_arena_skin_resource_state
  rr2nw_arena_smoke_subject_state
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_sound_object_state
  rr2nw_arena_storage
  rr2nw_graph_software_texture
)
target_include_directories(rr2nw_arena_explosion_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Explosion
)
target_compile_features(
  rr2nw_arena_explosion_attribute_state PUBLIC cxx_std_14
)
target_compile_definitions(
  rr2nw_arena_explosion_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_explosion_attribute_state PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_skin_resource_state STATIC
  nw/ARENA/OBASE/Skin/SkinResourceState.cpp
  nw/ARENA/OBASE/Skin/SkinResourceState.h
  nw/ARENA/OBASE/Skin/SKIN.H
  nw/ARENA/OBASE/Skin/AnimPeop.h
)
target_link_libraries(rr2nw_arena_skin_resource_state PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_time_runtime_state
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
  rr2nw_view_frame_state
  rr2nw_view_object_decoder
)
target_include_directories(rr2nw_arena_skin_resource_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Skin
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_arena_skin_resource_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_skin_resource_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_arena_skin_resource_state PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_recovered_skin_resource_catalog STATIC
  src/runtime/RecoveredSkinResourceCatalog.cpp
  src/runtime/RecoveredSkinResourceCatalog.h
)
target_link_libraries(rr2nw_recovered_skin_resource_catalog PUBLIC
  rr2nw_mod_runtime
)
target_include_directories(rr2nw_recovered_skin_resource_catalog PUBLIC
  src/runtime
)
target_compile_features(
  rr2nw_recovered_skin_resource_catalog PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_recovered_skin_resource_catalog PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_recovered_skin_resource_catalog PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_recovered_wav_metadata_catalog STATIC
  src/runtime/RecoveredWavMetadataCatalog.cpp
  src/runtime/RecoveredWavMetadataCatalog.h
)
target_link_libraries(rr2nw_recovered_wav_metadata_catalog PUBLIC
  rr2nw_mod_runtime
)
target_include_directories(rr2nw_recovered_wav_metadata_catalog PUBLIC
  src/runtime
)
target_compile_features(
  rr2nw_recovered_wav_metadata_catalog PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_recovered_wav_metadata_catalog PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_recovered_wav_metadata_catalog PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_arena_smoke_attribute_state STATIC
  nw/ARENA/OBASE/Smoke/SmokeAttributeState.cpp
  nw/ARENA/OBASE/Smoke/SmokeAttributeState.h
  nw/ARENA/OBASE/Smoke/SmokeAttributeState.inl
)
target_link_libraries(rr2nw_arena_smoke_attribute_state PUBLIC
  rr2nw_arena_physics
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_storage
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_smoke_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_arena_smoke_attribute_state PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_arena_smoke_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_smoke_attribute_state PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_smoker_attribute_state STATIC
  nw/ARENA/OBASE/Smoke/SmokerAttributeState.cpp
  nw/ARENA/OBASE/Smoke/SmokerAttributeState.h
  nw/ARENA/OBASE/Smoke/SmokerAttributeState.inl
)
target_link_libraries(rr2nw_arena_smoker_attribute_state PUBLIC
  rr2nw_arena_smoke_attribute_state
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_storage
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_smoker_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_arena_smoker_attribute_state PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_arena_smoker_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_arena_smoker_attribute_state PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_smoke_visual_state STATIC
  nw/ARENA/OBASE/Smoke/SmokeVisualState.cpp
  nw/ARENA/OBASE/Smoke/SmokeVisualState.h
)
target_link_libraries(rr2nw_arena_smoke_visual_state PUBLIC
  rr2nw_arena_smoke_attribute_state
  rr2nw_arena_smoke_subject_state
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_smoker_attribute_state
  rr2nw_arena_storage
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_smoke_visual_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_smoke_visual_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_smoke_visual_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_smoke_visual_state PRIVATE
    /W4 /permissive-
  )
endif()

add_library(rr2nw_arena_wav_resource_state STATIC
  nw/ARENA/OBASE/Sound/WAVResourceState.cpp
  nw/ARENA/OBASE/Sound/WAVResourceState.h
  nw/ARENA/OBASE/Sound/WAVResourceState.inl
  nw/ARENA/OBASE/Sound/WAVObj.h
)
target_link_libraries(rr2nw_arena_wav_resource_state PUBLIC
  rr2nw_arena_storage
  rr2nw_sound_state
)
target_include_directories(rr2nw_arena_wav_resource_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Sound
)
target_compile_features(rr2nw_arena_wav_resource_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_wav_resource_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_arena_wav_resource_state PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_farter_attribute_state STATIC
  nw/ARENA/OBASE/Farter/FarterAttributeState.cpp
  nw/ARENA/OBASE/Farter/FarterAttributeState.h
  nw/ARENA/OBASE/Farter/FarterAttributeState.inl
)
target_link_libraries(rr2nw_arena_farter_attribute_state PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_wav_resource_state
)
target_include_directories(rr2nw_arena_farter_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Farter
)
target_compile_features(rr2nw_arena_farter_attribute_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_farter_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_corpse_attribute_state STATIC
  nw/ARENA/OBASE/Corpse/CorpseAttributeState.cpp
  nw/ARENA/OBASE/Corpse/CorpseAttributeState.h
  nw/ARENA/OBASE/Corpse/CorpseAttributeState.inl
)
target_link_libraries(rr2nw_arena_corpse_attribute_state PUBLIC
  rr2nw_arena_skin_resource_state
  rr2nw_arena_smoker_attribute_state
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_corpse_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Corpse
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_corpse_attribute_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_corpse_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_lamp_attribute_state STATIC
  nw/ARENA/OBASE/Lamp/LampAttributeState.cpp
  nw/ARENA/OBASE/Lamp/LampAttributeState.h
  nw/ARENA/OBASE/Lamp/LampAttributeState.inl
)
target_link_libraries(rr2nw_arena_lamp_attribute_state PUBLIC
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_storage
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_lamp_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Lamp
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_lamp_attribute_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_lamp_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_farter_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_corpse_attribute_state PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_arena_lamp_attribute_state PRIVATE /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_obase_farter_full STATIC
  nw/ARENA/OBASE/Farter/Farter.cpp
)
target_link_libraries(rr2nw_arena_obase_farter_full PUBLIC
  rr2nw_arena_farter_attribute_state
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_farter_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Farter
)
target_compile_features(rr2nw_arena_obase_farter_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_farter_full PRIVATE
  __NT__
  RR2NW_FARTER_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_corpse_full STATIC
  nw/ARENA/OBASE/Corpse/Corpse.cpp
  nw/ARENA/OBASE/Corpse/CorpseSubjectState.cpp
  nw/ARENA/OBASE/Corpse/CorpseSubjectState.h
  nw/ARENA/OBASE/DynObj/DynObj.cpp
)
target_link_libraries(rr2nw_arena_obase_corpse_full PUBLIC
  rr2nw_arena_corpse_attribute_state
  rr2nw_arena_physics
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_corpse_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Corpse
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_corpse_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_corpse_full PRIVATE
  __NT__
  RR2NW_CORPSE_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_lamp_full STATIC
  nw/ARENA/OBASE/Lamp/LAMP.CPP
)
target_link_libraries(rr2nw_arena_obase_lamp_full PUBLIC
  rr2nw_arena_lamp_attribute_state
  rr2nw_arena_light_chain
  rr2nw_arena_physics
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_lamp_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Lamp
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_lamp_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_lamp_full PRIVATE
  __NT__
  RR2NW_LAMP_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_farter_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_obase_corpse_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_obase_lamp_full PRIVATE /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_obase_smoke_full STATIC
  nw/ARENA/OBASE/Smoke/Smoke.cpp
  nw/ARENA/OBASE/Smoke/SmokeActiveWorldState.h
  nw/ARENA/OBASE/Smoke/SmokeSubjectState.h
)
target_link_libraries(rr2nw_arena_obase_smoke_full PUBLIC
  rr2nw_arena_smoke_attribute_state
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_storage
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_obase_smoke_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_smoke_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_smoke_full PRIVATE
  __NT__
  RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
  RR2NW_SMOKE_TEXTURE_CACHE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_arena_obase_smoke_full PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_smoke_subject_state STATIC
  nw/ARENA/OBASE/Smoke/Smoke.cpp
  nw/ARENA/OBASE/Smoke/SmokeDirectionalStartState.cpp
  nw/ARENA/OBASE/Smoke/SmokeActiveWorldState.h
  nw/ARENA/OBASE/Smoke/SmokeSubjectState.h
)
target_link_libraries(rr2nw_arena_smoke_subject_state PUBLIC
  rr2nw_arena_smoke_attribute_state
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_storage
  rr2nw_graph_runtime_state
  rr2nw_graph_software_texture
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
  rr2nw_view_object_state
  rr2nw_scene_runtime_core
)
target_include_directories(rr2nw_arena_smoke_subject_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_smoke_subject_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_smoke_subject_state PRIVATE
  __NT__
  RR2NW_SMOKE_ATTRIBUTE_STATE_EXTERNAL
  RR2NW_SMOKE_TEXTURE_CACHE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_smoke_subject_state PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_obase_smoker_full STATIC
  nw/ARENA/OBASE/Smoke/SMOKER.CPP
  nw/ARENA/OBASE/Smoke/SmokerSubjectState.h
)
target_link_libraries(rr2nw_arena_obase_smoker_full PUBLIC
  rr2nw_arena_light_chain
  rr2nw_arena_obase_smoke_full
  rr2nw_arena_physics
  rr2nw_arena_smoker_attribute_state
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_obase_smoker_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_smoker_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_smoker_full PRIVATE
  __NT__
  RR2NW_SMOKER_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_smoker_subject_state STATIC
  nw/ARENA/OBASE/DynObj/DynCorona.cpp
  nw/ARENA/OBASE/Smoke/SMOKER.CPP
  nw/ARENA/OBASE/Smoke/SmokerSubjectState.h
)
target_link_libraries(rr2nw_arena_smoker_subject_state PUBLIC
  rr2nw_arena_light_chain_core
  rr2nw_arena_smoke_subject_state
  rr2nw_arena_smoker_attribute_state
  rr2nw_arena_storage
  rr2nw_graph_software_texture
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
  rr2nw_view_object_state
)
target_include_directories(rr2nw_arena_smoker_subject_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Smoke
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_smoker_subject_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_smoker_subject_state PRIVATE
  __NT__
  RR2NW_SMOKER_ATTRIBUTE_STATE_EXTERNAL
  RR2NW_SMOKER_SUBJECT_ONLY
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_obase_wav_full STATIC
  nw/ARENA/OBASE/Sound/Wavobj.cpp
)
target_link_libraries(rr2nw_arena_obase_wav_full PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_wav_resource_state
  rr2nw_sound_state
)
target_include_directories(rr2nw_arena_obase_wav_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Sound
)
target_compile_features(rr2nw_arena_obase_wav_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_wav_full PRIVATE
  __NT__
  RR2NW_WAV_RESOURCE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_smoker_full PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_smoker_subject_state PRIVATE
    /W4 /permissive- /wd4081 /wd4189 /wd4244
  )
  target_compile_options(
    rr2nw_arena_obase_wav_full PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_obase_soundobj_full STATIC
  nw/ARENA/OBASE/Sound/SoundObj.cpp
  nw/ARENA/OBASE/Sound/SoundObj.h
  nw/ARENA/OBASE/Sound/SoundObjectState.h
)
target_link_libraries(rr2nw_arena_obase_soundobj_full PUBLIC
  rr2nw_arena_obase_wav_full
  rr2nw_arena_storage
  rr2nw_legacy_rsx_ids
  rr2nw_sound_state
  ole32
)
target_include_directories(rr2nw_arena_obase_soundobj_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Sound
)
target_compile_features(rr2nw_arena_obase_soundobj_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_soundobj_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_arena_sound_object_state STATIC
  nw/ARENA/OBASE/Sound/SoundObj.cpp
  nw/ARENA/OBASE/Sound/SoundObj.h
  nw/ARENA/OBASE/Sound/SoundObjectState.h
)
target_link_libraries(rr2nw_arena_sound_object_state PUBLIC
  rr2nw_arena_obase_wav_full
  rr2nw_arena_storage
  rr2nw_sound_state
)
target_include_directories(rr2nw_arena_sound_object_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Sound
)
target_compile_features(rr2nw_arena_sound_object_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_sound_object_state PRIVATE
  __NT__
  RR2NW_SOUNDOBJ_DEVICE_FREE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_soundobj_full PRIVATE
    /W3 /permissive- /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_arena_sound_object_state PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_bullet_attribute_state STATIC
  nw/ARENA/OBASE/Bullet/BulletActiveWorldState.h
  nw/ARENA/OBASE/Bullet/BulletAttributeState.cpp
  nw/ARENA/OBASE/Bullet/BulletAttributeState.h
  nw/ARENA/OBASE/Bullet/BulletSubjectState.cpp
  nw/ARENA/OBASE/Bullet/BulletSubjectState.h
)
target_link_libraries(rr2nw_arena_bullet_attribute_state PUBLIC
  rr2nw_arena_explosion_attribute_state
  rr2nw_arena_obase_spark_full
  rr2nw_arena_skin_resource_state
  rr2nw_arena_smoke_subject_state
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_storage
  rr2nw_arena_wav_resource_state
  rr2nw_graph_runtime_state
)
target_include_directories(rr2nw_arena_bullet_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Bullet
  nw/ARENA/OBASE/Skin
  nw/ARENA/OBASE/Sound
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_arena_bullet_attribute_state PUBLIC cxx_std_14
)
target_compile_definitions(
  rr2nw_arena_bullet_attribute_state PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_arena_bullet_attribute_state PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_farter_subject_state STATIC
  nw/ARENA/OBASE/Farter/Farter.cpp
  nw/ARENA/OBASE/Farter/Farter.h
  nw/ARENA/OBASE/Farter/FarterSubjectState.h
)
target_link_libraries(rr2nw_arena_farter_subject_state PUBLIC
  rr2nw_arena_farter_attribute_state
  rr2nw_arena_sound_object_state
  rr2nw_arena_storage
  rr2nw_view_dynamic_waste_state
)
target_include_directories(rr2nw_arena_farter_subject_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Farter
  nw/ARENA/OBASE/Sound
)
target_compile_features(rr2nw_arena_farter_subject_state PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_farter_subject_state PRIVATE
  __NT__
  RR2NW_FARTER_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_arena_farter_subject_state PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_menu_full STATIC
  nw/ARENA/OBASE/MENU/MENU.CPP
  nw/ARENA/OBASE/MENU/MenuGlobal.cpp
  nw/ARENA/OBASE/MENU/MenuGlobal.inl
)
target_link_libraries(rr2nw_arena_menu_full PUBLIC
  rr2nw_arena_smoke_texture_cache
  rr2nw_arena_sua_shutdown_state
  rr2nw_arena_storage
  rr2nw_arena_level_state
  rr2nw_arena_time_runtime_state
  rr2nw_graph_runtime_state
  rr2nw_graph_image_runtime_state
  rr2nw_graph_fixed_font_runtime_state
  rr2nw_main_zav_scene_state
  rr2nw_main_zav_shutdown_state
)
target_include_directories(rr2nw_arena_menu_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/MENU
  nw/BRIEFING
  nw/GRAPH
  nw/MAIN
  nw/SCENE
)
target_compile_features(rr2nw_arena_menu_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_menu_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_arena_menu_full PRIVATE
    /W3 /permissive- /wd4018 /wd4081 /wd4244
  )
endif()

add_library(rr2nw_legacy_shell_full STATIC
  nw/ARENA/HARDWARE.cpp
  nw/ARENA/ShellGlobals.cpp
  nw/ARENA/ShellGlobals.inl
  nw/BRIEFING/CONSOLE.CPP
  nw/BRIEFING/COMMANDS.CPP
  nw/BRIEFING/BRIEFING.CPP
)
target_link_libraries(rr2nw_legacy_shell_full PUBLIC
  rr2nw_arena_menu_full
  rr2nw_arena_const_string
  rr2nw_briefing_channel_map
  rr2nw_arena_storage
  rr2nw_arena_level_state
  rr2nw_arena_time_runtime_state
  rr2nw_briefing_god_mode_state
  rr2nw_graph_runtime_state
  rr2nw_graph_image_runtime_state
  rr2nw_graph_fixed_font_runtime_state
  rr2nw_graph_panel_lifecycle
  rr2nw_frame_runtime
  rr2nw_main_zav_scene_state
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
  winmm
)
target_include_directories(rr2nw_legacy_shell_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BRIEFING
  nw/GRAPH
  nw/MAIN
  nw/SCENE
)
target_compile_features(rr2nw_legacy_shell_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_legacy_shell_full PRIVATE
  __NT__
  RR2NW_GOD_MODE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_legacy_shell_full PRIVATE
    /W3 /permissive- /wd4018 /wd4081 /wd4244
  )
endif()

add_library(rr2nw_mainproc_full STATIC
  nw/MAIN/mainproc.cpp
)
target_link_libraries(rr2nw_mainproc_full PUBLIC
  rr2nw_arena_obase_debug_map_mission
  rr2nw_arena_obase_vehicle
  rr2nw_arena_obase_fountain_state
  rr2nw_game_entry_runtime
  rr2nw_legacy_shell_full
  rr2nw_recovered_software_frame
  rr2nw_supervisor_global
)
target_include_directories(rr2nw_mainproc_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BRIEFING
  nw/GRAPH
  nw/MAIN
  nw/SCENE
)
target_compile_features(rr2nw_mainproc_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_mainproc_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_mainproc_full PRIVATE
    /W3 /permissive- /wd4018 /wd4081 /wd4244
  )
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

add_library(rr2nw_arena_obase_debug_map_mission STATIC
  nw/ARENA/OBASE/DMAP/DebugMapMissionState.cpp
  nw/ARENA/OBASE/DMAP/DebugMapMissionState.inl
  nw/ARENA/OBASE/DMAP/DebugMapMissionEvents.inl
  nw/ARENA/OBASE/DMAP/DebugMapMissionContent.cpp
  nw/ARENA/OBASE/DMAP/DebugMapMissionContent.inl
  nw/ARENA/OBASE/DMAP/DebugMapGlobal.inl
)
target_link_libraries(rr2nw_arena_obase_debug_map_mission PUBLIC
  rr2nw_arena_obase_route
)
target_include_directories(rr2nw_arena_obase_debug_map_mission PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_debug_map_mission PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_debug_map_mission PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_debug_map_mission PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_debug_map STATIC
  nw/ARENA/OBASE/DMAP/dmap.cpp
)
target_link_libraries(rr2nw_arena_obase_debug_map PUBLIC
  rr2nw_arena_obase_debug_map_mission
)
target_include_directories(rr2nw_arena_obase_debug_map PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_debug_map PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_debug_map PRIVATE
  __NT__
  RR2NW_DMAP_MISSION_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_debug_map PRIVATE /W3 /permissive- /wd4081
  )
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
  nw/ARENA/OBASE/Vehicle/VehicleVesselTelemetry.cpp
  nw/ARENA/OBASE/Vehicle/VehicleVesselTelemetry.h
  nw/ARENA/OBASE/Vehicle/VehicleVesselSaveState.h
  nw/ARENA/OBASE/Vehicle/VehicleGameplayTuning.cpp
  nw/ARENA/OBASE/Vehicle/VehicleGameplayTuning.h
  nw/ARENA/OBASE/Vehicle/VehicleAttributeState.cpp
  nw/ARENA/OBASE/Vehicle/VehicleAttributeState.h
  nw/ARENA/OBASE/Vehicle/VehicleRuntimeState.cpp
  nw/ARENA/OBASE/Vehicle/VehicleRuntimeState.h
  nw/ARENA/OBASE/Vehicle/VehicleActiveWorldState.cpp
  nw/ARENA/OBASE/Vehicle/VehicleActiveWorldState.h
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
  RR2NW_VESSEL_RUNTIME_GLOBALS_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_vehicle PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_player STATIC
  nw/ARENA/OBASE/Player/PLAYER.CPP
)
target_link_libraries(rr2nw_arena_obase_player PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_mproj
  rr2nw_arena_level_state
  rr2nw_arena_obase_debug_map_mission
)
target_include_directories(rr2nw_arena_obase_player PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_player PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_player PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_player PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_carrier STATIC
  nw/ARENA/OBASE/Artefact/Carrier.cpp
  nw/ARENA/OBASE/Artefact/Carrier.inl
)
target_link_libraries(rr2nw_arena_obase_carrier PUBLIC rr2nw_arena_storage)
target_include_directories(rr2nw_arena_obase_carrier PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_carrier PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_carrier PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_carrier PRIVATE /W3 /permissive- /wd4081
  )
endif()

add_library(rr2nw_arena_obase_artefact STATIC
  nw/ARENA/OBASE/Artefact/Artefact.cpp
)
target_link_libraries(
  rr2nw_arena_obase_artefact PUBLIC
  rr2nw_arena_artefact_attribute_state
  rr2nw_arena_obase_carrier
)
target_include_directories(rr2nw_arena_obase_artefact PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_arena_obase_artefact PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_artefact PRIVATE
  __NT__
  RR2NW_ARTEFACT_ATTRIBUTE_STATE_EXTERNAL
  RR2NW_CARRIER_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_arena_obase_artefact PRIVATE /W3 /permissive- /wd4081
  )
endif()

target_link_libraries(rr2nw_arena_obase_vehicle PUBLIC
  rr2nw_arena_obase_player
  rr2nw_arena_obase_carrier
  rr2nw_arena_physics
  rr2nw_briefing_god_mode_state
  rr2nw_view_object_state
  rr2nw_view_figure_state
  rr2nw_view_terrain_waterline_state
  rr2nw_main_zav_scene_state
  rr2nw_arena_script_viewpoints
  rr2nw_scene_vessel_runtime_state
  rr2nw_scene_draw_state
  rr2nw_graph_runtime_state
  rr2nw_graph_panel_lifecycle
  rr2nw_legacy_rsx_ids
  rr2nw_arena_time_runtime_state
  rr2nw_arena_taxi_attribute_state
)

add_library(rr2nw_arena_obase_taxi_full STATIC
  nw/ARENA/OBASE/Taxi/Taxi.cpp
  nw/ARENA/OBASE/Taxi/Taxi.h
  nw/ARENA/OBASE/Taxi/TaxiSubjectState.h
)
target_link_libraries(rr2nw_arena_obase_taxi_full PUBLIC
  rr2nw_arena_taxi_attribute_state
  rr2nw_arena_obase_vehicle
  rr2nw_arena_obase_corpse_full
  rr2nw_arena_sound_object_state
)
target_include_directories(rr2nw_arena_obase_taxi_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Taxi
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_taxi_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_taxi_full PRIVATE
  __NT__
  RR2NW_TAXI_ATTRIBUTE_STATE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_obase_taxi_full PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_obase_people_full STATIC
  nw/ARENA/OBASE/People/PEOPLE.CPP
  nw/ARENA/OBASE/People/PEOPLE.H
  nw/ARENA/OBASE/People/PDY_OBJ.CPP
  nw/ARENA/OBASE/People/PeopleSubjectState.cpp
  nw/ARENA/OBASE/People/PeopleSubjectState.h
  nw/ARENA/OBASE/People/PeopleActiveWorldState.cpp
  nw/ARENA/OBASE/People/PeopleActiveWorldState.h
)
target_link_libraries(rr2nw_arena_obase_people_full PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_obase_route
  rr2nw_arena_physics
  rr2nw_arena_bullet_attribute_state
  rr2nw_arena_explosion_attribute_state
  rr2nw_arena_obase_corpse_full
  rr2nw_arena_sound_object_state
  rr2nw_arena_skin_resource_state
  rr2nw_scene_draw_state
)
target_include_directories(rr2nw_arena_obase_people_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/People
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_people_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_people_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_obase_people_full PRIVATE
    /W4 /permissive- /wd4081 /wd4244
  )
endif()

add_library(rr2nw_arena_obase_cannon_full STATIC
  nw/ARENA/OBASE/Cannon/Cannon.cpp
  nw/ARENA/OBASE/Cannon/Cannon.h
  nw/ARENA/OBASE/Cannon/CannonSubjectState.cpp
  nw/ARENA/OBASE/Cannon/CannonSubjectState.h
)
target_link_libraries(rr2nw_arena_obase_cannon_full PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_physics
  rr2nw_arena_bullet_attribute_state
  rr2nw_arena_sound_object_state
  rr2nw_scene_draw_state
)
target_include_directories(rr2nw_arena_obase_cannon_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Cannon
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_cannon_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_cannon_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_obase_cannon_full PRIVATE
    /W4 /permissive /wd4081 /wd4244 /wd4867
  )
endif()

add_library(rr2nw_arena_obase_tank_full STATIC
  nw/ARENA/OBASE/Tank/TANK.CPP
  nw/ARENA/OBASE/Tank/TANK.H
  nw/ARENA/OBASE/Tank/TANK_2.CPP
  nw/ARENA/OBASE/Tank/TANK_3.CPP
  nw/ARENA/OBASE/Tank/T_DYNOBJ.CPP
  nw/ARENA/OBASE/Tank/Nearai.cpp
  nw/ARENA/OBASE/Tank/NEARAI.H
  nw/ARENA/OBASE/Tank/TankSubjectState.cpp
  nw/ARENA/OBASE/Tank/TankSubjectState.h
  nw/ARENA/OBASE/Tank/TankActiveWorldState.cpp
  nw/ARENA/OBASE/Tank/TankActiveWorldState.h
)
target_link_libraries(rr2nw_arena_obase_tank_full PUBLIC
  rr2nw_simulation_random
  rr2nw_arena_storage
  rr2nw_arena_physics
  rr2nw_arena_obase_cannon_full
  rr2nw_arena_bullet_attribute_state
  rr2nw_arena_explosion_attribute_state
  rr2nw_arena_obase_corpse_full
  rr2nw_arena_sound_object_state
  rr2nw_arena_skin_resource_state
  rr2nw_scene_draw_state
)
target_include_directories(rr2nw_arena_obase_tank_full PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Tank
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_arena_obase_tank_full PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_arena_obase_tank_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_obase_tank_full PRIVATE
    /W3 /permissive /wd4081 /wd4244 /wd4867
    /FITankSimulationRandomShim.h
  )
endif()

add_library(rr2nw_arena_obase_commander_group_full STATIC
  nw/ARENA/OBASE/Comander/Comander.cpp
  nw/ARENA/OBASE/Comander/CommanderState.h
  nw/ARENA/OBASE/GROUP/GROUP.CPP
  nw/ARENA/OBASE/GROUP/GROUP.H
  nw/ARENA/OBASE/GROUP/GROUP_2.CPP
  nw/ARENA/OBASE/GROUP/TankGroupState.h
)
target_link_libraries(rr2nw_arena_obase_commander_group_full PUBLIC
  rr2nw_arena_storage
  rr2nw_arena_obase_route
  rr2nw_arena_obase_tank_full
  rr2nw_arena_physics
  rr2nw_scene_draw_state
)
target_include_directories(rr2nw_arena_obase_commander_group_full PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/ARENA/OBASE/Comander
  nw/ARENA/OBASE/GROUP
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_arena_obase_commander_group_full PUBLIC cxx_std_14
)
target_compile_definitions(
  rr2nw_arena_obase_commander_group_full PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_arena_obase_commander_group_full PRIVATE
    /W3 /permissive /wd4081 /wd4244 /wd4867
  )
endif()

add_library(rr2nw_game_entry_runtime STATIC
  src/runtime/GameEntryRuntimeState.cpp
  src/runtime/GameEntryRuntimeState.h
  src/runtime/RecoveredGameEntryBindings.cpp
  src/runtime/RecoveredLevelAssets.cpp
  src/runtime/RecoveredLevelAssets.h
  src/runtime/RecoveredLevelRuntime.cpp
  src/runtime/RecoveredLevelRuntime.h
  src/runtime/RecoveredSoftwareGraph.cpp
  src/runtime/RecoveredSoftwareGraph.h
)
target_link_libraries(rr2nw_game_entry_runtime PUBLIC
  rr2nw_arena_kernel
  rr2nw_graph_fixed_font_runtime_state
  rr2nw_graph_runtime_state
  rr2nw_legacy_design
  rr2nw_mod_runtime
  rr2nw_main_zav_shutdown_state
  rr2nw_patterns_palette_runtime
  rr2nw_view_figure_library_state
  rr2nw_view_frame_state
  rr2nw_view_object_state
  rr2nw_view_terrain_waterline_state
  gdi32
  user32
)
target_include_directories(rr2nw_game_entry_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/BRIEFING
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_game_entry_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_game_entry_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_recovered_terrain_runtime STATIC
  src/runtime/RecoveredTerrainRuntime.cpp
  src/runtime/RecoveredTerrainRuntime.h
)
target_link_libraries(rr2nw_recovered_terrain_runtime PUBLIC
  rr2nw_game_entry_runtime
  rr2nw_view_terrain_decoder
)
target_include_directories(rr2nw_recovered_terrain_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_recovered_terrain_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_recovered_terrain_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_recovered_terrain_runtime PRIVATE /W4 /permissive- /wd4081
  )
endif()

add_library(rr2nw_recovered_scene_order_runtime STATIC
  src/runtime/RecoveredSceneOrderRuntime.cpp
  src/runtime/RecoveredSceneOrderRuntime.h
)
target_link_libraries(rr2nw_recovered_scene_order_runtime PUBLIC
  rr2nw_game_entry_runtime
  rr2nw_recovered_terrain_runtime
  rr2nw_view_land_object_decoder
  rr2nw_view_order_decode_state
)
target_include_directories(rr2nw_recovered_scene_order_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/GRAPH
  nw/VIEW
)
target_compile_features(rr2nw_recovered_scene_order_runtime PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_recovered_scene_order_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(
    rr2nw_recovered_scene_order_runtime PRIVATE /W4 /permissive- /wd4081
  )
endif()

add_library(rr2nw_scene_runtime_core STATIC
  nw/SCENE/SCENE.CPP
)
target_link_libraries(rr2nw_scene_runtime_core PUBLIC
  rr2nw_scene_software_draw
  rr2nw_view_land_object_full
  rr2nw_view_object_decoder
  rr2nw_view_terrain_full
)
target_include_directories(rr2nw_scene_runtime_core PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_scene_runtime_core PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_scene_runtime_core PRIVATE
  __NT__
  RR2NW_SCENE_DRAW_EXTERNAL
  RR2NW_SCENE_NO_HARDWARE_PUMP
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_recovered_drawable_scene_runtime STATIC
  src/runtime/RecoveredDrawableSceneRuntime.cpp
  src/runtime/RecoveredDrawableSceneRuntime.h
)
target_link_libraries(rr2nw_recovered_drawable_scene_runtime PUBLIC
  rr2nw_game_entry_runtime
  rr2nw_main_zav_scene_state
  rr2nw_scene_runtime_core
)
target_include_directories(rr2nw_recovered_drawable_scene_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_recovered_drawable_scene_runtime PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_recovered_drawable_scene_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_recovered_retail_script_manifest STATIC
  src/runtime/RecoveredRetailScriptManifest.cpp
  src/runtime/RecoveredRetailScriptManifest.h
)
target_link_libraries(rr2nw_recovered_retail_script_manifest PUBLIC
  rr2nw_mod_runtime
)
target_include_directories(rr2nw_recovered_retail_script_manifest PUBLIC
  src/runtime
)
target_compile_features(
  rr2nw_recovered_retail_script_manifest PUBLIC cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_retail_script_manifest PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_recovered_retail_script_manifest PRIVATE /W4 /permissive-
  )
endif()

add_library(rr2nw_recovered_game_level_runtime STATIC
  src/runtime/RecoveredGameLevelRuntime.cpp
  src/runtime/RecoveredGameLevelRuntime.h
)
target_link_libraries(rr2nw_recovered_game_level_runtime PUBLIC
  rr2nw_recovered_drawable_scene_runtime
  rr2nw_recovered_retail_script_manifest
)
target_include_directories(rr2nw_recovered_game_level_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_recovered_game_level_runtime PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_recovered_game_level_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_recovered_legacy_script_runtime STATIC
  src/runtime/RecoveredLegacyScriptHost.cpp
  src/runtime/RecoveredLegacyScriptHost.h
  src/runtime/RecoveredLegacyScriptRunner.cpp
  src/runtime/RecoveredLegacyScriptRunner.h
)
target_link_libraries(rr2nw_recovered_legacy_script_runtime PUBLIC
  rr2nw_arena_obase_route
  rr2nw_arena_storage
  rr2nw_arena_time_runtime_state
  rr2nw_script_runtime
)
target_include_directories(rr2nw_recovered_legacy_script_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(
  rr2nw_recovered_legacy_script_runtime PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_recovered_legacy_script_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_active_world_save STATIC
  src/runtime/ActiveWorldSave.cpp
  src/runtime/ActiveWorldSave.h
)
target_include_directories(rr2nw_active_world_save PUBLIC src/runtime)
target_compile_features(rr2nw_active_world_save PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_active_world_save PRIVATE UNICODE _UNICODE)

add_library(rr2nw_recovered_game_services_runtime STATIC
  nw/ARENA/OBASE/Corpse/CorpseActiveWorldState.cpp
  nw/ARENA/OBASE/Corpse/CorpseActiveWorldState.h
  src/runtime/ActiveWorldSemanticEvents.cpp
  src/runtime/ActiveWorldSemanticEvents.h
  src/runtime/ClockActiveWorldState.cpp
  src/runtime/ClockActiveWorldState.h
  src/runtime/IndexedPng.cpp
  src/runtime/IndexedPng.h
  src/runtime/LevelContinuation.cpp
  src/runtime/LevelContinuation.h
  src/runtime/LevelSaveSlot.cpp
  src/runtime/LevelSaveSlot.h
  src/runtime/MissionActiveWorldState.cpp
  src/runtime/MissionActiveWorldState.h
  src/runtime/ActiveWorldRuntimeProbe.cpp
  src/runtime/ActiveWorldRuntimeProbe.h
  src/runtime/RecoveredArenaSeanceRuntime.cpp
  src/runtime/RecoveredArenaSeanceRuntime.h
  src/runtime/RecoveredFramePreview.cpp
  src/runtime/RecoveredFramePreview.h
  src/runtime/RecoveredGameServicesRuntime.cpp
  src/runtime/RecoveredGameServicesRuntime.h
  src/runtime/RecoveredGameplayTuningRuntime.cpp
  src/runtime/RecoveredGameplayTuningRuntime.h
  src/runtime/RecoveredScriptEventRuntime.cpp
  src/runtime/RecoveredScriptEventRuntime.h
  src/runtime/RecoveredSavePreview.cpp
  src/runtime/RecoveredSavePreview.h
  src/runtime/RecoveredSaveSlotDialog.cpp
  src/runtime/RecoveredSaveSlotDialog.h
  src/runtime/VehicleControlJournal.cpp
  src/runtime/VehicleControlJournal.h
  src/runtime/VehicleControlReplayProbe.cpp
  src/runtime/VehicleControlReplayProbe.h
)
target_link_libraries(rr2nw_recovered_game_services_runtime PUBLIC
  rr2nw_active_world_save
  rr2nw_mod_runtime
  rr2nw_arena_obase_debug_map_mission
  rr2nw_arena_sua_shutdown_state
  rr2nw_arena_time_runtime_state
  rr2nw_arena_obase_vehicle
  rr2nw_arena_obase_taxi_full
  rr2nw_arena_obase_people_full
  rr2nw_arena_obase_cannon_full
  rr2nw_arena_obase_tank_full
  rr2nw_arena_obase_commander_group_full
  rr2nw_arena_bird_attribute_state
  rr2nw_arena_artefact_attribute_state
  rr2nw_arena_obase_orphan_full
  rr2nw_arena_obase_portal
  rr2nw_arena_bullet_attribute_state
  rr2nw_arena_explosion_attribute_state
  rr2nw_arena_farter_attribute_state
  rr2nw_arena_lamp_attribute_state
  rr2nw_arena_corpse_attribute_state
  rr2nw_arena_obase_corpse_full
  rr2nw_arena_smoker_attribute_state
  rr2nw_arena_smoker_subject_state
  rr2nw_arena_smoke_subject_state
  rr2nw_arena_smoke_visual_state
  rr2nw_arena_wav_resource_state
  rr2nw_arena_sound_object_state
  rr2nw_arena_farter_subject_state
  rr2nw_arena_skin_resource_state
  rr2nw_arena_smoke_attribute_state
  rr2nw_arena_spark_attribute_state
  rr2nw_arena_obase_spark_full
  rr2nw_legacy_shell_full
  rr2nw_recovered_legacy_script_runtime
  rr2nw_recovered_skin_resource_catalog
  rr2nw_recovered_wav_metadata_catalog
  rr2nw_recovered_game_level_runtime
  rr2nw_recovered_software_frame
  rr2nw_supervisor_global
  ole32
  gdi32
  shell32
  user32
  windowscodecs
)
target_include_directories(rr2nw_recovered_game_services_runtime PUBLIC
  src/runtime
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
  nw/GRAPH
  nw/MAIN
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_recovered_game_services_runtime PUBLIC cxx_std_14
)
target_compile_definitions(rr2nw_recovered_game_services_runtime PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_active_world_save PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_scene_runtime_core PRIVATE /W3 /permissive- /Gy /wd4081 /wd4244
  )
  target_compile_options(
    rr2nw_recovered_drawable_scene_runtime PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_game_level_runtime PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_legacy_script_runtime PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_game_services_runtime PRIVATE /W4 /permissive- /wd4081
  )
endif()

add_library(rr2nw_supervisor_global STATIC
  src/runtime/SupervisorGlobal.cpp
)
target_link_libraries(rr2nw_supervisor_global PUBLIC
  rr2nw_arena_kernel
  rr2nw_arena_obase_publish_full
  rr2nw_arena_storage
  rr2nw_game_entry_runtime
)
target_include_directories(rr2nw_supervisor_global PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
)
target_compile_features(rr2nw_supervisor_global PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_supervisor_global PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

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

add_executable(rr2nw_config_smoke
  src/portability/ConfigSmoke.cpp
)
target_link_libraries(rr2nw_config_smoke PRIVATE rr2nw_legacy_design)
target_compile_features(rr2nw_config_smoke PRIVATE cxx_std_14)

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

add_executable(rr2nw_publisher_lifecycle_smoke
  src/portability/PublisherLifecycleSmoke.cpp
)
target_link_libraries(
  rr2nw_publisher_lifecycle_smoke PRIVATE rr2nw_arena_obase_publish_full
)
target_compile_features(rr2nw_publisher_lifecycle_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_publisher_lifecycle_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_storage_save_smoke
  src/portability/StorageSaveSmoke.cpp
)
target_link_libraries(rr2nw_storage_save_smoke PRIVATE rr2nw_arena_storage)
target_compile_features(rr2nw_storage_save_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_active_world_save_smoke
  src/portability/ActiveWorldSaveSmoke.cpp
)
target_link_libraries(
  rr2nw_active_world_save_smoke PRIVATE rr2nw_active_world_save
)
target_compile_features(rr2nw_active_world_save_smoke PRIVATE cxx_std_14)
target_compile_definitions(
  rr2nw_active_world_save_smoke PRIVATE UNICODE _UNICODE
)

add_executable(rr2nw_simulation_continuation_smoke
  src/portability/SimulationContinuationSmoke.cpp
)
target_link_libraries(rr2nw_simulation_continuation_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_simulation_continuation_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_simulation_continuation_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_context_route_smoke
  src/portability/ContextRouteSmoke.cpp
)
target_link_libraries(rr2nw_context_route_smoke PRIVATE rr2nw_arena_obase_route)
target_compile_features(rr2nw_context_route_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_mproj_smoke
  src/portability/MprojSmoke.cpp
)
target_link_libraries(rr2nw_mproj_smoke PRIVATE rr2nw_arena_mproj)
target_include_directories(rr2nw_mproj_smoke PRIVATE nw/OUTPUT)
target_compile_features(rr2nw_mproj_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_level_state_smoke
  src/portability/LevelStateSmoke.cpp
)
target_link_libraries(rr2nw_level_state_smoke PRIVATE rr2nw_arena_level_state)
target_compile_features(rr2nw_level_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_level_state_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_const_string_smoke
  src/portability/ConstStringSmoke.cpp
)
target_link_libraries(rr2nw_const_string_smoke PRIVATE
  rr2nw_arena_const_string
)
target_compile_features(rr2nw_const_string_smoke PRIVATE cxx_std_14)

add_executable(rr2nw_channel_map_smoke
  src/portability/ChannelMapSmoke.cpp
)
target_link_libraries(rr2nw_channel_map_smoke PRIVATE
  rr2nw_briefing_channel_map
)
target_compile_features(rr2nw_channel_map_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_channel_map_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_debug_map_mission_smoke
  src/portability/DebugMapMissionSmoke.cpp
)
target_link_libraries(
  rr2nw_debug_map_mission_smoke PRIVATE rr2nw_arena_obase_debug_map_mission
)
target_compile_features(rr2nw_debug_map_mission_smoke PRIVATE cxx_std_14)

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

add_executable(rr2nw_vehicle_services_smoke
  src/portability/VehicleServicesSmoke.cpp
)
target_link_libraries(rr2nw_vehicle_services_smoke PRIVATE
  rr2nw_legacy_rsx_ids
  rr2nw_arena_time_runtime_state
  rr2nw_arena_taxi_attribute_state
)
target_compile_features(rr2nw_vehicle_services_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_vehicle_services_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
target_link_libraries(
  rr2nw_vehicle_state_smoke PRIVATE rr2nw_arena_obase_vehicle_state
)
target_compile_features(rr2nw_vehicle_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_vehicle_state_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_physics_core_smoke
  src/portability/PhysicsCoreSmoke.cpp
)
target_link_libraries(rr2nw_physics_core_smoke PRIVATE
  rr2nw_arena_physics_math
  rr2nw_view_dynamic_collision
  rr2nw_briefing_god_mode_state
)
target_compile_features(rr2nw_physics_core_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_physics_core_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_view_state_smoke
  src/portability/ViewStateSmoke.cpp
)
target_link_libraries(rr2nw_view_state_smoke PRIVATE
  rr2nw_view_object_state
  rr2nw_view_figure_state
  rr2nw_view_terrain_waterline_state
  rr2nw_main_zav_scene_state
  rr2nw_arena_script_viewpoints
)
target_compile_features(rr2nw_view_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_view_state_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_scene_draw_smoke
  src/portability/SceneDrawSmoke.cpp
)
target_link_libraries(rr2nw_scene_draw_smoke PRIVATE
  rr2nw_scene_draw_state
  rr2nw_scene_vessel_runtime_state
  rr2nw_view_object_state
)
target_compile_features(rr2nw_scene_draw_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_scene_draw_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_graph_runtime_smoke
  src/portability/GraphRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_graph_runtime_smoke PRIVATE
  rr2nw_graph_image_runtime_state
  rr2nw_graph_runtime_state
)
target_compile_features(rr2nw_graph_runtime_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_graph_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_software_polygon_rasterizer_smoke
  src/portability/SoftwarePolygonRasterizerSmoke.cpp
)
target_link_libraries(rr2nw_software_polygon_rasterizer_smoke PRIVATE
  rr2nw_graph_software_texture
)
target_compile_features(
  rr2nw_software_polygon_rasterizer_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_software_polygon_rasterizer_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_frame_runtime_smoke
  src/portability/FrameRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_frame_runtime_smoke PRIVATE rr2nw_frame_runtime)
target_include_directories(rr2nw_frame_runtime_smoke PRIVATE
  nw/ARENA
  nw/ARENA/H
  nw/SCENE
  nw/VIEW
)
target_compile_features(rr2nw_frame_runtime_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_frame_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_software_frame_smoke
  src/portability/RecoveredSoftwareFrameSmoke.cpp
)
target_link_libraries(rr2nw_recovered_software_frame_smoke PRIVATE
  rr2nw_arena_time_runtime_state
  rr2nw_recovered_software_frame
)
target_include_directories(rr2nw_recovered_software_frame_smoke PRIVATE
  nw/ARENA
  nw/ARENA/H
  nw/SCENE
  nw/VIEW
)
target_compile_features(
  rr2nw_recovered_software_frame_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_recovered_software_frame_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_scene_software_state_smoke
  src/portability/SceneSoftwareStateSmoke.cpp
)
target_link_libraries(rr2nw_scene_software_state_smoke PRIVATE
  rr2nw_arena_light_chain
  rr2nw_view_light_state
  rr2nw_view_frame_state
  rr2nw_view_land_dynamic_frame_state
)
target_include_directories(rr2nw_scene_software_state_smoke PRIVATE
  nw/ARENA
  nw/ARENA/H
  nw/PATTERNS
  nw/VIEW
)
target_compile_features(
  rr2nw_scene_software_state_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_scene_software_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_terrain_view_state_smoke
  src/portability/TerrainViewStateSmoke.cpp
)
target_link_libraries(rr2nw_terrain_view_state_smoke PRIVATE
  rr2nw_view_terrain_view_state
)
target_include_directories(rr2nw_terrain_view_state_smoke PRIVATE
  nw/VIEW
)
target_compile_features(rr2nw_terrain_view_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_terrain_view_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
add_executable(rr2nw_menu_texture_smoke
  src/portability/MenuTextureSmoke.cpp
)
target_link_libraries(rr2nw_menu_texture_smoke PRIVATE
  rr2nw_arena_smoke_texture_cache
)
target_compile_features(rr2nw_menu_texture_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_menu_texture_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_menu_shutdown_smoke
  src/portability/MenuShutdownSmoke.cpp
)
target_link_libraries(rr2nw_menu_shutdown_smoke PRIVATE
  rr2nw_arena_sua_shutdown_state
  rr2nw_main_zav_shutdown_state
)
target_compile_features(rr2nw_menu_shutdown_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_menu_shutdown_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_panel_lifecycle_smoke
  src/portability/PanelLifecycleSmoke.cpp
)
target_link_libraries(rr2nw_panel_lifecycle_smoke PRIVATE
  rr2nw_graph_panel_lifecycle
)
target_compile_features(rr2nw_panel_lifecycle_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_panel_lifecycle_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_vehicle_link_probe EXCLUDE_FROM_ALL
  src/portability/VehicleLinkProbe.cpp
)
target_link_libraries(
  rr2nw_vehicle_link_probe PRIVATE rr2nw_arena_obase_vehicle
)
target_compile_features(rr2nw_vehicle_link_probe PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_vehicle_link_probe PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_vehicle_shell_link_probe EXCLUDE_FROM_ALL
  src/portability/VehicleLinkProbe.cpp
)
target_link_libraries(rr2nw_vehicle_shell_link_probe PRIVATE
  rr2nw_arena_obase_vehicle
  rr2nw_legacy_shell_full
)
target_compile_features(rr2nw_vehicle_shell_link_probe PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_vehicle_shell_link_probe PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_game_link_probe WIN32
  nw/MAIN/mainproc.cpp
)
target_link_libraries(rr2nw_game_link_probe PRIVATE
  rr2nw_mainproc_full
)
target_compile_features(rr2nw_game_link_probe PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_game_link_probe PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_game_entry_runtime_smoke
  src/portability/GameEntryRuntimeSmoke.cpp
)

add_executable(rr2nw_recovered_software_graph_smoke
  src/portability/RecoveredSoftwareGraphSmoke.cpp
)
target_link_libraries(rr2nw_recovered_software_graph_smoke PRIVATE
  rr2nw_game_entry_runtime
)
target_compile_features(
  rr2nw_recovered_software_graph_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_recovered_software_graph_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_level_runtime_smoke
  src/portability/RecoveredLevelRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_recovered_level_runtime_smoke PRIVATE
  rr2nw_game_entry_runtime
)
target_compile_features(
  rr2nw_recovered_level_runtime_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_recovered_level_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_level_assets_smoke
  src/portability/RecoveredLevelAssetsSmoke.cpp
)
target_link_libraries(rr2nw_recovered_level_assets_smoke PRIVATE
  rr2nw_game_entry_runtime
)
target_compile_features(
  rr2nw_recovered_level_assets_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_recovered_level_assets_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_retail_script_manifest_smoke
  src/portability/RecoveredRetailScriptManifestSmoke.cpp
)
target_link_libraries(
  rr2nw_recovered_retail_script_manifest_smoke PRIVATE
  rr2nw_recovered_retail_script_manifest
)
target_compile_features(
  rr2nw_recovered_retail_script_manifest_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_retail_script_manifest_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_mod_runtime_smoke
  src/portability/RecoveredModRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_mod_runtime_smoke PRIVATE rr2nw_mod_runtime)
target_compile_features(rr2nw_mod_runtime_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_mod_runtime_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_mod_runtime_smoke PRIVATE /W4 /permissive-)
endif()
add_executable(rr2nw_mod_stack_runtime_smoke
  src/portability/RecoveredModStackRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_mod_stack_runtime_smoke PRIVATE rr2nw_mod_runtime)
target_compile_features(rr2nw_mod_stack_runtime_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_mod_stack_runtime_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(rr2nw_mod_stack_runtime_smoke PRIVATE
    /W4 /permissive-
  )
endif()
target_link_libraries(rr2nw_game_entry_runtime_smoke PRIVATE
  rr2nw_arena_obase_debug_map_mission
  rr2nw_game_entry_runtime
  rr2nw_supervisor_global
)
target_compile_features(rr2nw_game_entry_runtime_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_game_entry_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_library(rr2nw_game_startup STATIC
  src/platform/windows/GameStartup.cpp
  src/platform/windows/GameStartup.h
)
set(RR2NW_BUILD_REVISION_HEADER
  "${CMAKE_CURRENT_BINARY_DIR}/generated/RR2NWBuildRevision.h"
)
add_custom_target(rr2nw_build_revision
  COMMAND "${CMAKE_COMMAND}"
          "-DRR2NW_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
          "-DRR2NW_OUTPUT=${RR2NW_BUILD_REVISION_HEADER}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildRevision.cmake"
  BYPRODUCTS "${RR2NW_BUILD_REVISION_HEADER}"
  VERBATIM
)
add_dependencies(rr2nw_game_startup rr2nw_build_revision)
target_include_directories(rr2nw_game_startup PUBLIC
  src/platform/windows
)
target_include_directories(rr2nw_game_startup PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_compile_features(rr2nw_game_startup PUBLIC cxx_std_14)
target_compile_definitions(rr2nw_game_startup PRIVATE
  UNICODE
  _UNICODE
  RR2NW_BUILD_VERSION="${PROJECT_VERSION}"
  RR2NW_BUILD_CONFIGURATION="$<CONFIG>"
)
target_link_libraries(rr2nw_game_startup PUBLIC
  rr2nw_recovered_game_services_runtime
  shell32
)

add_executable(rr2nw_game WIN32
  src/platform/windows/GameMain.cpp
)
target_link_libraries(rr2nw_game PRIVATE rr2nw_game_startup)
target_compile_features(rr2nw_game PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_game PRIVATE UNICODE _UNICODE)
set_target_properties(rr2nw_game PROPERTIES OUTPUT_NAME rr2nw)

add_executable(rr2nw_scene_software_draw_link_probe
  src/portability/SceneSoftwareDrawLinkProbe.cpp
)
target_link_libraries(rr2nw_scene_software_draw_link_probe PRIVATE
  rr2nw_scene_software_draw
)
target_compile_features(
  rr2nw_scene_software_draw_link_probe PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_scene_software_draw_link_probe PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_view_object_decoder_smoke
  src/portability/ViewObjectDecoderSmoke.cpp
)
target_link_libraries(rr2nw_view_object_decoder_smoke PRIVATE
  rr2nw_game_entry_runtime
  rr2nw_view_object_decoder
)
target_compile_features(rr2nw_view_object_decoder_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_view_object_decoder_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_view_terrain_decoder_smoke
  src/portability/ViewTerrainDecoderSmoke.cpp
)
target_link_libraries(rr2nw_view_terrain_decoder_smoke PRIVATE
  rr2nw_recovered_terrain_runtime
)
target_compile_features(rr2nw_view_terrain_decoder_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_view_terrain_decoder_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_scene_order_decoder_smoke
  src/portability/SceneOrderDecoderSmoke.cpp
)
target_link_libraries(rr2nw_scene_order_decoder_smoke PRIVATE
  rr2nw_recovered_scene_order_runtime
)
target_compile_features(rr2nw_scene_order_decoder_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_scene_order_decoder_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_drawable_scene_smoke
  src/portability/RecoveredDrawableSceneSmoke.cpp
)
target_link_libraries(rr2nw_recovered_drawable_scene_smoke PRIVATE
  rr2nw_recovered_drawable_scene_runtime
)
target_compile_features(
  rr2nw_recovered_drawable_scene_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_recovered_drawable_scene_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_game_level_runtime_smoke
  src/portability/RecoveredGameLevelRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_recovered_game_level_runtime_smoke PRIVATE
  rr2nw_recovered_game_level_runtime
)
target_compile_features(
  rr2nw_recovered_game_level_runtime_smoke PRIVATE cxx_std_14
)
target_compile_definitions(rr2nw_recovered_game_level_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_game_services_runtime_smoke
  src/portability/RecoveredGameServicesRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_recovered_game_services_runtime_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)

add_executable(rr2nw_recovered_gameplay_tuning_smoke
  src/portability/RecoveredGameplayTuningSmoke.cpp
)
target_link_libraries(rr2nw_recovered_gameplay_tuning_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_recovered_gameplay_tuning_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_gameplay_tuning_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
add_executable(rr2nw_recovered_script_event_smoke
  src/portability/RecoveredScriptEventSmoke.cpp
)
target_link_libraries(rr2nw_recovered_script_event_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_recovered_script_event_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_script_event_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
target_compile_features(
  rr2nw_recovered_game_services_runtime_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_game_services_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_vehicle_control_journal_smoke
  src/portability/VehicleControlJournalSmoke.cpp
)
target_link_libraries(rr2nw_vehicle_control_journal_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_vehicle_control_journal_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_vehicle_control_journal_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_level_continuation_smoke
  src/portability/LevelContinuationSmoke.cpp
)
target_link_libraries(rr2nw_level_continuation_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_level_continuation_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_level_continuation_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_level_save_slot_smoke
  src/portability/LevelSaveSlotSmoke.cpp
)
target_link_libraries(rr2nw_level_save_slot_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_level_save_slot_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_level_save_slot_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_indexed_png_smoke
  src/portability/IndexedPngSmoke.cpp
)
target_link_libraries(rr2nw_indexed_png_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_indexed_png_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_indexed_png_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_active_world_fresh_restore_smoke
  src/portability/ActiveWorldFreshRestoreSmoke.cpp
)
target_link_libraries(rr2nw_active_world_fresh_restore_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_active_world_fresh_restore_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_active_world_fresh_restore_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_arena_seance_runtime_smoke
  src/portability/RecoveredArenaSeanceRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_recovered_arena_seance_runtime_smoke PRIVATE
  rr2nw_recovered_game_services_runtime
)
target_compile_features(
  rr2nw_recovered_arena_seance_runtime_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_arena_seance_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_peripheral_attribute_state_smoke
  src/portability/PeripheralAttributeStateSmoke.cpp
)
target_link_libraries(rr2nw_peripheral_attribute_state_smoke PRIVATE
  rr2nw_arena_farter_attribute_state
  rr2nw_arena_lamp_attribute_state
  rr2nw_arena_corpse_attribute_state
)
target_compile_features(
  rr2nw_peripheral_attribute_state_smoke PRIVATE cxx_std_14
)

add_executable(rr2nw_sound_object_state_smoke
  src/portability/SoundObjectStateSmoke.cpp
)
target_link_libraries(rr2nw_sound_object_state_smoke PRIVATE
  rr2nw_arena_sound_object_state
)
target_compile_features(rr2nw_sound_object_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_sound_object_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_farter_subject_state_smoke
  src/portability/FarterSubjectStateSmoke.cpp
)
target_link_libraries(rr2nw_farter_subject_state_smoke PRIVATE
  rr2nw_arena_farter_subject_state
)
target_compile_features(rr2nw_farter_subject_state_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_farter_subject_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
target_compile_definitions(
  rr2nw_peripheral_attribute_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_smoker_subject_state_smoke
  src/portability/SmokerSubjectStateSmoke.cpp
)
target_link_libraries(rr2nw_smoker_subject_state_smoke PRIVATE
  rr2nw_arena_smoker_subject_state
)
target_compile_features(
  rr2nw_smoker_subject_state_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_smoker_subject_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_smoke_subject_state_smoke
  src/portability/SmokeSubjectStateSmoke.cpp
)
target_link_libraries(rr2nw_smoke_subject_state_smoke PRIVATE
  rr2nw_arena_smoke_subject_state
)
target_compile_features(
  rr2nw_smoke_subject_state_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_smoke_subject_state_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_legacy_script_runtime_smoke
  src/portability/RecoveredLegacyScriptRuntimeSmoke.cpp
)
target_link_libraries(rr2nw_recovered_legacy_script_runtime_smoke PRIVATE
  rr2nw_recovered_legacy_script_runtime
  rr2nw_arena_bird_attribute_state
  rr2nw_arena_artefact_attribute_state
  rr2nw_arena_orphan_attribute_state
  rr2nw_arena_smoke_attribute_state
  rr2nw_arena_spark_attribute_state
)
target_compile_features(
  rr2nw_recovered_legacy_script_runtime_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_legacy_script_runtime_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_carrier_smoke
  src/portability/CarrierSmoke.cpp
)

add_executable(rr2nw_legacy_skin_resource_smoke
  src/portability/LegacySkinResourceSmoke.cpp
)
target_link_libraries(rr2nw_legacy_skin_resource_smoke PRIVATE
  rr2nw_arena_skin_resource_state
  rr2nw_game_entry_runtime
  rr2nw_recovered_skin_resource_catalog
)
target_compile_features(rr2nw_legacy_skin_resource_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_legacy_skin_resource_smoke PRIVATE
  __NT__
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

add_executable(rr2nw_recovered_wav_metadata_catalog_smoke
  src/portability/RecoveredWavMetadataCatalogSmoke.cpp
)
target_link_libraries(rr2nw_recovered_wav_metadata_catalog_smoke PRIVATE
  rr2nw_recovered_wav_metadata_catalog
)
target_compile_features(
  rr2nw_recovered_wav_metadata_catalog_smoke PRIVATE cxx_std_14
)
target_compile_definitions(
  rr2nw_recovered_wav_metadata_catalog_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)
if(MSVC)
  target_compile_options(
    rr2nw_recovered_wav_metadata_catalog_smoke PRIVATE /W4 /permissive-
  )
endif()
target_link_libraries(
  rr2nw_carrier_smoke PRIVATE rr2nw_arena_obase_carrier
)
target_compile_features(rr2nw_carrier_smoke PRIVATE cxx_std_14)
target_compile_definitions(rr2nw_carrier_smoke PRIVATE
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_math_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_tagged_file_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_config_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_script_vm_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_kernel_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_publisher_lifecycle_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_storage_save_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_context_route_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_mproj_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_level_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_debug_map_mission_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(rr2nw_fountain_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_const_string_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_channel_map_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_vehicle_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_vehicle_services_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(rr2nw_physics_core_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_view_state_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_scene_draw_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_graph_runtime_smoke PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_software_polygon_rasterizer_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_frame_runtime_smoke PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_recovered_software_frame_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_scene_software_state_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_terrain_view_state_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_menu_texture_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_menu_shutdown_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_panel_lifecycle_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_vehicle_link_probe PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_vehicle_shell_link_probe PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_game_link_probe PRIVATE /W4 /permissive- /wd4081
  )
  target_link_options(rr2nw_game_link_probe PRIVATE /INCREMENTAL:NO /OPT:REF)
  target_compile_options(
    rr2nw_game_entry_runtime PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_supervisor_global PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_game_entry_runtime_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_software_graph_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_level_runtime_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_level_assets_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_retail_script_manifest_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_game_startup PRIVATE /W4 /permissive- /utf-8
  )
  target_compile_options(rr2nw_game PRIVATE /W4 /permissive- /utf-8)
  target_link_options(rr2nw_game PRIVATE /INCREMENTAL:NO /MAP)
  target_compile_options(
    rr2nw_scene_software_draw_link_probe PRIVATE /W4 /permissive-
  )
  target_link_options(rr2nw_scene_software_draw_link_probe PRIVATE
    /INCREMENTAL:NO /OPT:REF
  )
  target_compile_options(
    rr2nw_view_object_decoder_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_view_terrain_decoder_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_scene_order_decoder_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_drawable_scene_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_game_level_runtime_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_game_services_runtime_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_gameplay_tuning_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_script_event_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_vehicle_control_journal_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_level_continuation_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_level_save_slot_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_indexed_png_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_active_world_save_smoke PRIVATE /W4 /permissive-
  )
  target_compile_options(
    rr2nw_simulation_continuation_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_active_world_fresh_restore_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_arena_seance_runtime_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_sound_object_state_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_farter_subject_state_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_peripheral_attribute_state_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(
    rr2nw_recovered_legacy_script_runtime_smoke PRIVATE /W4 /permissive- /wd4081
  )
  target_compile_options(rr2nw_carrier_smoke PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
  add_test(
    NAME game-launch-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DRR2NW_GAME=$<TARGET_FILE:rr2nw_game>"
            "-DRR2NW_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/game-launch-smoke-$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GameLaunchSmoke.cmake"
  )
  add_test(NAME legacy-math-smoke COMMAND rr2nw_math_smoke)
  add_test(
    NAME game-entry-runtime-smoke
    COMMAND rr2nw_game_entry_runtime_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/game-entry-runtime-smoke.cfg"
  )
  add_test(NAME legacy-game-entry-link-smoke COMMAND rr2nw_game_link_probe)
  add_test(
    NAME recovered-software-graph-smoke
    COMMAND rr2nw_recovered_software_graph_smoke
  )
  add_test(
    NAME recovered-level-runtime-smoke
    COMMAND rr2nw_recovered_level_runtime_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/recovered-level-runtime-smoke-$<CONFIG>"
  )
  add_test(
    NAME recovered-level-assets-smoke
    COMMAND rr2nw_recovered_level_assets_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/recovered-level-assets-smoke-$<CONFIG>"
  )
  add_test(
    NAME recovered-retail-script-manifest-smoke
    COMMAND rr2nw_recovered_retail_script_manifest_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/retail-script-manifest-smoke-$<CONFIG>"
  )
  add_test(
    NAME mod-runtime-smoke
    COMMAND rr2nw_mod_runtime_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/mod-runtime-smoke-$<CONFIG>"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/rr2nw.example.data-pack"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/mods/rr2nw.example.derived-level"
  )
  add_test(
    NAME mod-stack-runtime-smoke
    COMMAND rr2nw_mod_stack_runtime_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/mod-stack-runtime-smoke-$<CONFIG>"
  )
  add_test(
    NAME legacy-tagged-file-smoke
    COMMAND rr2nw_tagged_file_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/tagged-file-smoke.tfb"
  )
  add_test(
    NAME legacy-config-smoke
    COMMAND rr2nw_config_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/config-smoke.cfg"
  )
  add_test(NAME legacy-script-vm-smoke COMMAND rr2nw_script_vm_smoke)
  add_test(NAME legacy-kernel-state-smoke COMMAND rr2nw_kernel_state_smoke)
  add_test(
    NAME legacy-publisher-lifecycle-smoke
    COMMAND rr2nw_publisher_lifecycle_smoke
  )
  add_test(
    NAME legacy-storage-save-smoke
    COMMAND rr2nw_storage_save_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/storage-save-smoke.sav"
  )
  add_test(
    NAME active-world-save-smoke
    COMMAND rr2nw_active_world_save_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/active-world-save-smoke-$<CONFIG>.rr2"
  )
  add_test(
    NAME simulation-continuation-smoke
    COMMAND rr2nw_simulation_continuation_smoke
  )
  add_test(
    NAME active-world-fresh-restore-smoke
    COMMAND rr2nw_active_world_fresh_restore_smoke
  )
  add_test(
    NAME legacy-context-route-smoke
    COMMAND rr2nw_context_route_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/context-route-smoke.sav"
  )
  add_test(
    NAME legacy-mproj-smoke
    COMMAND rr2nw_mproj_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/mproj-smoke.sav"
  )
  add_test(NAME legacy-level-state-smoke COMMAND rr2nw_level_state_smoke)
  add_test(NAME legacy-const-string-smoke COMMAND rr2nw_const_string_smoke)
  add_test(NAME legacy-channel-map-smoke COMMAND rr2nw_channel_map_smoke)
  add_test(
    NAME legacy-debug-map-mission-smoke
    COMMAND rr2nw_debug_map_mission_smoke
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
  add_test(
    NAME legacy-vehicle-services-smoke
    COMMAND rr2nw_vehicle_services_smoke
  )
  add_test(NAME legacy-physics-core-smoke COMMAND rr2nw_physics_core_smoke)
  add_test(NAME legacy-view-state-smoke COMMAND rr2nw_view_state_smoke)
  add_test(NAME legacy-scene-draw-smoke COMMAND rr2nw_scene_draw_smoke)
  add_test(NAME legacy-graph-runtime-smoke COMMAND rr2nw_graph_runtime_smoke)
  add_test(
    NAME software-polygon-rasterizer-smoke
    COMMAND rr2nw_software_polygon_rasterizer_smoke
  )
  add_test(NAME legacy-frame-runtime-smoke COMMAND rr2nw_frame_runtime_smoke)
  add_test(
    NAME recovered-software-frame-smoke
    COMMAND rr2nw_recovered_software_frame_smoke
  )
  add_test(
    NAME scene-software-state-smoke
    COMMAND rr2nw_scene_software_state_smoke
  )
  add_test(
    NAME terrain-view-state-smoke
    COMMAND rr2nw_terrain_view_state_smoke
  )
  add_test(
    NAME scene-software-draw-link-smoke
    COMMAND rr2nw_scene_software_draw_link_probe
  )
  add_test(
    NAME view-object-decoder-smoke
    COMMAND rr2nw_view_object_decoder_smoke
  )
  add_test(
    NAME view-terrain-decoder-smoke
    COMMAND rr2nw_view_terrain_decoder_smoke
  )
  add_test(
    NAME scene-order-decoder-smoke
    COMMAND rr2nw_scene_order_decoder_smoke
  )
  add_test(
    NAME recovered-drawable-scene-smoke
    COMMAND rr2nw_recovered_drawable_scene_smoke
  )
  add_test(
    NAME recovered-game-level-runtime-smoke
    COMMAND rr2nw_recovered_game_level_runtime_smoke
  )
  add_test(
    NAME recovered-game-services-runtime-smoke
    COMMAND rr2nw_recovered_game_services_runtime_smoke
  )
  add_test(
    NAME recovered-gameplay-tuning-smoke
    COMMAND rr2nw_recovered_gameplay_tuning_smoke
  )
  add_test(
    NAME recovered-script-event-smoke
    COMMAND rr2nw_recovered_script_event_smoke
  )
  add_test(
    NAME vehicle-control-journal-smoke
    COMMAND rr2nw_vehicle_control_journal_smoke
  )
  add_test(
    NAME level-continuation-smoke
    COMMAND rr2nw_level_continuation_smoke
  )
  add_test(
    NAME level-save-slot-smoke
    COMMAND rr2nw_level_save_slot_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/level-save-slot-smoke-$<CONFIG>"
  )
  add_test(
    NAME indexed-png-smoke
    COMMAND rr2nw_indexed_png_smoke
  )
  add_test(
    NAME recovered-arena-seance-runtime-smoke
    COMMAND rr2nw_recovered_arena_seance_runtime_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/arena-seance-smoke-$<CONFIG>"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/SMOKE.SCI"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/FARTER.SCI"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/LAMP.SCI"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/farterattr.sci"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/CORPSE.SCI"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/localmain.sci"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/LOADWAV.SCI"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/set_farter.sci"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/TAXI.SCI"
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/VEHICLE.SCI"
             "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/BULLET.SCI"
             "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/bullet_loc.sci"
             "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/load_route.sci"
             "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/PEOPLE.SCI"
             "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/set_people.sci"
             "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N/SCINC/TANK.SCI"
  )
  add_test(
    NAME peripheral-attribute-state-smoke
    COMMAND rr2nw_peripheral_attribute_state_smoke
  )
  add_test(
    NAME sound-object-state-smoke
    COMMAND rr2nw_sound_object_state_smoke
  )
  add_test(
    NAME farter-subject-state-smoke
    COMMAND rr2nw_farter_subject_state_smoke
  )
  add_test(
    NAME smoker-subject-state-smoke
    COMMAND rr2nw_smoker_subject_state_smoke
  )
  add_test(
    NAME smoke-subject-state-smoke
    COMMAND rr2nw_smoke_subject_state_smoke
  )
  add_test(
    NAME recovered-legacy-script-runtime-smoke
    COMMAND rr2nw_recovered_legacy_script_runtime_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/legacy-script-smoke-$<CONFIG>"
  )
  add_test(
    NAME legacy-menu-texture-smoke
    COMMAND rr2nw_menu_texture_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/menu-texture-smoke.spr"
  )
  add_test(NAME legacy-menu-shutdown-smoke COMMAND rr2nw_menu_shutdown_smoke)
  add_test(
    NAME legacy-panel-lifecycle-smoke
    COMMAND rr2nw_panel_lifecycle_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/panel-lifecycle-smoke.pnl"
            "${CMAKE_CURRENT_BINARY_DIR}/panel-lifecycle-truncated.pnl"
  )
  add_test(
    NAME legacy-carrier-smoke
    COMMAND rr2nw_carrier_smoke
            "${CMAKE_CURRENT_BINARY_DIR}/carrier-smoke.sav"
  )
  add_test(
    NAME legacy-skin-resource-smoke
    COMMAND rr2nw_legacy_skin_resource_smoke
  )
  add_test(
    NAME recovered-wav-metadata-catalog-smoke
    COMMAND rr2nw_recovered_wav_metadata_catalog_smoke
            "${CMAKE_CURRENT_SOURCE_DIR}/nw/OUTPUT/Level.03N"
  )
endif()
