cmake_minimum_required(VERSION 3.20)
project(pxa_opr C)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(CORE "${ROOT}/engine")

pxa_add_component(pxa_main COMPONENT_ID main
 SOURCES frontend.c ui.c render.c session.c portable-rng.c
 "${CORE}/src/opr_init.c" "${CORE}/src/opr_floor.c" "${CORE}/src/opr_move.c"
 "${CORE}/src/opr_combat.c" "${CORE}/src/opr_interact.c" "${CORE}/src/opr_progress.c"
 INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}" "${CORE}/include" "${ROOT}/../common/retro-games")
target_compile_definitions(pxa_main PRIVATE rand=opr_rand)
target_compile_options(pxa_main PRIVATE -std=c11 -ffp-contract=off)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=262144)
