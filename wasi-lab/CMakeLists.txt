cmake_minimum_required(VERSION 3.20)
project(pxa_wasi_lab CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main ALLOCATOR COMPONENT_ID main
    SOURCES main.cpp)
