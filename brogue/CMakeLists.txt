cmake_minimum_required(VERSION 3.20)
project(pxa_brogue C ASM)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games")
find_program(WASM_OPT wasm-opt REQUIRED)
set(BROGUE_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
file(GLOB BROGUE_GAME CONFIGURE_DEPENDS "${BROGUE_UPSTREAM}/src/brogue/*.c" "${BROGUE_UPSTREAM}/src/variants/*.c")
# Preserve upstream bytes; adapt only observed traversal stack bookkeeping.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(BROGUE_ARCHITECT "${BROGUE_UPSTREAM}/src/brogue/Architect.c")
set(BROGUE_ARCHITECT_PORT "${CMAKE_CURRENT_BINARY_DIR}/pxa_architect.c")
list(REMOVE_ITEM BROGUE_GAME "${BROGUE_ARCHITECT}")
add_custom_command(OUTPUT "${BROGUE_ARCHITECT_PORT}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_architect.py"
    "${BROGUE_ARCHITECT}" "${BROGUE_ARCHITECT_PORT}"
  DEPENDS "${BROGUE_ARCHITECT}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_architect.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/insert_room_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/lake_fill_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/fill_lake_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/check_loopiness_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/audit_loop_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/flood_count_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/connect_cell_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/machine_interior_stack.c.inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/build_machine_stack.c.inc"
  VERBATIM)
list(APPEND BROGUE_GAME "${BROGUE_ARCHITECT_PORT}")
set(BROGUE_GRID "${BROGUE_UPSTREAM}/src/brogue/Grid.c")
set(BROGUE_GRID_PORT "${CMAKE_CURRENT_BINARY_DIR}/pxa_grid.c")
list(REMOVE_ITEM BROGUE_GAME "${BROGUE_GRID}")
add_custom_command(OUTPUT "${BROGUE_GRID_PORT}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_grid.py"
    "${BROGUE_GRID}" "${BROGUE_GRID_PORT}"
  DEPENDS "${BROGUE_GRID}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_grid.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/fill_region_stack.c.inc"
  VERBATIM)
list(APPEND BROGUE_GAME "${BROGUE_GRID_PORT}")
set(BROGUE_ITEMS "${BROGUE_UPSTREAM}/src/brogue/Items.c")
set(BROGUE_ITEMS_PORT "${CMAKE_CURRENT_BINARY_DIR}/pxa_items.c")
list(REMOVE_ITEM BROGUE_GAME "${BROGUE_ITEMS}")
add_custom_command(OUTPUT "${BROGUE_ITEMS_PORT}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_items.py"
    "${BROGUE_ITEMS}" "${BROGUE_ITEMS_PORT}"
  DEPENDS "${BROGUE_ITEMS}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_items.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/item_heat_stack.c.inc"
  VERBATIM)
list(APPEND BROGUE_GAME "${BROGUE_ITEMS_PORT}")
set(BROGUE_MOVEMENT "${BROGUE_UPSTREAM}/src/brogue/Movement.c")
set(BROGUE_MOVEMENT_PORT "${CMAKE_CURRENT_BINARY_DIR}/pxa_movement.c")
list(REMOVE_ITEM BROGUE_GAME "${BROGUE_MOVEMENT}")
add_custom_command(OUTPUT "${BROGUE_MOVEMENT_PORT}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_movement.py"
    "${BROGUE_MOVEMENT}" "${BROGUE_MOVEMENT_PORT}"
  DEPENDS "${BROGUE_MOVEMENT}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_movement.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/compat/fov_stack.c.inc"
  VERBATIM)
list(APPEND BROGUE_GAME "${BROGUE_MOVEMENT_PORT}")
# Release the large UI worker frames around the original gameplay calls.
foreach(ui_unit MainMenu IO)
  set(ui_source "${BROGUE_UPSTREAM}/src/brogue/${ui_unit}.c")
  set(ui_port "${CMAKE_CURRENT_BINARY_DIR}/pxa_${ui_unit}.c")
  list(REMOVE_ITEM BROGUE_GAME "${ui_source}")
  add_custom_command(OUTPUT "${ui_port}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_ui.py"
      "${ui_source}" "${ui_port}"
    DEPENDS "${ui_source}" "${BROGUE_UPSTREAM}/src/brogue/Rogue.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/compat/generate_ui.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/compat/mainBrogueJunction_dispatch.c.inc"
      "${CMAKE_CURRENT_SOURCE_DIR}/compat/mainInputLoop_dispatch.c.inc"
    VERBATIM)
  list(APPEND BROGUE_GAME "${ui_port}")
endforeach()
pxa_retro_component(pxa_main COMPONENT_ID main
  SOURCES main.c memfs.c stack_bridge.S ${BROGUE_GAME} "${BROGUE_UPSTREAM}/src/platform/platformdependent.c"
  INCLUDE_DIRS "${BROGUE_UPSTREAM}/src/brogue" "${BROGUE_UPSTREAM}/src/platform" "${BROGUE_UPSTREAM}/src/variants"
  LIBRARIES m)
target_compile_definitions(pxa_main PRIVATE BROGUE_EXTRA_VERSION="+104a20f-pxa" DATADIR=.)
target_compile_options(pxa_main PRIVATE -Wno-incompatible-pointer-types -Wno-int-conversion -fno-optimize-sibling-calls)
set_source_files_properties(stack_bridge.S PROPERTIES COMPILE_OPTIONS "--target=wasm32-wasip1")
# Keep linker names until the named Asyncify exclusion pass; own Binaryen here.
target_link_options(pxa_main PRIVATE --no-wasm-opt -Wl,--allow-undefined -Wl,-z,stack-size=1048576
  -Wl,--export=malloc -Wl,--export=free)
foreach(wasi_call fd_prestat_get fd_prestat_dir_name path_open fd_close fd_fdstat_get
    fd_fdstat_set_flags fd_read fd_write fd_seek fd_filestat_get path_filestat_get
    path_unlink_file path_remove_directory path_rename fd_readdir)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${wasi_call}")
endforeach()
add_custom_command(TARGET pxa_main POST_BUILD
  COMMAND "${WASM_OPT}" "$<TARGET_FILE:pxa_main>" --asyncify --coalesce-locals --pass-arg=asyncify-ignore-imports --pass-arg=asyncify-removelist@pxa_app_start,br_app_start,br_call_game,br_save_game_stack,br_run_game,pxa_app_on_event,pxa_app_stop -g --enable-bulk-memory --enable-sign-ext --enable-mutable-globals -o "$<TARGET_FILE:pxa_main>.async"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:pxa_main>.async" "$<TARGET_FILE:pxa_main>"
  VERBATIM)
