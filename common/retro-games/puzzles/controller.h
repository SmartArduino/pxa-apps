/* SPDX-License-Identifier: MIT. Included after the shared frontend helpers. */
static void puzzle_resume(void) {
    release_pointer(1);fe.menu=0;full_clip();memset(pixels,UI_BG,sizeof(pixels));
    board_clip();rect(fe.ox,fe.oy,fe.bw,fe.bh,0);midend_force_redraw(fe.me);fe.dirty=1;
}
static void puzzle_controller(uint32_t pressed) {
    if(pressed&PXA_CONTROLLER_START) {
        if(fe.menu){if(fe.started)puzzle_resume();else new_game();}
        else command(4);
        return;
    }
    if(fe.menu) {
        if(pressed&PXA_CONTROLLER_A)new_game();
        else if((pressed&PXA_CONTROLLER_B)&&fe.started)puzzle_resume();
        return;
    }
    static const uint32_t buttons[]={PXA_CONTROLLER_UP,PXA_CONTROLLER_DOWN,PXA_CONTROLLER_LEFT,PXA_CONTROLLER_RIGHT,PXA_CONTROLLER_A,PXA_CONTROLLER_B};
    static const int keys[]={CURSOR_UP,CURSOR_DOWN,CURSOR_LEFT,CURSOR_RIGHT,CURSOR_SELECT,CURSOR_SELECT2};
    for(unsigned i=0;i<6;++i)if(pressed&buttons[i])midend_process_key(fe.me,0,0,keys[i]);
    if(pressed&PXA_CONTROLLER_X)command(2);
    if(pressed&PXA_CONTROLLER_SELECT)command(3);
#if PXA_RETRO_KEYPAD
    if(fe.nkeys) {
        if(pressed&PXA_CONTROLLER_L)fe.pad_key=(fe.pad_key+fe.nkeys-1)%fe.nkeys;
        if(pressed&PXA_CONTROLLER_R)fe.pad_key=(fe.pad_key+1)%fe.nkeys;
        if(pressed&PXA_CONTROLLER_Y)midend_process_key(fe.me,0,0,fe.keys[fe.pad_key].button);
    }
#else
    if(pressed&PXA_CONTROLLER_Y)command(3);
#endif
    fe.dirty=1;
}
