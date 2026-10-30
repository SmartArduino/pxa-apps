cmake_minimum_required(VERSION 3.20)
project(pxa_lantern_grove LANGUAGES C CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../common/cpp/Application.cmake")
pxa_add_callback_component(pxa_main COMPONENT_ID main SOURCES main.cpp core.cpp clock.cpp ../common/pocket-play/runtime.cpp)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)

# Native-resolution artwork and system-font Canvas overlay.
target_compile_definitions(pxa_main PRIVATE PXA_POCKET_STUDIO=1)
