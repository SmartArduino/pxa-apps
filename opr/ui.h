/* SPDX-License-Identifier: MIT. Original PXA frontend for the actual OPR core. */
#ifndef OPR_PXA_UI_H
#define OPR_PXA_UI_H
#include "session.h"
enum { OPR_UI_SELECT, OPR_UI_PLAY, OPR_UI_PAUSE, OPR_UI_HELP };
typedef struct {
    opr_session session;
    uint32_t seed, controller;
    int scene, selected_class, background, help_return;
    char message[80];
} opr_ui;
void opr_ui_init(opr_ui *u);
void opr_ui_pointer(opr_ui *u, int x, int y);
void opr_ui_controller(opr_ui *u, uint32_t buttons);
void opr_ui_back(opr_ui *u);
void opr_ui_background(opr_ui *u, int background);
void opr_ui_action(opr_ui *u, opr_action action);
void opr_ui_render(const opr_ui *u, unsigned char pixels[400*416]);
extern const uint16_t opr_ui_palette[16];
#endif
