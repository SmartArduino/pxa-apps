cmake_minimum_required(VERSION 3.20)
project(pxa_tomb_explorer CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main COMPONENT_ID main
    SOURCES main.cpp mesh.cpp world.cpp player.cpp character.cpp input.cpp palette.cpp textures.cpp level_data.cpp)
