# SPDX-License-Identifier: MIT
# Run prepare-engine.py first; this include performs no source-generation side effect.
get_filename_component(TTCARDS_PORT_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(TTCARDS_CORE_DIR "${TTCARDS_PORT_DIR}/engine" ABSOLUTE)
set(TTCARDS_CORE_SOURCES)
foreach(_name Board BoardTile Card CardHand CardRules IPlayer CPU_Player core_helpers)
 list(APPEND TTCARDS_CORE_SOURCES "${TTCARDS_CORE_DIR}/${_name}.cpp")
endforeach()
set(TTCARDS_CORE_INCLUDE_DIRS "${TTCARDS_CORE_DIR}" "${TTCARDS_PORT_DIR}/compat")
set(TTCARDS_CORE_CXX_STANDARD 17)
set(TRIAD_CORE_SOURCES ${TTCARDS_CORE_SOURCES})
set(TRIAD_CORE_INCLUDE_DIRS ${TTCARDS_CORE_INCLUDE_DIRS})
function(ttcards_add_core target)
 target_sources(${target} PRIVATE ${TTCARDS_CORE_SOURCES})
 target_include_directories(${target} PRIVATE ${TTCARDS_CORE_INCLUDE_DIRS})
 target_compile_features(${target} PRIVATE cxx_std_17)
endfunction()

