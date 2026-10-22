/* UTF-8 directory entries use the real Guest FS codec and frontend scanner. */
static void library_test_entry(const char *name) {
    uint8_t payload[1100]={0},packet[1132];uint32_t size;size_t at=4,written;
    assert(pxa_wire_record_encode(payload+at,sizeof(payload)-at,4,
        (const uint8_t*)name,strlen(name),&written));at+=written;
    const uint8_t kind=PXA_FS_KIND_REGULAR;
    assert(pxa_wire_record_encode(payload+at,sizeof(payload)-at,5,&kind,1,&written));at+=written;
    const uint8_t file_size[8]={0};
    assert(pxa_wire_record_encode(payload+at,sizeof(payload)-at,6,file_size,8,&written));at+=written;
    assert(pxa_build_message(packet,sizeof(packet),PXA_FS_SERVICE,
        PXA_FS_READ_DIRECTORY,T_ENTRY,payload,(uint32_t)at,&size));
    assert(pxa_app_on_event(packet,size)==PXA_EVENT_HANDLED);
}
static void test_library_contract(const char *extension) {
    mode=MENU;source=1;names=rom_names[1];count=0;scan_pending=1;
    directory=media=UINT64_C(0x100000001);
    char first[32],last[32],chinese[769];
    snprintf(first,sizeof(first),"before.%s",extension);
    snprintf(last,sizeof(last),"after.%s",extension);
    memcpy(chinese,"中文",6);
    memset(chinese+6,'a',sizeof(chinese)-6);
    const size_t ext=strlen(extension);
    chinese[768-ext-1]='.';memcpy(chinese+768-ext,extension,ext);chinese[768]=0;
    library_test_entry(first);
    library_test_entry(chinese);
    library_test_entry(last);
    assert(count==3 && strlen(names[1])==768 && strcmp(names[1],chinese)==0);
    assert(strcmp(names[2],last)==0); // A long entry cannot hide later ROMs.
    for(unsigned i=0;i<200;++i) {
        char row[48];snprintf(row,sizeof(row),"game-%u.%s",i,extension);
        library_test_entry(row);
    }
    assert(count==203); // No fixed 128-row ceiling.
    library_test_entry("../invalid.sfc");
    assert(scan_pending && count==203); // Malformed row cannot end scanning.
    library_test_entry(last);assert(count==204);
    emu_rom_names_sort(names,count);
    for(unsigned i=1;i<count;++i)assert(strcmp(names[i-1],names[i])<=0);
    emu_rom_names_clear(&rom_names[1],&count,&name_capacity[1]);
    names=NULL;source_count[1]=0;directory=media=0;scan_pending=0;
}
