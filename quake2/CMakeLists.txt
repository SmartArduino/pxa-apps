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
if(NOT DEFINED PXA_QUAKE2_ARENA_BYTES)
    set(PXA_QUAKE2_ARENA_BYTES 8388608)
endif()
pxa_recrof_component(TITLE "Quake II" SOURCES ${ENGINE} ${Q}/null/cd_null.c
    ${Q}/port_esp32.c port.c network.c pcx.c ../common/recrof/pcx_stream.c INCLUDES ${Q}
    DEFINITIONS PXA_STREAM_PCX=1 QUAKE_MEMSIZE=${PXA_QUAKE2_ARENA_BYTES} PORT_PACED_BY_MAIN=1)

# Match the spawn table signature without editing the upstream source.
target_link_options(pxa_main PRIVATE -Wl,--wrap=SP_info_player_intermission)

# Keep the software renderer hot, while reducing PSRAM code/instruction-cache
# pressure from menus, loaders and game/server code on small-memory devices.
target_compile_options(pxa_main PRIVATE -Os)
file(GLOB RENDER_SOURCES CONFIGURE_DEPENDS ${Q}/ref_soft/*.c)
set_source_files_properties(${RENDER_SOURCES} ${Q}/game/q_shared.c
    PROPERTIES COMPILE_OPTIONS -O3)
# Model parsing is cold; keep renderer loops at O3 without enlarging loaders.
set_source_files_properties(${Q}/ref_soft/r_model.c PROPERTIES COMPILE_OPTIONS -Os)
