cmake_minimum_required(VERSION 3.20)
project(pxa_sopwith C)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
file(GLOB SOPWITH_CORE CONFIGURE_DEPENDS "vendor/*.c")
# swmain.c is included by the asynchronous frontend to reuse its exact static
# command and lag routines. swinit is included to adapt its timed end-level
# transition without touching the core. Optional blocking high score UI is disabled.
list(FILTER SOPWITH_CORE EXCLUDE REGEX "/(swmain|swinit|hiscore|touch_area|tcpcomm)\\.c$")
pxa_retro_component(pxa_main COMPONENT_ID main
    SOURCES main.c compat.c ${SOPWITH_CORE}
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/compat" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
target_compile_options(pxa_main PRIVATE -Wno-unused-function)
target_link_options(pxa_main PRIVATE -Wl,--export=malloc -Wl,--export=free)
