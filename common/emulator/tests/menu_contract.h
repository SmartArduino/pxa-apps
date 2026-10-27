/* GPL-2.0-or-later. Actual frontend events, shared across all eight apps.
 * Adapters provide menu_test_reset() and menu_test_controller(player,bits,connected). */
static void menu_test_press(unsigned player,uint32_t buttons) {
    menu_test_controller(player,buttons,1);menu_test_controller(player,0,1);
}
static void test_menu_contract(void) {
    menu_test_reset();
    mode=MENU;background=0;scan_pending=0;source=0;source_count[0]=9;source_count[1]=2;
    count=9;names=rom_names[0];selected=page=0;menu_focus=MENU_FOCUS_LIST;
    menu_test_press(1,PXA_CONTROLLER_UP);assert(menu_focus==MENU_FOCUS_SOURCE);
    menu_test_press(1,PXA_CONTROLLER_RIGHT);
    assert(source==1 && count==2 && names==rom_names[1] && menu_focus==MENU_FOCUS_SOURCE);
    menu_test_press(1,PXA_CONTROLLER_A);assert(mode==MENU && menu_focus==MENU_FOCUS_LIST);
    menu_test_press(1,PXA_CONTROLLER_DOWN);assert(selected==1 && menu_focus==MENU_FOCUS_LIST);
    menu_test_press(1,PXA_CONTROLLER_DOWN);assert(menu_focus==MENU_FOCUS_REFRESH);
    menu_test_press(1,PXA_CONTROLLER_RIGHT);assert(menu_focus==MENU_FOCUS_IMPORT);
    menu_test_press(1,PXA_CONTROLLER_RIGHT);assert(menu_focus==MENU_FOCUS_EXIT);
    menu_test_press(1,PXA_CONTROLLER_DOWN);assert(menu_focus==MENU_FOCUS_SOURCE);
    menu_test_press(1,PXA_CONTROLLER_LEFT);assert(source==0 && count==9 && selected==0);
    menu_test_press(1,PXA_CONTROLLER_DOWN);assert(menu_focus==MENU_FOCUS_LIST);
    // Repeated zero-state refreshes from pad 1 must not cancel pad 2's hold.
    menu_test_controller(1,PXA_CONTROLLER_DOWN,1);assert(selected==1 && menu_repeat_at);
    uint64_t due=menu_repeat_at;
    menu_test_controller(0,0,1);assert(menu_repeat_at==due);
    menu_controller_repeat(due-1);assert(selected==1);
    menu_controller_repeat(due);assert(selected==2);
    menu_test_controller(0,0,0);menu_controller_repeat(due+90000);assert(selected==3);
    // Another actual press takes over; the old owner's release is harmless.
    menu_test_controller(0,PXA_CONTROLLER_DOWN,1);assert(selected==4);
    due=menu_repeat_at;menu_test_controller(1,0,1);assert(menu_repeat_at==due);
    menu_controller_repeat(due);assert(selected==5);
    menu_controller_repeat(due+90000);menu_controller_repeat(due+180000);
    assert(selected==7 && page && selected>=page && selected<page+(menu_height()-108)/22);
    menu_test_controller(0,0,0);assert(!menu_repeat_at);
    menu_controller_repeat(due+900000);assert(selected==7);
    // No hidden source can trap focus. An empty library can still refresh/exit.
    source_count[0]=0;source_count[1]=2;source=1;count=2;selected=0;menu_focus=MENU_FOCUS_SOURCE;
    menu_test_press(1,PXA_CONTROLLER_LEFT);assert(source==1 && count==2);
    source_count[0]=source_count[1]=count=0;selected=page=menu_focus=0;
    menu_test_press(1,PXA_CONTROLLER_RIGHT);assert(menu_focus==MENU_FOCUS_IMPORT);
    menu_test_press(1,PXA_CONTROLLER_RIGHT);assert(menu_focus==MENU_FOCUS_EXIT);
    menu_test_press(1,PXA_CONTROLLER_LEFT);assert(menu_focus==MENU_FOCUS_IMPORT);
    menu_test_press(1,PXA_CONTROLLER_LEFT);assert(menu_focus==MENU_FOCUS_REFRESH);
    menu_test_press(1,PXA_CONTROLLER_A);assert(scan_pending);
    // Pause uses the same confirm key and scrolls all choices into view.
    scan_pending=0;mode=PAUSED;pause_selected=pause_top=0;
    menu_test_press(1,PXA_CONTROLLER_UP);assert(pause_selected==PAUSE_CHOICES-1);
    assert(pause_top+pause_rows()>pause_selected);
    menu_test_press(1,PXA_CONTROLLER_DOWN);assert(!pause_selected && !pause_top);
    menu_test_press(1,PXA_CONTROLLER_B);assert(mode==PLAYING);
    assert(!menu_repeat_buttons && !pad[0] && !pad[1]);
    puts("PASS: both controllers navigate source tabs/list/footer/empty/pause; repeat takeover/release/disconnect");
}
