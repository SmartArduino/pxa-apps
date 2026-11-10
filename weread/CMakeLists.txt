cmake_minimum_required(VERSION 3.20)
project(pxa_weread C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
add_compile_definitions(PXA_COROUTINE_SLOT_COUNT=8 PXA_COROUTINE_SLOT_BYTES=1536)
include(PxaGuest)
pxa_add_component(pxa_main CPP COMPONENT_ID main SOURCES main.cpp zlib_memory.c
    vendor/zlib/inflate.c vendor/zlib/inffast.c vendor/zlib/inftrees.c
    vendor/zlib/adler32.c vendor/zlib/crc32.c)
target_compile_definitions(pxa_main PRIVATE Z_SOLO)
include(PxaCppDiagnostics)
pxa_use_bounded_libcpp_diagnostics(pxa_main)
target_compile_options(pxa_main PRIVATE -O3)
# Requires the PXA Runtime full-main-stack policy (2026-10-17+).
# Verified on S3 with the complete shelf/notes paths and bounded task pool.
target_link_options(pxa_main PRIVATE "-Wl,-z,stack-size=16384")

# Keep CMake IPO/archive wiring; use full LTO and O3 across the app and SDK.
set_property(TARGET pxa_main pxa_guest_cpp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_compile_options(pxa_main PRIVATE -flto=full)
target_compile_options(pxa_guest_cpp PRIVATE -flto=full)
target_link_options(pxa_main PRIVATE -flto=full "-Wl,--lto-O3")
