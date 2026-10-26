cmake_minimum_required(VERSION 3.20)
project(pxa_brogue C ASM)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
find_program(WASM_OPT wasm-opt REQUIRED)
set(BROGUE_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
file(GLOB BROGUE_GAME CONFIGURE_DEPENDS "${BROGUE_UPSTREAM}/src/brogue/*.c" "${BROGUE_UPSTREAM}/src/variants/*.c")
pxa_retro_component(pxa_main COMPONENT_ID main
  SOURCES main.c memfs.c stack_bridge.S ${BROGUE_GAME} "${BROGUE_UPSTREAM}/src/platform/platformdependent.c"
  INCLUDE_DIRS "${BROGUE_UPSTREAM}/src/brogue" "${BROGUE_UPSTREAM}/src/platform" "${BROGUE_UPSTREAM}/src/variants"
  LIBRARIES m)
target_compile_definitions(pxa_main PRIVATE BROGUE_EXTRA_VERSION="+104a20f-pxa" DATADIR=.)
target_compile_options(pxa_main PRIVATE -Wno-incompatible-pointer-types -Wno-int-conversion -fno-optimize-sibling-calls)
set_source_files_properties(stack_bridge.S PROPERTIES COMPILE_OPTIONS "--target=wasm32-wasip1")
target_link_options(pxa_main PRIVATE -Wl,--allow-undefined -Wl,-z,stack-size=1048576
  -Wl,--export=malloc -Wl,--export=free)
foreach(wasi_call fd_prestat_get fd_prestat_dir_name path_open fd_close fd_fdstat_get
    fd_fdstat_set_flags fd_read fd_write fd_seek fd_filestat_get path_filestat_get
    path_unlink_file path_remove_directory path_rename fd_readdir)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${wasi_call}")
endforeach()
add_custom_command(TARGET pxa_main POST_BUILD
  COMMAND "${WASM_OPT}" "$<TARGET_FILE:pxa_main>" --asyncify --pass-arg=asyncify-ignore-imports --pass-arg=asyncify-removelist@pxa_app_start,br_app_start,br_call_game,br_save_game_stack,br_run_game,pxa_app_on_event,pxa_app_stop -g --enable-bulk-memory --enable-sign-ext --enable-mutable-globals -o "$<TARGET_FILE:pxa_main>.async"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:pxa_main>.async" "$<TARGET_FILE:pxa_main>"
  VERBATIM)
