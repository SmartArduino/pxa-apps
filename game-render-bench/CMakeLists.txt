cmake_minimum_required(VERSION 3.20)
project(pxa_game_render_bench CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main COMPONENT_ID main
    SOURCES main.cpp)
# This bounded benchmark keeps its command/upload arrays in static storage.
# Reserve an explicit Guest stack instead of the libc linker's 64 KiB default;
# the eleven-group device regression checks the deepest geometry/codec path.
target_link_options(pxa_main PRIVATE "-Wl,-z,stack-size=8192")
