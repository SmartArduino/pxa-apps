cmake_minimum_required(VERSION 3.20)
project(pxa_arcade CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main COMPONENT_ID main
    SOURCES main.cpp modules/brick_breaker.cpp modules/flappy_bird.cpp modules/jump_jump.cpp modules/jumping.cpp modules/minesweeper.cpp modules/tetris.cpp)
