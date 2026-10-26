# SDL lives entirely inside each signed guest; no SDL/game dependency is linked
# into the firmware. GPL-3.0-or-later (the shared recrof guest bridge).
include_guard(GLOBAL)
set(PXA_SDL_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(PXA_RECROF_MAIN_LOOP ON)
set(PXA_RECROF_NATIVE_PCM ON)
set(PXA_RECROF_BULK_READ_COPY ON)
if(NOT DEFINED PXA_RECROF_ENGINE_STACK_BYTES)
  set(PXA_RECROF_ENGINE_STACK_BYTES 131072)
endif()
if(NOT DEFINED PXA_RECROF_CALLBACK_STACK_BYTES)
  set(PXA_RECROF_CALLBACK_STACK_BYTES 16384)
endif()
include("${PXA_SDL_DIR}/../recrof/Engine.cmake")

function(pxa_sdl_component)
  cmake_parse_arguments(GAME "" "TITLE" "SOURCES;INCLUDES;DEFINITIONS" ${ARGN})
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  file(GLOB_RECURSE asset_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/media/*")
  set(index "${CMAKE_CURRENT_BINARY_DIR}/asset_index.c")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${PXA_SDL_DIR}/tools/asset_index.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/media" "${index}" COMMAND_ERROR_IS_FATAL ANY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${asset_files}
    "${PXA_SDL_DIR}/tools/asset_index.py")
  # Cooperative event ports own scheduling and PCM. SDL provides its actual
  # surface, texture, software renderer, event queue and audio codecs.
  foreach(opt SHARED TEST TESTS INSTALL ATOMIC ASSEMBLY CPUINFO THREADS TIMERS
      LOADSO POWER SENSOR HAPTIC HIDAPI FILESYSTEM LOCALE MISC
      OPENGL OPENGLES VULKAN METAL DISKAUDIO SYSTEM_ICONV)
    set(SDL_${opt} OFF CACHE BOOL "PXA guest" FORCE)
  endforeach()
  set(SDL_JOYSTICK ON CACHE BOOL "PXA virtual controller" FORCE)
  set(SDL_VIRTUAL_JOYSTICK ON CACHE BOOL "PXA virtual controller" FORCE)
  set(SDL_STATIC ON CACHE BOOL "PXA guest" FORCE)
  set(SDL_LIBC ON CACHE BOOL "PXA guest" FORCE)
  set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "PXA guest" FORCE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "PXA guest" FORCE)
  foreach(opt FOPEN64 FSEEKO64 FSEEKO FTELLO64 FTELLO)
    set(HAVE_${opt} FALSE CACHE INTERNAL "Use the wrapped guest stdio API" FORCE)
  endforeach()
  add_subdirectory("${PXA_SDL_DIR}/vendor/sdl2" sdl2 EXCLUDE_FROM_ALL)
  target_compile_definitions(SDL2-static PUBLIC SDL_MAIN_HANDLED=1)
  target_compile_options(SDL2-static PRIVATE -O3 -Wno-error -Wno-macro-redefined)
  set(BUILD_SHARED_LIBS OFF)
  foreach(opt INSTALL SAMPLES FLAC GME MOD MP3 MIDI OPUS CMD WAVPACK)
    set(SDL2MIXER_${opt} OFF CACHE BOOL "PXA guest" FORCE)
  endforeach()
  set(SDL2MIXER_VORBIS STB CACHE STRING "PXA guest" FORCE)
  set(SDL2MIXER_WAVE ON CACHE BOOL "PXA guest" FORCE)
  add_subdirectory("${PXA_SDL_DIR}/vendor/sdl-mixer" sdl-mixer EXCLUDE_FROM_ALL)
  target_compile_options(SDL2_mixer PRIVATE -O3 -Wno-error)
  pxa_recrof_component(TITLE "${GAME_TITLE}"
    SOURCES ${GAME_SOURCES} "${PXA_SDL_DIR}/platform.c" "${PXA_SDL_DIR}/image.c"
      "${PXA_SDL_DIR}/stdio.c"
      "${index}"
    INCLUDES ${GAME_INCLUDES} "${PXA_SDL_DIR}/include"
    DEFINITIONS ${GAME_DEFINITIONS} PORT_PACED_BY_MAIN=1 PORT_ENGINE_CONTROLLER=1 PORT_SDL_CONTROLLER=1 PORT_ASSET_INDEX=1 PORT_ASSET_ARCHIVE=1 PORT_PCM_FALLBACK=1
      "PORT_ASSET_PREFIX=\"assets/data/\"")
  get_target_property(sources pxa_main SOURCES)
  list(REMOVE_ITEM sources "${PXA_RECROF_DIR}/controller.c")
  set_property(TARGET pxa_main PROPERTY SOURCES "${sources}")
  target_sources(pxa_main PRIVATE "${PXA_SDL_DIR}/controller.c")
  target_link_libraries(pxa_main PRIVATE SDL2_mixer SDL2-static m)
  target_link_options(pxa_main PRIVATE -Wl,--wrap=fscanf -Wl,--wrap=fstat
    -Wl,--wrap=__wasi_fd_read -Wl,--wrap=__wasilibc_tell -Wl,--wrap=vfprintf -Wl,--wrap=realpath -Wl,--wrap=chmod -Wl,--wrap=lstat -Wl,--wrap=__assert_fail)
  foreach(fn Init InitSubSystem QuitSubSystem WasInit CreateWindow CreateRenderer
      RenderPresent UpdateWindowSurface UpdateWindowSurfaceRects GetTicks GetTicks64
      GetPerformanceCounter GetPerformanceFrequency Delay PollEvent WaitEvent
      WaitEventTimeout GetKeyboardState GetMouseState SetWindowFullscreen ShowSimpleMessageBox
      GetBasePath GetPrefPath OpenAudioDevice CloseAudioDevice PauseAudioDevice
      LockAudioDevice UnlockAudioDevice OpenAudio CloseAudio PauseAudio LockAudio UnlockAudio)
    target_link_options(pxa_main PRIVATE "-Wl,--wrap=SDL_${fn}")
  endforeach()
endfunction()
