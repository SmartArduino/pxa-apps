/* GPL-2.0-or-later. Cold menu navigation; gameplay keeps its direct pad path. */
static __attribute__((noinline,cold)) void menu_controller_action(uint32_t pressed) {
    if(mode==MENU) {
        if(pressed&PXA_CONTROLLER_B){request_exit();return;}
        if(scan_pending)return;
        if(pressed&PXA_CONTROLLER_Y){open_library();return;}
        if(!count && !menu_focus)menu_focus=1;
        if(pressed&PXA_CONTROLLER_UP) {
            if(menu_focus){if(count)menu_focus=0;}
            else if(selected)--selected;else menu_focus=1;
        } else if(pressed&PXA_CONTROLLER_DOWN) {
            if(!menu_focus){if(selected+1<count)++selected;else menu_focus=1;}
        } else if(pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT)) {
            if(menu_focus)menu_focus=(pressed&PXA_CONTROLLER_RIGHT)?2:1;
            else {unsigned other=1-source;if(source_count[other]){source=other;names=rom_names[source];count=source_count[source];selected=page=0;}}
        } else if(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START)) {
            if(menu_focus==1)open_library();else if(menu_focus==2)request_exit();else if(count)select_rom(selected);
        }
        unsigned rows=(menu_height()-108)/22;
        if(selected<page)page=selected;else if(selected>=page+rows)page=selected-rows+1;
    } else if(mode==PAUSED) {
        if(pressed&PXA_CONTROLLER_UP)pause_selected=(pause_selected+PAUSE_CHOICES-1)%PAUSE_CHOICES;
        else if(pressed&PXA_CONTROLLER_DOWN)pause_selected=(pause_selected+1)%PAUSE_CHOICES;
        else if(pressed&PXA_CONTROLLER_A)pause_action(pause_selected);
        else if(pressed&(PXA_CONTROLLER_B|PXA_CONTROLLER_START))pause_action(0);
        if(pause_selected<pause_top)pause_top=pause_selected;
        else if(pause_selected>=pause_top+pause_rows())pause_top=pause_selected-pause_rows()+1;
    } else if(mode==STATES) {
        if(slot_phase)return;
        if(pressed&PXA_CONTROLLER_B){mode=PAUSED;redraw=1;return;}
        if(pressed&PXA_CONTROLLER_X){slot_new();return;}
        if(!slot_total && !slot_focus)slot_focus=1;
        if(pressed&PXA_CONTROLLER_UP) {
            if(slot_focus){if(slot_total)slot_focus=0;}
            else if(slot_selected)--slot_selected;else slot_focus=1;
        } else if(pressed&PXA_CONTROLLER_DOWN) {
            if(!slot_focus){if(slot_selected+1<slot_total)++slot_selected;else slot_focus=1;}
        } else if(slot_focus && (pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT))) {
            slot_focus=(pressed&PXA_CONTROLLER_RIGHT)?2:1;
        } else if(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START)) {
            if(slot_focus==1)slot_new();else if(slot_focus==2)mode=PAUSED;else if(slot_total)slot_load(slot_selected);
        }
        unsigned rows=list_rows(64,50,22);
        if(slot_selected<slot_top)slots_scroll(slot_selected);
        else if(slot_selected>=slot_top+rows)slots_scroll(slot_selected-rows+1);
    } else if(mode==ERROR_SCREEN && (pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_B|PXA_CONTROLLER_X|PXA_CONTROLLER_START)))open_library();
    redraw=1;
}
static __attribute__((noinline,cold)) void menu_controller_event(uint32_t pressed,uint32_t buttons) {
    const uint32_t directions=PXA_CONTROLLER_UP|PXA_CONTROLLER_DOWN|PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT;
    uint32_t held=buttons&directions;
    if(held && (held&(held-1)))held=0; /* No repeat for contradictory/diagonal input. */
    if(held!=menu_repeat_buttons || menu_repeat_mode!=mode) {
        menu_repeat_buttons=held;menu_repeat_at=held?micros()+350000:0;menu_repeat_mode=mode;
    }
    if(pressed&~directions){menu_repeat_buttons=0;menu_repeat_at=0;}
    if(pressed)menu_controller_action(pressed);
}
static __attribute__((noinline,cold)) void menu_controller_repeat(uint64_t now) {
    if(!menu_repeat_buttons || !menu_repeat_at)return;
    if(background || mode!=menu_repeat_mode || !controller_connected[0]) {
        menu_repeat_buttons=0;menu_repeat_at=0;return;
    }
    if(now<menu_repeat_at)return;
    menu_repeat_at=now+90000; /* One step only; never catch up after slow storage. */
    menu_controller_action(menu_repeat_buttons);
}
