cmake_minimum_required(VERSION 3.20)
project(pxa_quake2 C CXX)
include(../common/recrof/Engine.cmake)
set(Q ${CMAKE_CURRENT_SOURCE_DIR}/vendor/quake2)
file(GLOB ENGINE CONFIGURE_DEPENDS ${Q}/client/*.c ${Q}/server/sv_*.c
    ${Q}/qcommon/*.c ${Q}/ref_soft/*.c ${Q}/game/*.c)
list(FILTER ENGINE EXCLUDE REGEX "sv_null.c$")
foreach(name glob q_arena q_assets q_hunk q_system vid_lib vid_menu)
    list(APPEND ENGINE ${Q}/other/${name}.c)
endforeach()
pxa_recrof_component(TITLE "Quake II" SOURCES ${ENGINE} ${Q}/null/cd_null.c
    ${Q}/port_esp32.c port.c network.c INCLUDES ${Q}
    DEFINITIONS QUAKE_MEMSIZE=8388608 PORT_PACED_BY_MAIN=1)
