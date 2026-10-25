cmake_minimum_required(VERSION 3.20)
project(pxa_doom C CXX)
set(PXA_RECROF_MAIN_LOOP ON)
# Product hosts currently implement OPEN_SESSION (16 kHz mono), not OPEN_PCM.
# Let the shared runtime convert the engine's 11025 Hz stereo stream.
set(PXA_RECROF_NATIVE_PCM OFF)
set(PXA_RECROF_HOST_BLIT ON)
set(PXA_RECROF_BULK_READ_COPY ON)
set(PXA_RECROF_PACKED_GAME_CODEC ON)
set(PXA_RECROF_ENGINE_STACK_BYTES 65536)
set(PXA_RECROF_CALLBACK_STACK_BYTES 16384)
include(../common/recrof/Engine.cmake)
pxa_recrof_component(TITLE "DOOM" SOURCES doom.c INCLUDES .
    DEFINITIONS PORT_EXTERNAL_GAME_DATA=1 PORT_PACED_BY_MAIN=1
    PORT_DOOM_CONTROLLER=1
    PORT_TICK_US=8000 PORT_COOPERATE_BUDGET_US=8000)
# Customize labels/roles for a game with no jump, preserving the shared blitter.
get_target_property(doom_sources pxa_main SOURCES)
list(REMOVE_ITEM doom_sources "${PXA_RECROF_DIR}/framebuffer.c")
set_property(TARGET pxa_main PROPERTY SOURCES "${doom_sources}")
target_sources(pxa_main PRIVATE controls.c)
target_compile_options(pxa_main PRIVATE -fwrapv -fno-strict-aliasing)
