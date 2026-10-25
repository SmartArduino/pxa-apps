cmake_minimum_required(VERSION 3.20)
project(pxa_recrof_fixture C CXX)
include(../../Engine.cmake)
set(CNC_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../../../../command-conquer/vendor/Vanilla-Conquer/common")
pxa_recrof_component(TITLE "Runtime fixture" SOURCES main.cpp
    ${CNC_COMMON}/rawfile.cpp ${CNC_COMMON}/debugstring.cpp ${CNC_COMMON}/alloc.cpp ${CNC_COMMON}/newdel.cpp ${CNC_COMMON}/file.cpp ${CNC_COMMON}/file_posix.cpp INCLUDES ${CNC_COMMON})

# Exercise negotiated 8ms with the protocol host, including legacy fallback.
target_compile_definitions(pxa_main PRIVATE PORT_TICK_US=8000)
