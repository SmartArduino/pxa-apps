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
# Menus, input dispatch and save coroutines expanded the AOT loading peak.
# Keep this entry unit small while meshing and projection retain SDK -O3.
# The override is for matched compiler/performance measurements only.
if(NOT "VOXEL_SIZE_MAIN=0" IN_LIST PXA_APP_DEFINITIONS)
    set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-Oz")
endif()
