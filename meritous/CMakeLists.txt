cmake_minimum_required(VERSION 3.20)
project(pxa_meritous C ASM)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
find_program(WASM_OPT wasm-opt REQUIRED)
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/*.c")
list(FILTER GAME_SOURCES EXCLUDE REGEX "/audio\\.c$")
pxa_retro_component(pxa_main COMPONENT_ID main
  SOURCES main.c silent_audio.c compat/sdl_surface.c compat/memfs.c compat/stack_bridge.S ${GAME_SOURCES}
  INCLUDE_DIRS compat vendor
  LIBRARIES m)
target_compile_definitions(pxa_main PRIVATE _GNU_SOURCE)
target_compile_options(pxa_main PRIVATE -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-implicit-function-declaration -fno-optimize-sibling-calls)
set_source_files_properties(compat/stack_bridge.S PROPERTIES COMPILE_OPTIONS "--target=wasm32-wasip1")
target_link_options(pxa_main PRIVATE -Wl,--allow-undefined -Wl,-z,stack-size=4194304 -Wl,--export=malloc -Wl,--export=free)
foreach(wasi_call fd_prestat_get fd_prestat_dir_name path_open fd_close fd_fdstat_get
    fd_fdstat_set_flags fd_read fd_write fd_seek fd_filestat_get path_filestat_get
    path_unlink_file path_remove_directory path_rename fd_readdir proc_exit)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${wasi_call}")
endforeach()
add_custom_command(TARGET pxa_main POST_BUILD
  COMMAND "${WASM_OPT}" "$<TARGET_FILE:pxa_main>" --asyncify --pass-arg=asyncify-ignore-imports --pass-arg=asyncify-removelist@pxa_app_start,mt_app_start,mt_call_game,mt_get_stack_pointer,mt_run_game,pxa_app_on_event,pxa_app_stop --enable-bulk-memory --enable-sign-ext --enable-mutable-globals -o "$<TARGET_FILE:pxa_main>.async"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:pxa_main>.async" "$<TARGET_FILE:pxa_main>"
  VERBATIM)
