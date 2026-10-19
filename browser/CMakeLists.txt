cmake_minimum_required(VERSION 3.20)
project(pxa_browser C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
add_compile_definitions(PXA_COROUTINE_SLOT_COUNT=8 PXA_COROUTINE_SLOT_BYTES=2048)
# Keep O3 and let the Wasm linker remove/specialize SDK code across translation
# units. This is local to this app; it adds no default SDK cache or runtime state.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
include(PxaGuest)
pxa_add_component(pxa_main CPP COMPONENT_ID main SOURCES main.cpp browser_script_client.cpp browser_script_common.cpp)
pxa_add_component(pxa_script CPP COMPONENT_ID script SOURCES browser_script_service.cpp browser_script.cpp browser_script_common.cpp)
target_compile_definitions(pxa_script PRIVATE PXA_SCRIPT_INTERPRETED=1)
include(PxaCppDiagnostics)
pxa_use_bounded_libcpp_diagnostics(pxa_main)
pxa_use_bounded_libcpp_diagnostics(pxa_script)
target_link_options(pxa_main PRIVATE "-Wl,-z,stack-size=49152")
target_link_options(pxa_script PRIVATE "-Wl,-z,stack-size=49152")
if(DEFINED ENV{PXA_BROWSER_START_URL})
    target_compile_definitions(pxa_main PRIVATE PXA_BROWSER_START_URL="$ENV{PXA_BROWSER_START_URL}")
endif()
if(DEFINED ENV{PXA_BROWSER_TRACE})
    target_compile_definitions(pxa_main PRIVATE PXA_BROWSER_TRACE=1)
endif()
# Script-only phase profiling keeps the production UI binary and avoids its
# per-touch/layout logging cost. Both switches are absent from normal builds.
if(DEFINED ENV{PXA_BROWSER_TRACE} OR DEFINED ENV{PXA_BROWSER_SCRIPT_TRACE})
    target_compile_definitions(pxa_script PRIVATE PXA_BROWSER_TRACE=1)
endif()
if(DEFINED ENV{PXA_BROWSER_PROFILE})
    target_compile_definitions(pxa_main PRIVATE PXA_BROWSER_PROFILE=1)
endif()

# Private, pinned Guest engine. No OS module, subprocesses or Host extension.
add_library(browser_quickjs STATIC
    vendor/quickjs/quickjs.c vendor/quickjs/dtoa.c vendor/quickjs/libregexp.c
    vendor/quickjs/libunicode.c vendor/quickjs/cutils.c)
target_compile_features(browser_quickjs PRIVATE c_std_11)
target_compile_definitions(browser_quickjs PRIVATE _GNU_SOURCE CONFIG_VERSION="2026-06-04")
# Size-oriented engine code keeps the C++ UI at O3. Validate both code size
# and bounded callback time; no per-frame script work occurs while dragging.
target_compile_options(browser_quickjs PRIVATE -Oz -fwrapv -ffunction-sections -fdata-sections)
target_link_libraries(pxa_script PRIVATE browser_quickjs m)
# Prevent silently shipping stale bytecode after editing the trusted bridge.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/compiled.sha256" runtime_hashes)
foreach(entry IN LISTS runtime_hashes)
    string(REGEX MATCH "^([0-9a-f]+)  (.+)$" match "${entry}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/runtime/${CMAKE_MATCH_2}" actual_hash)
    if(NOT actual_hash STREQUAL CMAKE_MATCH_1)
        message(FATAL_ERROR "Stale browser runtime; run tools/compile_runtime.sh")
    endif()
endforeach()

# Full LTO keeps UI AOT and interpreted engine code compact.
# SDK objects remain compatible ThinLTO inputs; UI optimization stays at O3.
target_compile_options(pxa_main PRIVATE -flto=full)
target_compile_options(browser_quickjs PRIVATE -flto=full)
target_link_options(pxa_main PRIVATE -flto=full)

target_compile_options(pxa_script PRIVATE -flto=full)
# Interpreted code benefits from fewer instructions/operands rather than native
# inlining. The UI remains O3. Keep Wasm names in debug builds only.
target_compile_options(pxa_script PRIVATE -Oz)
target_link_options(pxa_script PRIVATE -flto=full)
target_link_options(pxa_script PRIVATE "$<$<CONFIG:Release>:-Wl,--strip-all>")
