cmake_minimum_required(VERSION 3.20)
project(pxa_maze_evil CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main COMPONENT_ID main
    SOURCES main.cpp raster.cpp palette.cpp font.cpp assets.cpp sprites.cpp raycast.cpp world.cpp render.cpp input.cpp audio.cpp)
