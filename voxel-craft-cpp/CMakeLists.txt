cmake_minimum_required(VERSION 3.20)
project(pxa_voxel_craft_cpp CXX)

list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)

pxa_add_component(pxa_main CPP COMPONENT_ID main
    SOURCES
        main.cpp
        voxel_world.cpp
        voxel_mesher.cpp
        voxel_render.cpp)
if(PXA_GENERATED_INCLUDE_DIR)
    target_include_directories(pxa_main PRIVATE "${PXA_GENERATED_INCLUDE_DIR}")
endif()
