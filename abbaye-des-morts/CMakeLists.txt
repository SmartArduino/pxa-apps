cmake_minimum_required(VERSION 3.20)
project(pxa_abbaye_des_morts C ASM)
include(../common/sdl-games/Build.cmake)
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS vendor/src/*.c)
set_source_files_properties(vendor/src/main.c PROPERTIES COMPILE_DEFINITIONS main=pxa_sdl_game_main)
pxa_sdl_component(TITLE "L'Abbaye des Morts" SOURCES ${GAME_SOURCES} progress.c
  INCLUDES . vendor/src DEFINITIONS DATADIR="/data" GAME_TITLE="L'Abbaye des Morts" PXA_PROGRESS=1)
