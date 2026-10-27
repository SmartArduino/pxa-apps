/* GPL-2.0-or-later. Cold menu navigation; gameplay keeps its direct pad path. */
static __attribute__((noinline,cold)) void menu_controller_action(uint32_t pressed) {
    if(mode==MENU) {
        if(import_pending)return;
        if(pressed&PXA_CONTROLLER_B){request_exit();return;}
        if(scan_pending)return;
        if(pressed&PXA_CONTROLLER_X){import_roms();return;}
        if(pressed&PXA_CONTROLLER_Y){open_library();return;}
        if(!count && menu_focus==MENU_FOCUS_LIST)menu_focus=MENU_FOCUS_REFRESH;
        const unsigned has_tabs=source_count[0] || source_count[1];
        if(pressed&PXA_CONTROLLER_UP) {
            if(menu_focus==MENU_FOCUS_SOURCE)menu_focus=MENU_FOCUS_REFRESH;
            else if(menu_focus!=MENU_FOCUS_LIST)menu_focus=count?MENU_FOCUS_LIST:(has_tabs?MENU_FOCUS_SOURCE:MENU_FOCUS_REFRESH);
            else if(selected)--selected;
            else menu_focus=has_tabs?MENU_FOCUS_SOURCE:MENU_FOCUS_REFRESH;
        } else if(pressed&PXA_CONTROLLER_DOWN) {
            if(menu_focus==MENU_FOCUS_SOURCE)menu_focus=count?MENU_FOCUS_LIST:MENU_FOCUS_REFRESH;
            else if(menu_focus!=MENU_FOCUS_LIST)menu_focus=has_tabs?MENU_FOCUS_SOURCE:(count?MENU_FOCUS_LIST:MENU_FOCUS_REFRESH);
            else if(selected+1<count)++selected;
            else menu_focus=MENU_FOCUS_REFRESH;
        } else if(pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT|PXA_CONTROLLER_L|PXA_CONTROLLER_R)) {
            if(menu_focus==MENU_FOCUS_REFRESH || menu_focus==MENU_FOCUS_IMPORT || menu_focus==MENU_FOCUS_EXIT) {
                if(pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT))
                    menu_focus=(pressed&PXA_CONTROLLER_RIGHT)?
                        (menu_focus==MENU_FOCUS_REFRESH?MENU_FOCUS_IMPORT:MENU_FOCUS_EXIT):
                        (menu_focus==MENU_FOCUS_EXIT?MENU_FOCUS_IMPORT:MENU_FOCUS_REFRESH);
            }
            if(menu_focus==MENU_FOCUS_LIST || menu_focus==MENU_FOCUS_SOURCE ||
               (pressed&(PXA_CONTROLLER_L|PXA_CONTROLLER_R))) {
                unsigned next=(pressed&(PXA_CONTROLLER_RIGHT|PXA_CONTROLLER_R))?1:0;
                if(source_count[next] && source!=next) {
                    source=next;names=rom_names[source];count=source_count[source];selected=page=0;
                }
                if(has_tabs)menu_focus=MENU_FOCUS_SOURCE;
            }
        } else if(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START)) {
            if(menu_focus==MENU_FOCUS_REFRESH)open_library();
            else if(menu_focus==MENU_FOCUS_IMPORT)import_roms();
            else if(menu_focus==MENU_FOCUS_EXIT)request_exit();
            else if(menu_focus==MENU_FOCUS_SOURCE)menu_focus=count?MENU_FOCUS_LIST:MENU_FOCUS_REFRESH;
            else if(count)select_rom(selected);
        }
        unsigned rows=(menu_height()-108)/22;if(!rows)rows=1;
        if(selected<page)page=selected;else if(selected>=page+rows)page=selected-rows+1;
    } else if(mode==PAUSED) {
        if(pressed&PXA_CONTROLLER_UP)pause_selected=(pause_selected+PAUSE_CHOICES-1)%PAUSE_CHOICES;
        else if(pressed&PXA_CONTROLLER_DOWN)pause_selected=(pause_selected+1)%PAUSE_CHOICES;
        else if(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START))pause_action(pause_selected);
        else if(pressed&PXA_CONTROLLER_B)pause_action(0);
        if(pause_selected<pause_top)pause_top=pause_selected;
        else if(pause_selected>=pause_top+pause_rows())pause_top=pause_selected-pause_rows()+1;

    }
    else if(mode==STATES) {
        if(slot_phase)return;
        if(pressed&PXA_CONTROLLER_B){slot_delete_pending=0;mode=PAUSED;redraw=1;return;}
        if(pressed&PXA_CONTROLLER_X){slot_new();return;}
        if(pressed&PXA_CONTROLLER_Y){slot_delete();return;}
        if(pressed&(PXA_CONTROLLER_UP|PXA_CONTROLLER_DOWN|PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT|PXA_CONTROLLER_L|PXA_CONTROLLER_R)){slot_delete_pending=0;slot_status=UI_STATES;}
        if(!slot_total && !slot_focus)slot_focus=1;
        if(pressed&PXA_CONTROLLER_UP) {
            if(slot_focus){if(slot_total)slot_focus=0;}
            else if(slot_selected)--slot_selected;else slot_focus=1;
        } else if(pressed&PXA_CONTROLLER_DOWN) {
            if(!slot_focus){if(slot_selected+1<slot_total)++slot_selected;else slot_focus=1;}
        } else if(!slot_focus && (pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT|PXA_CONTROLLER_L|PXA_CONTROLLER_R))) {
            uint64_t step=list_rows(64,50,22);
            if(pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_L))slot_selected=slot_selected>step?slot_selected-step:0;
            else if(slot_total)slot_selected=step<slot_total-1-slot_selected?slot_selected+step:slot_total-1;
        } else if(slot_focus && (pressed&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT))) {
            slot_focus=(pressed&PXA_CONTROLLER_RIGHT)?(slot_focus==3?1:slot_focus+1):(slot_focus==1?3:slot_focus-1);
        } else if(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START)) {
            if(slot_focus==1)slot_new();else if(slot_focus==2)slot_delete();else if(slot_focus==3){slot_delete_pending=0;mode=PAUSED;}else if(slot_total)slot_load(slot_selected);
        }
        unsigned rows=list_rows(64,50,22);
        if(slot_selected<slot_top)slots_scroll(slot_selected);
        else if(slot_selected>=slot_top+rows)slots_scroll(slot_selected-rows+1);

    }
    else if(mode==ERROR_SCREEN && (pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_B|PXA_CONTROLLER_X|PXA_CONTROLLER_START)))open_library();
    redraw=1;
}
static __attribute__((noinline,cold)) void menu_controller_event(unsigned controller,uint32_t pressed,uint32_t buttons) {
    const uint32_t directions=PXA_CONTROLLER_UP|PXA_CONTROLLER_DOWN|PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT;
    uint32_t held=buttons&directions;
    if(held && (held&(held-1)))held=0; /* No repeat for contradictory/diagonal input. */
    /* Any supported pad may move the single UI cursor. Idle refreshes/releases
     * from another pad must not cancel the current owner's held direction. */
    if(pressed || controller==menu_repeat_controller) {
        if(controller!=menu_repeat_controller || held!=menu_repeat_buttons || menu_repeat_mode!=mode) {
            menu_repeat_buttons=held;menu_repeat_at=held?micros()+350000:0;menu_repeat_mode=mode;
        }
        menu_repeat_controller=controller;
        if(pressed&~directions){menu_repeat_buttons=0;menu_repeat_at=0;}
    }
    if(pressed)menu_controller_action(pressed);
}
static __attribute__((noinline,cold)) void menu_controller_repeat(uint64_t now) {
    if(!menu_repeat_buttons || !menu_repeat_at)return;
    if(background || mode!=menu_repeat_mode || !controller_connected[menu_repeat_controller]) {
        menu_repeat_buttons=0;menu_repeat_at=0;return;
    }
    if(now<menu_repeat_at)return;
    menu_repeat_at=now+90000; /* One step only; never catch up after slow storage. */
    menu_controller_action(menu_repeat_buttons);
}
