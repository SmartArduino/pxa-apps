/* GPL-2.0-or-later. Cold list pointer input, shared by all seven frontends. */
static unsigned drag_active,drag_moved,drag_mode,drag_id;
static int drag_y;static uint64_t drag_top;
static __attribute__((noinline,cold)) void list_pointer(const pxa_ui_pointer_data_t *p,int x,int y) {
    if((mode==MENU && scan_pending) || (mode==STATES && slot_phase))return;
    unsigned top=mode==PAUSED?PAUSE_TOP:64,pitch=mode==PAUSED?PAUSE_PITCH:22;
    unsigned rows=mode==PAUSED?pause_rows():(menu_height()-(mode==STATES?114:108))/22;
    if(p->phase==PXA_POINTER_DOWN) {
        if(x<24 || x>=(int)menu_width()-24 || y<(int)top || y>=(int)(top+rows*pitch))return;
        if(mode==MENU)menu_focus=0;else if(mode==STATES)slot_focus=0;
        drag_active=1;drag_moved=0;drag_y=y;drag_mode=mode;drag_id=p->pointer_id;
        drag_top=mode==MENU?page:(mode==PAUSED?pause_top:slot_top);return;
    }
    if(!drag_active || drag_id!=p->pointer_id || drag_mode!=mode)return;
    int delta=drag_y-y;if(delta>6 || delta< -6)drag_moved=1;
    if(drag_moved) {
        int64_t n=(int64_t)drag_top+delta/(int)pitch;if(n<0)n=0;
        if(mode==MENU){unsigned max=count>rows?count-rows:0;page=n>max?max:n;}
        else if(mode==PAUSED){unsigned max=PAUSE_CHOICES>rows?PAUSE_CHOICES-rows:0;pause_top=n>max?max:n;}
        else slots_scroll(n);
        redraw=1;
    }
    if(p->phase==PXA_POINTER_UP) {
        drag_active=0;
        if(drag_moved || x<24 || x>=(int)menu_width()-24 || y<(int)top || y>=(int)(top+rows*pitch))return;
        unsigned row=(y-top)/pitch;
        if(mode==MENU)select_rom(page+row);
        else if(mode==PAUSED)pause_action(pause_top+row);
        else slot_load(slot_top+row);
    }
}
