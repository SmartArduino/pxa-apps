cmake_minimum_required(VERSION 3.20)
project(pxa_jinyong C ASM)
set(PXA_CPP_SDK_DIR "")
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
get_filename_component(JY_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(JY_WORK "${JY_ROOT}/engine")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(JY_HOST_CC NAMES cc gcc clang REQUIRED)
set(JY_ASSET_METADATA "${CMAKE_CURRENT_BINARY_DIR}/jy_runtime_asset_index.c")
set(JY_SCRIPT_METADATA "${CMAKE_CURRENT_BINARY_DIR}/jy_script_asset_index.c")
set(JY_IMAGE_METADATA "${CMAKE_CURRENT_BINARY_DIR}/jy_image_asset_index.c")
file(GLOB JY_SCRIPT_COMPILER_INPUTS CONFIGURE_DEPENDS "${JY_WORK}/lua/*.[ch]")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${JY_ROOT}/assets/jinyong.bundle" "${JY_WORK}/jy_asset_index.c"
  "${JY_ROOT}/tools/compile_scripts.py" "${JY_ROOT}/tools/lua_compile.c"
  "${JY_ROOT}/tools/compile_images.py" "${JY_ROOT}/tools/image_compile.c"
  "${JY_ROOT}/tools/compile_asset_views.py"
  "${JY_ROOT}/compat/third_party/stb/stb_image.h"
  ${JY_SCRIPT_COMPILER_INPUTS})
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${JY_ROOT}/tools/compile_scripts.py"
  --root "${JY_ROOT}" --build-dir "${CMAKE_CURRENT_BINARY_DIR}/lua-scripts"
  --metadata "${JY_SCRIPT_METADATA}" --cc "${JY_HOST_CC}"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${JY_ROOT}/tools/compile_images.py"
  --root "${JY_ROOT}" --build-dir "${CMAKE_CURRENT_BINARY_DIR}/images"
  --input-index "${JY_SCRIPT_METADATA}" --metadata "${JY_IMAGE_METADATA}" --cc "${JY_HOST_CC}"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${JY_ROOT}/tools/compile_asset_views.py"
  --root "${JY_ROOT}" --input-index "${JY_IMAGE_METADATA}" --metadata "${JY_ASSET_METADATA}"
  COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_CURRENT_LIST_DIR}/game.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/font.cmake")
find_program(WASM_OPT wasm-opt REQUIRED)
# Only these four temporary Asyncify imports are permitted at the raw link.
# Binaryen must replace them; official final import validation still runs.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/jy-asyncify-imports.txt"
  "jy_async_start_unwind\njy_async_stop_unwind\njy_async_start_rewind\njy_async_stop_rewind\n")
pxa_add_component(pxa_main COMPONENT_ID main SOURCES
  frontend.c protected_bridge.c coroutine_bridge.S noaudio_device.c base_aliases.c pxa_fs_backend.c fatal.c guest_wasi.c guest_calls.c
  fs/jy_fs.c compat/jy_png.c compat/jy_rwops.c compat/jy_sdl_host.c compat/jy_surface.c
  ${JY_ENGINE_SOURCES} ${JY_LUA_SOURCES} ${JY_ZLIB_SOURCES}
  "${JY_ASSET_METADATA}"
  INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/compat/SDL" "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/fs" "${CMAKE_CURRENT_LIST_DIR}/compat"
    "${JY_WORK}/src" "${JY_WORK}/lua"
    "${CMAKE_CURRENT_LIST_DIR}/deps/zlib" "${CMAKE_CURRENT_LIST_DIR}/deps/font"
    "${CMAKE_CURRENT_LIST_DIR}/deps/font/vendor/SDL_ttf-2.0.11-git" "${JY_ROOT}/../common/retro-games"
  LIBRARIES jy_ttf freetype jy_ft_protected m)
target_compile_definitions(pxa_main PRIVATE JY_COOPERATIVE_LUA=1 JY_PXA_FILESYSTEM=1 JY_COOPERATIVE_FT=1 NO_GZIP)
target_compile_options(pxa_main PRIVATE -Oz -g -fno-optimize-sibling-calls -fno-strict-aliasing)
set_source_files_properties(${JY_ENGINE_SOURCES} PROPERTIES COMPILE_OPTIONS
  "-include;${CMAKE_CURRENT_LIST_DIR}/fs/jy_fs_redirect.h;-include;${CMAKE_CURRENT_LIST_DIR}/port_hooks.h")
# Keep the continuation boundary at its existing optimization level.
set_source_files_properties(protected_bridge.c PROPERTIES COMPILE_OPTIONS "-O1")
set_source_files_properties("${JY_WORK}/src/jymain.c" PROPERTIES COMPILE_DEFINITIONS "main=jy_game_main")
set_source_files_properties(${JY_LUA_SOURCES} PROPERTIES COMPILE_OPTIONS
  "-include;${CMAKE_CURRENT_LIST_DIR}/fs/jy_fs_redirect.h")
set_property(SOURCE "${JY_WORK}/lua/loslib.c" APPEND PROPERTY COMPILE_OPTIONS
  "-include;${CMAKE_CURRENT_LIST_DIR}/port_hooks.h")
target_link_options(pxa_main PRIVATE -O0 -Wl,--stack-first -Wl,-z,stack-size=196608
  "-Wl,--allow-undefined-file=${CMAKE_CURRENT_BINARY_DIR}/jy-asyncify-imports.txt"
  -Wl,--export=__stack_low -Wl,--export=__stack_high -Wl,--export=__data_end -Wl,--export=__heap_base
  -Wl,--export=malloc -Wl,--export=free -Wl,--export=__wasm_call_ctors)
foreach(symbol abort __assert_fail exit _Exit _exit __wasi_proc_exit)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=${symbol}")
endforeach()
target_link_options(pxa_main PRIVATE -Wl,--wrap=pxa_call -Wl,--wrap=pxa_io)
foreach(symbol environ_get environ_sizes_get fd_close fd_fdstat_get fd_read fd_seek fd_write)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${symbol}")
endforeach()
add_custom_command(TARGET pxa_main POST_BUILD
  COMMAND "${WASM_OPT}" "$<TARGET_FILE:pxa_main>" --asyncify --pass-arg=asyncify-ignore-imports
    "--pass-arg=asyncify-removelist@${JY_ASYNCIFY_REMOVE}"
    --coalesce-locals --simplify-locals --optimize-instructions --vacuum --dce -g
    --enable-bulk-memory --enable-sign-ext --enable-mutable-globals -o "$<TARGET_FILE:pxa_main>.async"
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:pxa_main>.async" "$<TARGET_FILE:pxa_main>"
  VERBATIM)
