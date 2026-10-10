cmake_minimum_required(VERSION 3.20)
project(pxa_novel_reader CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
add_compile_definitions(PXA_COROUTINE_SLOT_COUNT=8 PXA_COROUTINE_SLOT_BYTES=2048)
include(PxaGuest)
pxa_add_component(pxa_main CPP COMPONENT_ID main SOURCES main.cpp)
include(PxaCppDiagnostics)
pxa_use_bounded_libcpp_diagnostics(pxa_main)

# ESP32 AOT requires more auxiliary C stack than the desktop target. Keep the
# established 64 KiB reserve; removing duplicate page storage saves Guest RAM.
target_link_options(pxa_main PRIVATE "-Wl,-z,stack-size=65536")
