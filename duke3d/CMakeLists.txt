cmake_minimum_required(VERSION 3.20)
project(pxa_duke3d C CXX)
include(../common/recrof/Engine.cmake)
file(GLOB ENGINE CONFIGURE_DEPENDS vendor/Engine/src/*.c)
file(GLOB_RECURSE GAME CONFIGURE_DEPENDS vendor/Game/src/*.c)
pxa_recrof_component(TITLE "Duke Nukem 3D" SOURCES ${ENGINE} ${GAME} port.c input_stats.c
    graphics.c internalFlash.c radio.c crc.c extmemory.c ../common/recrof/vflash.c
    INCLUDES . vendor/Engine/src vendor/Game/src port
    DEFINITIONS ESP_PLATFORM=1 BOARD=7 BOARD_TDECK=7 BOARD_WIO_TRACKER_L2=8
        BOARD_THEGAMEPAD_ARDUINONANOMATTER=6)
target_link_options(pxa_main PRIVATE -Wl,--wrap=sampletimer)
