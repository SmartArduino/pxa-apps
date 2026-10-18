cmake_minimum_required(VERSION 3.20)
project(pxa_quake C CXX)
include(../common/recrof/Engine.cmake)
file(GLOB ENGINE CONFIGURE_DEPENDS vendor/quake/*.c)
# Keep the renderer and movement math fast without expanding cold parsers,
# menu logic and the generated QuakeC program in the AOT instruction cache.
foreach(source IN LISTS ENGINE)
    get_filename_component(name "${source}" NAME)
    if(NOT name MATCHES "^(d_|r_)" AND NOT name MATCHES "^(draw|mathlib|world)\\.c$")
        set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS -Os)
    endif()
endforeach()
pxa_recrof_component(TITLE "Quake" SOURCES ${ENGINE} port.c extmemory.c ../common/recrof/vflash.c
    INCLUDES port vendor/quake DEFINITIONS ESP_PLATFORM=1 PORT_PACED_BY_MAIN=1)
if(PXA_QUAKE_FRAME_PROFILE)
    target_compile_definitions(pxa_main PRIVATE PXA_FRAME_PROFILE=1)
endif()
