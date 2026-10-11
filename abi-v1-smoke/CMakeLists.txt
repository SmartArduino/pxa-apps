cmake_minimum_required(VERSION 3.20)
project(pxa_abi_v1_smoke CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main COMPONENT_ID main
    SOURCES main.cpp)
