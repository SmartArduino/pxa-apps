cmake_minimum_required(VERSION 3.20)
project(pxa_motion_app LANGUAGES CXX)
set(PXA_COROUTINE_SLOT_BYTES 4096)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp)
target_include_directories(pxa_main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# Size tuning applies to this app's SDK archive, without changing SDK sources.
set_property(TARGET pxa_guest_cpp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(TARGET pxa_main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_options(pxa_guest_cpp PRIVATE -Oz)
target_compile_options(pxa_main PRIVATE -Oz)
target_link_options(pxa_main PRIVATE -Wl,--lto-O2)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=32768)

# All UI/time diagnostics use integers; omit unused floating-point printf.
target_link_libraries(pxa_main PRIVATE c-printscan-no-floating-point)
