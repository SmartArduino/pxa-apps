cmake_minimum_required(VERSION 3.20)
project(pxa_garden_guard CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main COMPONENT_ID main
    SOURCES main.cpp)
