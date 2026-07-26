if(NOT DEFINED RR2NW_SOURCE_DIR OR NOT DEFINED RR2NW_OUTPUT)
  message(FATAL_ERROR "RR2NW_SOURCE_DIR and RR2NW_OUTPUT are required")
endif()

set(revision "unknown")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${RR2NW_SOURCE_DIR}"
    RESULT_VARIABLE revision_result
    OUTPUT_VARIABLE revision_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(revision_result EQUAL 0 AND NOT revision_output STREQUAL "")
    set(revision "${revision_output}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
      WORKING_DIRECTORY "${RR2NW_SOURCE_DIR}"
      RESULT_VARIABLE status_result
      OUTPUT_VARIABLE status_output
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(status_result EQUAL 0 AND NOT status_output STREQUAL "")
      string(APPEND revision "-dirty")
    endif()
  endif()
endif()

string(REGEX REPLACE "[^0-9A-Za-z._-]" "_" revision "${revision}")
set(content
  "#pragma once\n\n#define RR2NW_BUILD_REVISION \"${revision}\"\n"
)

get_filename_component(output_directory "${RR2NW_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
if(EXISTS "${RR2NW_OUTPUT}")
  file(READ "${RR2NW_OUTPUT}" old_content)
else()
  set(old_content "")
endif()
if(NOT old_content STREQUAL content)
  file(WRITE "${RR2NW_OUTPUT}" "${content}")
endif()
