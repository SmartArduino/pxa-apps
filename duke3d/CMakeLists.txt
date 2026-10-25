cmake_minimum_required(VERSION 3.20)
project(pxa_duke3d C CXX)
# Preserve the validated native PCM/main-loop/indexed-blit device profile.
if(NOT DEFINED PXA_RECROF_MAIN_LOOP)
    set(PXA_RECROF_MAIN_LOOP ON)
endif()
if(NOT DEFINED PXA_RECROF_NATIVE_PCM)
    set(PXA_RECROF_NATIVE_PCM ON)
endif()
option(PXA_RECROF_HOST_BLIT "Resolve indexed output in GameRender" ON)
set(PXA_RECROF_ENGINE_STACK_BYTES 131072)
set(PXA_RECROF_CALLBACK_STACK_BYTES 16384)
include(../common/recrof/Engine.cmake)
file(GLOB ENGINE CONFIGURE_DEPENDS vendor/Engine/src/*.c)
file(GLOB_RECURSE GAME CONFIGURE_DEPENDS vendor/Game/src/*.c)
pxa_recrof_component(TITLE "Duke Nukem 3D" SOURCES ${ENGINE} ${GAME} port.c input_stats.c
    graphics.c internalFlash.c radio.c crc.c extmemory.c ../common/recrof/vflash.c
    INCLUDES . vendor/Engine/src vendor/Game/src port
    DEFINITIONS ESP_PLATFORM=1 BOARD=7 BOARD_TDECK=7 BOARD_WIO_TRACKER_L2=8
        BOARD_THEGAMEPAD_ARDUINONANOMATTER=6)
target_link_options(pxa_main PRIVATE -Wl,--wrap=sampletimer)

target_compile_definitions(pxa_main PRIVATE PORT_ENGINE_CONTROLLER=1 PORT_EXTERNAL_GAME_DATA=1
    PORT_COOPERATE_BUDGET_US=32000 PORT_TICK_MS=16 PORT_TICK_US=8000 PORT_PACED_BY_MAIN=1 PORT_CONTINUATION_BYTES=16384
    PORT_VFLASH_CACHE_PAGES=192 PORT_VFLASH_COOPERATE=1 PORT_VFLASH_PAGED_NVM=1)
