#pragma once
#include <stdint.h>
#define OL_LEVEL_CAPACITY (2700000u)
#define OL_TRACKS_CAPACITY (3010000u)
extern uint8_t ol_level_data[OL_LEVEL_CAPACITY];
extern uint8_t ol_tracks_data[OL_TRACKS_CAPACITY];
extern uint8_t ol_title_data[240*160];
extern uint32_t ol_tracks_size;
extern uint16_t ol_palette[256];
const uint8_t *ol_pixels();
void ol_start(int level);
void ol_update(unsigned frames, uint32_t buttons);
void ol_render();
void ol_free();
int ol_next_level();
void ol_set_time(uint64_t us);
void ol_set_fps(int fps);
void ol_player_position(int32_t out[4]);
void ol_audio(int16_t *output, unsigned samples, unsigned rate);
bool ol_quick_save();
bool ol_quick_load();
void ol_draw_message(const char *message);
void ol_show_title();
void ol_draw_menu(unsigned selection, bool help, bool settings);
bool ol_toggle_setting(unsigned index);
bool ol_audio_active();
