cmake_minimum_required(VERSION 3.20)
project(pxa_jump_jump_3d_cpp CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_component(pxa_main CPP COMPONENT_ID main
    SOURCES
        main.cpp
        jump3d_audio.cpp
        jump3d_font.cpp
        jump3d_game.cpp
        jump3d_palette.cpp
        jump3d_render.cpp)
