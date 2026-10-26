cmake_minimum_required(VERSION 3.20)
project(pxa_neverputt C ASM)
include(../common/sdl-games/Build.cmake)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version.h" [=[#define VERSION "1.6.0-pxa"
]=])
set_source_files_properties(vendor/putt/main.c PROPERTIES COMPILE_DEFINITIONS main=pxa_sdl_game_main)
pxa_sdl_component(TITLE "Neverputt" SOURCES
  vendor/share/lang.c
  vendor/share/st_common.c
  vendor/share/vec3.c
  vendor/share/base_image.c
  vendor/share/image.c
  vendor/share/solid_base.c
  vendor/share/solid_vary.c
  vendor/share/solid_draw.c
  vendor/share/solid_all.c
  vendor/share/mtrl.c
  vendor/share/part.c
  vendor/share/geom.c
  vendor/share/ball.c
  vendor/share/base_config.c
  vendor/share/config.c
  vendor/share/video.c
  vendor/share/glext.c
  vendor/share/binary.c
  vendor/share/state.c
  vendor/share/gui.c
  vendor/share/font.c
  vendor/share/theme.c
  vendor/share/text.c
  vendor/share/common.c
  vendor/share/list.c
  vendor/share/fs_common.c
  vendor/share/fs_rwops.c
  vendor/share/dir.c
  vendor/share/fbo.c
  vendor/share/glsl.c
  vendor/share/array.c
  vendor/share/log.c
  vendor/putt/hud.c
  vendor/putt/game.c
  vendor/putt/hole.c
  vendor/putt/course.c
  vendor/putt/st_all.c
  vendor/putt/st_conf.c
  vendor/putt/main.c
  vendor/share/solid_sim_sol.c
  vendor/share/fs_stdio.c
  vendor/share/hmd_null.c
  audio_pxa.c snapshot.c "${PXA_SDL_DIR}/gl.c" "${PXA_SDL_DIR}/font.c"
  INCLUDES vendor/share vendor/putt "${CMAKE_CURRENT_BINARY_DIR}"
  DEFINITIONS PXA_WORLD_HALF_RES=1 CONFIG_DATA="/data" CONFIG_USER=".neverputt" ENABLE_NLS=0 ENABLE_HMD=0 PORT_SDL_JOYSTICK_AXES=1)
foreach(fn SetAttribute GetAttribute CreateContext DeleteContext MakeCurrent SetSwapInterval GetDrawableSize SwapWindow GetProcAddress)
  target_link_options(pxa_main PRIVATE "-Wl,--wrap=SDL_GL_${fn}")
endforeach()
