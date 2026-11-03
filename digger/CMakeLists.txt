cmake_minimum_required(VERSION 3.20)
project(pxa_digger C ASM)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(SRC "${ROOT}/engine")
find_program(WASM_OPT wasm-opt REQUIRED)
set(CORE main digger drawing sprite scores record edrf edrf_feed sound ini input monster bags alpha vgagrafx
 digger_math monster_obj digger_obj bullet_obj title_anim cgagrafx keyboard game netsim_stubs netsim_game netsim_debug state_hash netsim_friends sdl_timer)
set(SOURCES)
foreach(name IN LISTS CORE)
 list(APPEND SOURCES "${SRC}/${name}.c")
endforeach()
list(APPEND SOURCES "${SRC}/microsippy/src/usipy_str.c" "${SRC}/microsippy/src/external/mackron_md5/md5.c" "${SRC}/title-decoded.c")
pxa_add_component(pxa_main COMPONENT_ID main
 SOURCES frontend.c asyncify-driver.c coroutine_bridge.S engine-entry.c keyboard-host.c silent-device.c log-host.c
 session-fs.c raster.c surface-lite.c ${SOURCES}
 INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/compat" "${SRC}" "${SRC}/microsippy/src" "${SRC}/microsippy/src/external/mackron_md5" "${ROOT}/../common/retro-games"
 LIBRARIES m)
target_compile_definitions(pxa_main PRIVATE DIGGER_PXA=1 _WASI_EMULATED_PROCESS_CLOCKS=1)
target_compile_options(pxa_main PRIVATE -fno-optimize-sibling-calls -fwrapv
 "$<$<COMPILE_LANGUAGE:C>:-include>" "$<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_LIST_DIR}/compat/platform.h>")
set_source_files_properties(coroutine_bridge.S PROPERTIES COMPILE_OPTIONS "--target=wasm32-wasip1")
target_link_options(pxa_main PRIVATE -Wl,--allow-undefined -Wl,--stack-first -Wl,-z,stack-size=1114112 -Wl,--export=__stack_low -Wl,--export=__stack_high -Wl,--export=malloc -Wl,--export=free
 -Wl,--wrap=__assert_fail -Wl,--wrap=abort -Wl,--wrap=exit -Wl,--wrap=clock)
foreach(wasi fd_prestat_get fd_prestat_dir_name path_open fd_close fd_fdstat_get fd_fdstat_set_flags fd_read fd_write fd_seek fd_tell fd_filestat_get path_filestat_get fd_sync fd_datasync path_unlink_file path_remove_directory path_rename clock_time_get clock_res_get environ_sizes_get environ_get proc_exit)
 target_link_options(pxa_main PRIVATE "-Wl,--wrap=__wasi_${wasi}")
endforeach()
add_custom_command(TARGET pxa_main POST_BUILD
 COMMAND "${WASM_OPT}" "$<TARGET_FILE:pxa_main>" --asyncify --pass-arg=asyncify-ignore-imports
 --pass-arg=asyncify-removelist@pxa_app_start,pxa_app_on_event,pxa_app_stop,digger_drive,digger_coroutine_drive,digger_coroutine_start,digger_coroutine_resume,digger_coroutine_cancel,digger_stack_enter
 -g --enable-bulk-memory --enable-sign-ext --enable-mutable-globals -o "$<TARGET_FILE:pxa_main>.async"
 COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:pxa_main>.async" "$<TARGET_FILE:pxa_main>"
 VERBATIM)

