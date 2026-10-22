/* GPL-2.0-or-later. Full UI theme wire events must not disturb gameplay. */
static void test_theme_contract(void) {
    uint16_t colors[]={UI_BG,UI_CARD,UI_EDGE,UI_FOCUS,UI_ACCENT,UI_ON_ACCENT,UI_INK,UI_MUTED,UI_DANGER};
    unsigned old_mode=mode,old_background=background;uint16_t old_pad=pad[0];
    mode=PLAYING;background=0;pad[0]=16;redraw=0;
    uint8_t payload[PXA_UI_THEME_WIRE_BYTES]={0},packet[160];uint32_t length;
    pxa_store_u32(payload,7);payload[4]=PXA_UI_COLOR_SCHEME_LIGHT;
    const uint32_t rgba[]={0xffffffff,0xeeeeeeff,0xff0000ff,0xffffffff,0x000000ff,0x808080ff,0x000000ff,0x00ff00ff,0xffff00ff,0xff0000ff};
    for(unsigned i=0;i<PXA_UI_THEME_COLOR_COUNT;++i)pxa_store_u32(payload+8+i*4,rgba[i]);
    for(unsigned i=0;i<PXA_UI_THEME_FONT_COUNT;++i)pxa_store_u16(payload+48+i*2,14);
    assert(pxa_build_message(packet,sizeof(packet),PXA_UI_SERVICE,PXA_UI_THEME_CHANGED,0,payload,sizeof(payload),&length));
    assert(pxa_app_on_event(packet,length)==PXA_EVENT_HANDLED);
    assert(UI_BG==0xffff && UI_INK==0 && UI_ACCENT==0xf800 && UI_ON_ACCENT==0xffff && UI_DANGER==0xf800);
    assert(UI_FOCUS!=UI_CARD && mode==PLAYING && pad[0]==16 && redraw);
    UI_BG=colors[0];UI_CARD=colors[1];UI_EDGE=colors[2];UI_FOCUS=colors[3];UI_ACCENT=colors[4];
    UI_ON_ACCENT=colors[5];UI_INK=colors[6];UI_MUTED=colors[7];UI_DANGER=colors[8];
    mode=old_mode;background=old_background;pad[0]=old_pad;
    puts("PASS: live system palette/on-primary contrast without gameplay/input changes");
}
