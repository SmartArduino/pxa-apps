cmake_minimum_required(VERSION 3.20)
project(pxa_nano_j2me C ASM)
# The engine is a pinned, unmodified submodule. Only its public build options
# and nj_port interface are used by this application.
foreach(option BUILD_TESTS BUILD_CLI BUILD_LIBRETRO BUILD_SDL2 BUILD_EMBEDDED_PORTS BUILD_POSIX_NETWORK)
  set(NJ_${option} OFF CACHE BOOL "PXA guest" FORCE)
endforeach()
set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB TRUE)
set(NJ_ENABLE_JIT 0 CACHE STRING "Wasm cannot execute emitted native code" FORCE)
set(NJ_THREADED_DISPATCH 0 CACHE STRING "Portable Wasm dispatch" FORCE)
set(NJ_ENABLE_MEDIA 1 CACHE STRING "MIDP audio" FORCE)
set(NJ_ENABLE_FONT_CJK_AA 0 CACHE STRING "Embedded monochrome CJK" FORCE)
set(NJ_ENABLE_FONT_CJK 0 CACHE STRING "PXA system glyph provider" FORCE)
set(NJ_ENABLE_TTF 0 CACHE STRING "System owns font rasterization" FORCE)
add_subdirectory(upstream EXCLUDE_FROM_ALL)
# Upstream always adds its POSIX headless port. PXA provides a different port
# and does not have mmap/desktop directories; select only portable sources.
get_target_property(engine_sources nj2me SOURCES)
list(FILTER engine_sources EXCLUDE REGEX "^src/(port|client)/")
set_property(TARGET nj2me PROPERTY SOURCES "${engine_sources}")
# These sources directly use POSIX UDP and file metadata/truncation. Select
# their existing freestanding branches on WASI. Datagram and FileConnection
# access are unavailable; RMS still uses the nj_port file callbacks.
set_source_files_properties(upstream/src/midp/nj_lib_datagram.c
  upstream/src/midp/nj_lib_fc.c DIRECTORY upstream
  PROPERTIES COMPILE_DEFINITIONS NJ_USE_LIBC=0)
# Keep borrowed JAR ownership in the adapter without changing the submodule.
# Restrict the shim to the runner; Host malloc/free exports remain ordinary libc.
set_source_files_properties(upstream/src/runner/nj_run.c DIRECTORY upstream
  PROPERTIES COMPILE_DEFINITIONS free=nano_runner_free)
target_compile_options(nj2me PRIVATE -Os)
target_compile_definitions(nj2me PUBLIC NJ_MAX_HEAP=2097152)

# The existing application bridge supplies the Core 2.1 event loop, mapped
# surfaces, private stdio and PCM. It is linked into the guest only (GPL-3.0+).
set(PXA_RECROF_MAIN_LOOP ON)
set(PXA_RECROF_NATIVE_PCM ON)
set(PXA_RECROF_BULK_READ_COPY ON)
set(PXA_RECROF_ENGINE_STACK_BYTES 262144)
set(PXA_RECROF_CALLBACK_STACK_BYTES 32768)
include(../common/recrof/Engine.cmake)
pxa_recrof_component(TITLE "nano J2ME" SOURCES main.c system_font.c frontend.c layout.c settings.c input.c controls.c
  DEFINITIONS PORT_SDL_CONTROLLER=1 PORT_ENGINE_CONTROLLER=1 PORT_PACED_BY_MAIN=1 PORT_CUSTOM_FRONTEND=1)
get_target_property(bridge_sources pxa_main SOURCES)
list(REMOVE_ITEM bridge_sources "${PXA_RECROF_DIR}/controller.c")
set_property(TARGET pxa_main PROPERTY SOURCES "${bridge_sources}")
target_sources(pxa_main PRIVATE ../common/sdl-games/controller.c)
target_link_libraries(pxa_main PRIVATE nj2me m)
foreach(symbol port_control_key_at port_draw_controls port_controls_set_visible port_controls_visible)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=${symbol}")
endforeach()
