cmake_minimum_required(VERSION 3.20)
project(glitch_garden LANGUAGES CXX)
set(PXA_COROUTINE_SLOT_BYTES 4096)
find_package(Pxa CONFIG REQUIRED)
pxa_add_app(main SOURCES main.cpp)
target_include_directories(main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(main PRIVATE -O2)
target_link_options(main PRIVATE -Wl,-z,stack-size=32768)

# Keep the renderer budget identical to the selected package memory profile.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package.json" GARDEN_PACKAGE_JSON)
string(JSON GARDEN_MEMORY_BYTES GET "${GARDEN_PACKAGE_JSON}" build linear_memory maximum_bytes)
target_compile_definitions(main PRIVATE GARDEN_MEMORY_BYTES=${GARDEN_MEMORY_BYTES})
