cmake_minimum_required(VERSION 3.20)
project(pxa_whatajong C)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
pxa_retro_component(pxa_main COMPONENT_ID main
  SOURCES main.c qjs-platform.c libc-boundary.c quickjs/dtoa.c quickjs/cutils.c
    quickjs/libregexp.c quickjs/libunicode.c
  INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/quickjs" "${CMAKE_CURRENT_SOURCE_DIR}/compat"
  LIBRARIES m)
target_compile_definitions(pxa_main PRIVATE _GNU_SOURCE CONFIG_VERSION="2026-06-04")
target_compile_options(pxa_main PRIVATE -Wno-unused-function -Wno-unused-parameter)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=1048576 -Wl,--export=malloc -Wl,--export=free)
foreach(wasi_call fd_write fd_close fd_seek fd_fdstat_get)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${wasi_call}")
endforeach()
