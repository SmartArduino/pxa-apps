/* GPL-2.0-or-later. Real menu state and FS result codec for all frontends. */
static void test_import_contract(void) {
    pxa_i18n_init(&i18n,&pxa_app_i18n_bundle);
    mode=MENU;scan_pending=import_pending=0;file=0;reading_save=0;rom_loaded=0;
    save_pending=writing_save=save_replace_pending=save_failed=0;
    import_roms();assert(import_pending && import_message[0]);
    unsigned old_selected=selected;menu_controller_action(PXA_CONTROLLER_DOWN);
    assert(selected==old_selected && import_pending);
    uint8_t body[20]={0};pxa_store_u32(body+4,2);pxa_store_u32(body+12,1);
    pxa_event_t event={.service=PXA_FS_SERVICE,.opcode=PXA_FS_IMPORT_FILES,.token=T_ROM_IMPORT,
        .payload=body,.payload_size=sizeof(body)};
    import_completed(&event);assert(!import_pending && scan_pending && preferred_source==1);
    assert(strstr(import_message,"2") && strstr(import_message,"1"));
    scan_pending=0;import_roms();assert(import_pending);
    event.payload_size=4;pxa_store_u32(body,(uint32_t)PXA_STATUS_CANCELLED);
    import_completed(&event);assert(!import_pending && scan_pending); /* retain partial batch */
    scan_pending=0;import_message[0]=0;
    assert(pxa_fs_request_import(T_ROM_IMPORT,0,".gba",4,"Copy",4)==PXA_STATUS_INVALID_ARGUMENT);
    assert(pxa_fs_request_import(T_ROM_IMPORT,0,"gba,",4,"Copy",4)==PXA_STATUS_INVALID_ARGUMENT);
    assert(!pxa_fs_request_import(T_ROM_IMPORT,0,"gba",3,"Copy",4));
    puts("PASS: import button, pending input isolation, count decoding, media refresh and cancelled-batch refresh");
}
