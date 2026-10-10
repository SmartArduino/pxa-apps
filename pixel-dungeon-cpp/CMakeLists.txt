cmake_minimum_required(VERSION 3.20)
project(pxa_pixel_dungeon_cpp CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_component(pxa_main CPP COMPONENT_ID main
    SOURCES
        main.cpp
        audio.cpp
        assets.cpp
        dungeon.cpp
        font.cpp
        font_data.cpp
        game.cpp
        input.cpp
        layout.cpp
        render.cpp
        strings.cpp)
