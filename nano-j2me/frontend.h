/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "settings.h"
int nano_frontend_init(void);
int nano_frontend_next_game(char name[256]);
void *nano_frontend_read_jar(const char *name,nj_u4 *size);
void nano_frontend_profile(uint64_t fingerprint,const char *name);
const nano_settings *nano_frontend_settings(void);
int nano_frontend_begin_game(void);
int nano_frontend_finish_game(int result,unsigned frames);
int nano_frontend_display(int *w,int *h,int *stride,void **pixels);
void nano_frontend_present(void);
void nano_frontend_cooperate(void);
int nano_frontend_poll(nj_event *event);
void nano_frontend_key(int code,int down);
void nano_frontend_pointer(int x,int y,unsigned phase,unsigned buttons,unsigned id);
void nano_frontend_cancel_input(void);
int nano_frontend_in_menu(void);
int nano_frontend_finished(void);
void nano_frontend_error(const char *message);

void nano_frontend_memory_warning(void);
