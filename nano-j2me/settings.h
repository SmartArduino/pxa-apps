/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "nj2me/nj_port.h"
#include <stddef.h>
#include <stdint.h>
#define NANO_PAD_BUTTONS 12
#define NANO_ACTION_MENU NJ_KEY_COUNT
#define NANO_SETTINGS_BYTES 36
#define NANO_GAME_PIXELS 307200u
typedef struct {
    uint16_t width,height,heap_kib,fps_cap;
    uint8_t touch_mode,alpha,button_size,numeric;
    uint8_t map[NANO_PAD_BUTTONS];
} nano_settings;
void nano_settings_default(nano_settings *s);
int nano_resolution_valid(unsigned w,unsigned h);
int nano_settings_encode(const nano_settings *s,uint8_t out[NANO_SETTINGS_BYTES]);
int nano_settings_decode(nano_settings *s,const uint8_t *bytes,size_t size);
int nano_settings_load(nano_settings *s,const char *path);
int nano_settings_save(const nano_settings *s,const char *path,int (*replace)(const char*,const char*));
const char *nano_action_name(unsigned action);
extern const unsigned nano_resolution_presets[][2];
extern const unsigned nano_resolution_count;
extern const char *const nano_pad_names[NANO_PAD_BUTTONS];

extern const unsigned nano_heap_presets[];
extern const unsigned nano_heap_count;
void nano_settings_phone_keys(nano_settings *settings);
