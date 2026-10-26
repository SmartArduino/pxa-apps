cmake_minimum_required(VERSION 3.20)
project(pxa_cdogs C ASM)
# Upstream event messages contain large tagged unions, including on the stack.
set(PXA_RECROF_ENGINE_STACK_BYTES 1048576)
set(PXA_RECROF_CALLBACK_STACK_BYTES 65536)
include(../common/sdl-games/Build.cmake)
set(VERSION 2.5.0)
set(CDOGS_DATA_DIR /data/)
set(CDOGS_CFG_DIR .config/cdogs-sdl/)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated" "${CMAKE_CURRENT_BINARY_DIR}/generated/cdogs")
configure_file(vendor/src/cdogs/sys_config.h.cmake generated/cdogs/sys_config.h)
set(YAJL_MAJOR 2)
set(YAJL_MINOR 1)
set(YAJL_MICRO 1)
set(dollar $)
configure_file(vendor/src/cdogs/yajl/api/yajl_version.h.cmake generated/yajl/yajl_version.h)
file(GLOB yajl_headers vendor/src/cdogs/yajl/api/*.h)
foreach(header IN LISTS yajl_headers)
  get_filename_component(name "${header}" NAME)
  configure_file("${header}" "generated/yajl/${name}" COPYONLY)
endforeach()
file(READ vendor/src/cdogs/SDL_JoystickButtonNames/gamecontrollerbuttondb.txt DB)
string(REPLACE "\n" "\\n\\\n" DB "${DB}")
configure_file(vendor/src/cdogs/SDL_JoystickButtonNames/db.h.cmake generated/db.h)
set_source_files_properties(vendor/src/cdogs.c PROPERTIES COMPILE_DEFINITIONS main=pxa_sdl_game_main)
pxa_sdl_component(TITLE "C-Dogs SDL" SOURCES
  vendor/src/ammo_menu.c
  vendor/src/animated_counter.c
  vendor/src/autosave.c
  vendor/src/base64/base64.c
  vendor/src/briefing_screens.c
  vendor/src/cdogs.c
  vendor/src/command_line.c
  vendor/src/credits.c
  vendor/src/equip_menu.c
  vendor/src/game.c
  vendor/src/game_loop.c
  vendor/src/hiscores.c
  vendor/src/loading_screens.c
  vendor/src/mainmenu.c
  vendor/src/menu.c
  vendor/src/menu_utils.c
  vendor/src/namegen.c
  vendor/src/briefing_menu.c
  vendor/src/options_menu.c
  vendor/src/password.c
  vendor/src/pause_menu.c
  vendor/src/player_select_menus.c
  vendor/src/prep.c
  vendor/src/prep_equip.c
  vendor/src/screens_end.c
  vendor/src/util_menu.c
  vendor/src/weapon_menu.c
  vendor/src/cdogs/actor_fire.c
  vendor/src/cdogs/actor_pickup.c
  vendor/src/cdogs/actor_placement.c
  vendor/src/cdogs/actors.c
  vendor/src/cdogs/aheasing/easing.c
  vendor/src/cdogs/ai.c
  vendor/src/cdogs/ai_context.c
  vendor/src/cdogs/ai_coop.c
  vendor/src/cdogs/ai_utils.c
  vendor/src/cdogs/algorithms.c
  vendor/src/cdogs/ammo.c
  vendor/src/cdogs/animation.c
  vendor/src/cdogs/AStar.c
  vendor/src/cdogs/automap.c
  vendor/src/cdogs/blit.c
  vendor/src/cdogs/bullet_class.c
  vendor/src/cdogs/c_array.c
  vendor/src/cdogs/camera.c
  vendor/src/cdogs/campaign_entry.c
  vendor/src/cdogs/campaigns.c
  vendor/src/cdogs/character.c
  vendor/src/cdogs/character_class.c
  vendor/src/cdogs/collision/collision.c
  vendor/src/cdogs/collision/minkowski_hex.c
  vendor/src/cdogs/color.c
  vendor/src/cdogs/config.c
  vendor/src/cdogs/config_apply.c
  vendor/src/cdogs/config_io.c
  vendor/src/cdogs/config_json.c
  vendor/src/cdogs/config_old.c
  vendor/src/cdogs/cpic.c
  vendor/src/cdogs/damage.c
  vendor/src/cdogs/defs.c
  vendor/src/cdogs/door.c
  vendor/src/cdogs/draw/char_sprites.c
  vendor/src/cdogs/draw/draw.c
  vendor/src/cdogs/draw/draw_actor.c
  vendor/src/cdogs/draw/draw_buffer.c
  vendor/src/cdogs/draw/drawtools.c
  vendor/src/cdogs/draw/nine_slice.c
  vendor/src/cdogs/emitter.c
  vendor/src/cdogs/events.c
  vendor/src/cdogs/files.c
  vendor/src/cdogs/font.c
  vendor/src/cdogs/font_utils.c
  vendor/src/cdogs/game_events.c
  vendor/src/cdogs/game_mode.c
  vendor/src/cdogs/gamedata.c
  vendor/src/cdogs/grafx.c
  vendor/src/cdogs/grafx_bg.c
  vendor/src/cdogs/handle_game_events.c
  vendor/src/cdogs/hud/fps.c
  vendor/src/cdogs/hud/gauge.c
  vendor/src/cdogs/hud/health_gauge.c
  vendor/src/cdogs/hud/hud.c
  vendor/src/cdogs/hud/hud_num_popup.c
  vendor/src/cdogs/hud/player_hud.c
  vendor/src/cdogs/hud/wall_clock.c
  vendor/src/cdogs/joystick.c
  vendor/src/cdogs/json_utils.c
  vendor/src/cdogs/keyboard.c
  vendor/src/cdogs/log.c
  vendor/src/cdogs/los.c
  vendor/src/cdogs/map.c
  vendor/src/cdogs/map_archive.c
  vendor/src/cdogs/map_build.c
  vendor/src/cdogs/map_cave.c
  vendor/src/cdogs/map_classic.c
  vendor/src/cdogs/map_interior.c
  vendor/src/cdogs/map_new.c
  vendor/src/cdogs/map_object.c
  vendor/src/cdogs/map_static.c
  vendor/src/cdogs/map_wolf.c
  vendor/src/cdogs/material.c
  vendor/src/cdogs/mathc/mathc.c
  vendor/src/cdogs/mission.c
  vendor/src/cdogs/mission_convert.c
  vendor/src/cdogs/mission_static.c
  vendor/src/cdogs/mouse.c
  vendor/src/cdogs/music.c
  vendor/src/cdogs/net_client.c
  vendor/src/cdogs/net_server.c
  vendor/src/cdogs/net_util.c
  vendor/src/cdogs/objective.c
  vendor/src/cdogs/objs.c
  vendor/src/cdogs/palette.c
  vendor/src/cdogs/particle.c
  vendor/src/cdogs/particle_class.c
  vendor/src/cdogs/path_cache.c
  vendor/src/cdogs/pic.c
  vendor/src/cdogs/pic_manager.c
  vendor/src/cdogs/pickup.c
  vendor/src/cdogs/pickup_class.c
  vendor/src/cdogs/pics.c
  vendor/src/cdogs/player.c
  vendor/src/cdogs/player_template.c
  vendor/src/cdogs/powerup.c
  vendor/src/cdogs/quick_play.c
  vendor/src/cdogs/screen_shake.c
  vendor/src/cdogs/sounds.c
  vendor/src/cdogs/texture.c
  vendor/src/cdogs/thing.c
  vendor/src/cdogs/tile.c
  vendor/src/cdogs/tile_class.c
  vendor/src/cdogs/triggers.c
  vendor/src/cdogs/utils.c
  vendor/src/cdogs/vector.c
  vendor/src/cdogs/weapon.c
  vendor/src/cdogs/weapon_class.c
  vendor/src/cdogs/weapon_usage.c
  vendor/src/cdogs/window_context.c
  vendor/src/cdogs/XGetopt.c
  vendor/src/cdogs/yajl_utils.c
  vendor/src/cdogs/cwolfmap/audio.c
  vendor/src/cdogs/cwolfmap/audio_bs6.c
  vendor/src/cdogs/cwolfmap/audio_n3d.c
  vendor/src/cdogs/cwolfmap/audio_sod.c
  vendor/src/cdogs/cwolfmap/audio_wl1.c
  vendor/src/cdogs/cwolfmap/audio_wl6.c
  vendor/src/cdogs/cwolfmap/cwolfmap.c
  vendor/src/cdogs/cwolfmap/expand.c
  vendor/src/cdogs/cwolfmap/n3d.c
  vendor/src/cdogs/cwolfmap/vswap.c
  vendor/nuked-opl3/opl3.c
  opl_backend.c
  sound_cache_pxa.c
  pic_cache_pxa.c
  vendor/src/cdogs/yajl/yajl.c
  vendor/src/cdogs/yajl/yajl_lex.c
  vendor/src/cdogs/yajl/yajl_parser.c
  vendor/src/cdogs/yajl/yajl_buf.c
  vendor/src/cdogs/yajl/yajl_encode.c
  vendor/src/cdogs/yajl/yajl_gen.c
  vendor/src/cdogs/yajl/yajl_alloc.c
  vendor/src/cdogs/yajl/yajl_tree.c
  vendor/src/cdogs/yajl/yajl_version.c
  vendor/src/cdogs/c_hashmap/hashmap.c
  vendor/src/cdogs/SDL_JoystickButtonNames/SDL_joystickbuttonnames.c
  vendor/src/cdogs/cwolfmap/zip/zip.c
  vendor/src/cdogs/cwolfmap/wad/wad.c
  vendor/src/cdogs/cwolfmap/wad/waderrno.c
  vendor/src/json/json.c
  vendor/src/proto/msg.pb.c
  vendor/src/proto/nanopb/pb_common.c
  vendor/src/proto/nanopb/pb_decode.c
  vendor/src/proto/nanopb/pb_encode.c
  vendor/src/cdogs/enet/callbacks.c
  vendor/src/cdogs/enet/compress.c
  vendor/src/cdogs/enet/host.c
  vendor/src/cdogs/enet/inet_pton_mingw.c
  vendor/src/cdogs/enet/list.c
  vendor/src/cdogs/enet/packet.c
  vendor/src/cdogs/enet/peer.c
  vendor/src/cdogs/enet/protocol.c
  enet_platform.c
  INCLUDES . vendor/src vendor/src/cdogs vendor/src/proto/nanopb
    vendor/src/cdogs/enet/include vendor/src/cdogs/yajl
    "${CMAKE_CURRENT_BINARY_DIR}/generated/cdogs" "${CMAKE_CURRENT_BINARY_DIR}/generated"
  DEFINITIONS PXA_LAZY_TEXTURES=1 PXA_OFFLINE=1 PB_FIELD_16BIT=1 STATIC=1 YAJL_BUILD=1
    STB_IMAGE_STATIC=1 _GNU_SOURCE=1 MINIZ_NO_TIME=1 PORT_SDL_FPS=70)

target_link_options(pxa_main PRIVATE -Wl,--wrap=Mix_FreeChunk -Wl,--wrap=SDL_DestroyRenderer)
