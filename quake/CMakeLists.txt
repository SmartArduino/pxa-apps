cmake_minimum_required(VERSION 3.20)
project(pxa_quake C CXX)
include(../common/recrof/Engine.cmake)
file(GLOB ENGINE CONFIGURE_DEPENDS vendor/quake/*.c)
set_source_files_properties(vendor/quake/quakeProgs.c PROPERTIES COMPILE_OPTIONS -Os)
pxa_recrof_component(TITLE "Quake" SOURCES ${ENGINE} port.c extmemory.c ../common/recrof/vflash.c
    INCLUDES port vendor/quake DEFINITIONS ESP_PLATFORM=1 PORT_PACED_BY_MAIN=1)
