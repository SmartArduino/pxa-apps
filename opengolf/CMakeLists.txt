cmake_minimum_required(VERSION 3.20)
project(pxa_opengolf C)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
pxa_retro_component(pxa_main COMPONENT_ID main
 SOURCES main.c platform.c data.c memfs.c vendor/golf/game.c vendor/golf/golf.c
 vendor/common/bvh.c vendor/common/maths.c vendor/common/level.c
 vendor/common/map.c vendor/common/vec.c vendor/3rd_party/parson/parson.c
 INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/compat" "${CMAKE_CURRENT_SOURCE_DIR}"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/3rd_party")
target_compile_options(pxa_main PRIVATE -Wno-unused-function -Wno-unused-variable)
target_link_options(pxa_main PRIVATE -Wl,--export=malloc -Wl,--export=free)
foreach(wasi_call fd_prestat_get fd_prestat_dir_name path_open fd_close fd_fdstat_get
    fd_fdstat_set_flags fd_read fd_write fd_seek fd_filestat_get path_filestat_get
    path_unlink_file path_remove_directory path_rename fd_readdir proc_exit)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${wasi_call}")
endforeach()
