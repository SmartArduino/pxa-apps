cmake_minimum_required(VERSION 3.20)
project(pxa_blockout C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
pxa_retro_component(pxa_main COMPONENT_ID main
 SOURCES main.cpp compat/softgl.cpp compat/platform.cpp compat/memfs.c
 vendor/Game.cpp vendor/Pit.cpp vendor/PolyCube.cpp vendor/InitPolyCube.cpp
 vendor/BotPlayer.cpp vendor/BotPlayerAI.cpp vendor/BotMatrix.cpp
 vendor/SetupManager.cpp vendor/Sprites.cpp vendor/Utils.cpp
 vendor/GLApp/GLMatrix.cpp vendor/GLApp/GLSprite.cpp vendor/GLApp/GLFont.cpp
 INCLUDE_DIRS compat vendor
 LIBRARIES m)
target_compile_options(pxa_main PRIVATE -fno-exceptions -fno-rtti -Wno-writable-strings)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=1048576 -Wl,--export=malloc -Wl,--export=free)
foreach(call fd_prestat_get fd_prestat_dir_name path_open fd_close fd_fdstat_get fd_fdstat_set_flags fd_read fd_write fd_seek fd_filestat_get path_filestat_get path_unlink_file path_remove_directory path_rename fd_readdir proc_exit)
 target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${call}")
endforeach()
