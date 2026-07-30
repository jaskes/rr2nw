if(NOT DEFINED RR2NW_GAME OR NOT DEFINED RR2NW_TEST_ROOT)
  message(FATAL_ERROR "RR2NW_GAME and RR2NW_TEST_ROOT are required")
endif()

set(fixture "${RR2NW_TEST_ROOT}/fixture with space")
set(diagnostics "${RR2NW_TEST_ROOT}/diagnostics")
set(index_diagnostics "${RR2NW_TEST_ROOT}/index-diagnostics")
set(name_diagnostics "${RR2NW_TEST_ROOT}/name-diagnostics")
set(selection_diagnostics "${RR2NW_TEST_ROOT}/selection-diagnostics")
set(invalid_diagnostics "${RR2NW_TEST_ROOT}/invalid-diagnostics")
file(REMOVE_RECURSE "${RR2NW_TEST_ROOT}")
file(MAKE_DIRECTORY "${fixture}")

set(config "[Levels]\n")
foreach(index RANGE 0 8)
  string(APPEND config "${index}=Level.${index}\n")
  file(MAKE_DIRECTORY "${fixture}/Level.${index}")
endforeach()
string(APPEND config "\n[Init]\nStartLevel=3\n")
file(WRITE "${fixture}/game.cfg" "${config}")
file(WRITE "${fixture}/LEVEL0.SC" "synthetic launch fixture\n")

file(GLOB_RECURSE fixture_before RELATIVE "${fixture}" "${fixture}/*")
file(SHA256 "${fixture}/game.cfg" config_hash_before)
file(SHA256 "${fixture}/LEVEL0.SC" level_hash_before)
execute_process(
  COMMAND "${RR2NW_GAME}"
          --launch-smoke
          --data-dir "${fixture}"
          --diagnostics-dir "${diagnostics}"
  RESULT_VARIABLE launch_result
)
if(NOT launch_result EQUAL 0)
  message(FATAL_ERROR "rr2nw launch smoke returned ${launch_result}")
endif()

set(log "${diagnostics}/rr2nw-startup.log")
if(NOT EXISTS "${log}")
  message(FATAL_ERROR "rr2nw launch smoke did not create its diagnostic log")
endif()
file(READ "${log}" log_text)
foreach(expected
    "marker=process-ready"
    "retail_level_count=9"
    "start_level_source=game.cfg"
    "start_level=3"
    "start_level_dir=Level.3"
    "data_access=read-only"
    "marker=retail-data-ready"
    "recovered_runtime=skipped-for-launch-smoke"
    "marker=pre-content-ready")
  string(FIND "${log_text}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "startup log is missing: ${expected}")
  endif()
endforeach()

execute_process(
  COMMAND "${RR2NW_GAME}"
          --launch-smoke
          --data-dir "${fixture}"
          --start-level 5
          --diagnostics-dir "${index_diagnostics}"
  RESULT_VARIABLE index_result
)
if(NOT index_result EQUAL 0)
  message(FATAL_ERROR "rr2nw rejected a valid numeric level override")
endif()
file(READ "${index_diagnostics}/rr2nw-startup.log" index_log_text)
foreach(expected
    "start_level_source=command-line"
    "start_level_requested=5"
    "start_level=5"
    "start_level_dir=Level.5")
  string(FIND "${index_log_text}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "numeric override log is missing: ${expected}")
  endif()
endforeach()

execute_process(
  COMMAND "${RR2NW_GAME}"
          --launch-smoke
          --data-dir "${fixture}"
          --start-level "level.7"
          --diagnostics-dir "${name_diagnostics}"
  RESULT_VARIABLE name_result
)
if(NOT name_result EQUAL 0)
  message(FATAL_ERROR "rr2nw rejected a valid named level override")
endif()
file(READ "${name_diagnostics}/rr2nw-startup.log" name_log_text)
foreach(expected
    "start_level_source=command-line"
    "start_level_requested=level.7"
    "start_level=7"
    "start_level_dir=Level.7")
  string(FIND "${name_log_text}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "named override log is missing: ${expected}")
  endif()
endforeach()

execute_process(
  COMMAND "${RR2NW_GAME}"
          --launch-smoke
          --data-dir "${fixture}"
          --start-level "Level.missing"
          --diagnostics-dir "${selection_diagnostics}"
  RESULT_VARIABLE selection_result
)
if(selection_result EQUAL 0)
  message(FATAL_ERROR "rr2nw accepted an unknown named level override")
endif()
file(READ "${selection_diagnostics}/rr2nw-startup.log" selection_log_text)
foreach(expected
    "start_level_requested=Level.missing"
    "marker=level-selection-invalid")
  string(FIND "${selection_log_text}" "${expected}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "invalid selection log is missing: ${expected}")
  endif()
endforeach()

file(GLOB_RECURSE fixture_after RELATIVE "${fixture}" "${fixture}/*")
if(NOT fixture_before STREQUAL fixture_after)
  message(FATAL_ERROR "rr2nw modified the retail fixture during preflight")
endif()
file(SHA256 "${fixture}/game.cfg" config_hash_after)
file(SHA256 "${fixture}/LEVEL0.SC" level_hash_after)
if(NOT config_hash_before STREQUAL config_hash_after OR
   NOT level_hash_before STREQUAL level_hash_after)
  message(FATAL_ERROR "rr2nw changed fixture content during preflight")
endif()

execute_process(
  COMMAND "${RR2NW_GAME}"
          --launch-smoke
          --data-dir "${RR2NW_TEST_ROOT}/missing"
          --diagnostics-dir "${invalid_diagnostics}"
  RESULT_VARIABLE invalid_result
)
if(invalid_result EQUAL 0)
  message(FATAL_ERROR "rr2nw accepted a missing retail data directory")
endif()

file(READ "${invalid_diagnostics}/rr2nw-startup.log" invalid_log_text)
string(FIND "${invalid_log_text}" "marker=data-not-ready" invalid_marker)
if(invalid_marker EQUAL -1)
  message(FATAL_ERROR "invalid-data launch did not stop at data-not-ready")
endif()
