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
)
target_link_libraries(rr2nw_arena_taxi_attribute_state PUBLIC
  rr2nw_arena_storage
)
target_include_directories(rr2nw_arena_taxi_attribute_state PUBLIC
  nw/INCLUDE
  nw/DESIGN.LIB/H
  nw/ARENA
  nw/ARENA/H
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

add_library(rr2nw_main_zav_scene_state STATIC
  nw/MAIN/ZavSceneState.cpp
  nw/MAIN/ZavSceneState.h
  nw/MAIN/ZavSceneState.inl
)
target_include_directories(rr2nw_main_zav_scene_state PUBLIC nw/MAIN)
target_compile_features(rr2nw_main_zav_scene_state PUBLIC cxx_std_14)

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

add_library(rr2nw_scene_draw_state STATIC
  nw/SCENE/SceneDrawState.cpp
)
target_link_libraries(rr2nw_scene_draw_state PUBLIC rr2nw_legacy_design)
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

add_library(rr2nw_arena_obase_smoke_full STATIC
  nw/ARENA/OBASE/Smoke/Smoke.cpp
)
target_link_libraries(rr2nw_arena_obase_smoke_full PUBLIC
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
  RR2NW_SMOKE_TEXTURE_CACHE_EXTERNAL
  _CRT_NONSTDC_NO_WARNINGS
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(rr2nw_arena_obase_smoke_full PRIVATE
    /W3 /permissive- /wd4081 /wd4244
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
  rr2nw_arena_obase_artefact PUBLIC rr2nw_arena_obase_carrier
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

add_executable(rr2nw_carrier_smoke
  src/portability/CarrierSmoke.cpp
)
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
  target_compile_options(rr2nw_menu_texture_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_menu_shutdown_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_panel_lifecycle_smoke PRIVATE /W4 /permissive-)
  target_compile_options(rr2nw_vehicle_link_probe PRIVATE /W4 /permissive-)
  target_compile_options(
    rr2nw_vehicle_shell_link_probe PRIVATE /W4 /permissive-
  )
  target_compile_options(rr2nw_carrier_smoke PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
  add_test(NAME legacy-math-smoke COMMAND rr2nw_math_smoke)
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
endif()
