cmake_minimum_required(VERSION 3.20)
project(pxa_starfighter C ASM)
include(../common/sdl-games/Build.cmake)
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS vendor/src/*.c)
set_source_files_properties(vendor/src/Starfighter.c PROPERTIES COMPILE_DEFINITIONS main=pxa_sdl_game_main)
pxa_sdl_component(TITLE "Project: Starfighter" SOURCES ${GAME_SOURCES}
  INCLUDES vendor/src DEFINITIONS DATADIR="/data" VERSION="2.4.0-pxa" NOFONT=1 SCREEN_WIDTH=640 SCREEN_HEIGHT=480)
