/* GPL-2.0-or-later. Cache semantic Host colors on cold configuration events. */
#pragma once
static uint16_t UI_BG=0x0863,UI_CARD=0x10c5,UI_EDGE=0x2148,UI_FOCUS=0x11ea,
    UI_ACCENT=0x4ed8,UI_ON_ACCENT=0x0863,UI_INK=0xef9f,UI_MUTED=0x8cb3,UI_DANGER=0xfb8c;
static uint16_t menu_theme_rgb(uint32_t rgba) {
    return (uint16_t)(((rgba>>16)&0xf800)|((rgba>>13)&0x07e0)|((rgba>>11)&0x001f));
}
static uint16_t menu_theme_blend(uint16_t a,uint16_t b) {
    unsigned r=(((a>>11)*3+(b>>11)+2)/4),g=((((a>>5)&63)*3+((b>>5)&63)+2)/4);
    unsigned blue=(((a&31)*3+(b&31)+2)/4);
    return (uint16_t)((r<<11)|(g<<5)|blue);
}
static __attribute__((noinline,cold)) void menu_theme_apply(const pxa_ui_theme_t *theme) {
    UI_BG=menu_theme_rgb(theme->rgba[PXA_UI_THEME_BACKGROUND]);
    UI_CARD=menu_theme_rgb(theme->rgba[PXA_UI_THEME_SURFACE]);
    UI_ACCENT=menu_theme_rgb(theme->rgba[PXA_UI_THEME_PRIMARY]);
    UI_ON_ACCENT=menu_theme_rgb(theme->rgba[PXA_UI_THEME_ON_PRIMARY]);
    UI_INK=menu_theme_rgb(theme->rgba[PXA_UI_THEME_TEXT]);
    UI_MUTED=menu_theme_rgb(theme->rgba[PXA_UI_THEME_MUTED]);
    UI_EDGE=menu_theme_rgb(theme->rgba[PXA_UI_THEME_BORDER]);
    UI_DANGER=menu_theme_rgb(theme->rgba[PXA_UI_THEME_DANGER]);
    UI_FOCUS=menu_theme_blend(UI_CARD,UI_ACCENT);
    redraw=1;
}
