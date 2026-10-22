/* GPL-2.0-or-later. Pause-only storage, bounded listing cache, no slot limit.
 * Files become visible only after a complete write and atomic no-replace rename. */
#define SLOT_CACHE 32u
enum { S_DIR=200,S_ENTRY,S_WRITE,S_RENAME,S_READ,S_SIZE,S_REWIND,S_REMOVE,S_MKDIR,S_DELETE };
static uint64_t slot_dir,slot_file,slot_total,slot_top,slot_selected,slot_base,slot_seen;
static unsigned slot_open_pending,slot_delete_pending;
static unsigned slot_focus; /* 0=list, 1=new, 2=delete, 3=back */
static unsigned slot_phase,slot_cached,slot_failed; /* 1=list,2=open,3=write,4=rename,5=open read,6=seek,7=read */
static uint8_t *slot_data;
static size_t slot_bytes,slot_used;
static char slot_names[SLOT_CACHE][64],slot_path[64],slot_tmp[72],slot_prefix[16];
static const char *slot_status;
static unsigned list_rows(unsigned top,unsigned bottom,unsigned pitch) {
    unsigned h=menu_height();return h>top+bottom?(h-top-bottom)/pitch:1;
}
static __attribute__((noinline,cold)) void slot_cleanup(void) {close_handle(&slot_file);free(slot_data);slot_data=NULL;slot_phase=0;}
static __attribute__((noinline,cold)) void slot_error(const char *why) {
    unsigned partial=slot_phase==3 || slot_phase==4;
    app_state_log(APP_STATE_TAG " STATE ERROR %s",why);slot_cleanup();close_handle(&slot_dir);
    if(partial)pxa_fs_request_path(PXA_FS_REMOVE,S_REMOVE,slot_tmp,strlen(slot_tmp));
    slot_failed=1;slot_status=UI_STATE_ERROR;redraw=1;
}
static __attribute__((noinline,cold)) void slots_scan(void) {
    if(slot_phase)return;
    slot_phase=1;slot_seen=slot_cached=0;slot_status=UI_SCANNING;
    slot_base=slot_top;
    snprintf(slot_prefix,sizeof(slot_prefix),APP_STATE_PREFIX "-%08x-",app_state_checksum());
    int r=pxa_fs_request_path(PXA_FS_MAKE_DIRECTORY,S_MKDIR,"states",6);
    if(r)slot_error("Cannot list snapshots");redraw=1;
}
static __attribute__((noinline,cold)) void slots_open(void) {
    if(slot_phase)return;
    if(save_pending || writing_save || save_replace_pending){slot_open_pending=1;redraw=1;return;}
    slot_open_pending=slot_delete_pending=0;
    mode=STATES;slot_focus=0;slot_top=slot_selected=slot_total=0;slot_failed=0;slot_status=UI_STATES;
    slots_scan();
}
static __attribute__((noinline,cold)) void slots_scroll(uint64_t top) {
    unsigned rows=list_rows(64,50,22);
    uint64_t max=slot_total>rows?slot_total-rows:0;if(top>max)top=max;
    if(slot_phase || slot_top==top)return;slot_top=top;redraw=1;
    if(top<slot_base || top+rows>slot_base+slot_cached)slots_scan();
}
static __attribute__((noinline,cold)) void slot_new(void) {
    if(slot_phase || !rom_loaded)return;
    slot_delete_pending=0;slot_bytes=app_state_size();slot_data=slot_bytes?malloc(slot_bytes):NULL;
    if(!slot_data){slot_error("No snapshot memory");return;}
    const char *error=app_state_save(slot_data,slot_bytes);
    if(error){slot_error(error);return;}
    struct timespec now;clock_gettime(CLOCK_REALTIME,&now);
    uint64_t stamp=(uint64_t)now.tv_sec*1000+now.tv_nsec/1000000;
    uint32_t nonce=(uint32_t)micros()^(uint32_t)rand();
    snprintf(slot_path,sizeof(slot_path),"states/" APP_STATE_PREFIX "-%08x-%016llx-%08x" APP_STATE_EXTENSION,app_state_checksum(),(unsigned long long)stamp,nonce);
    snprintf(slot_tmp,sizeof(slot_tmp),"%s.tmp",slot_path);
    slot_failed=0;slot_status=UI_SAVING;slot_phase=2;slot_used=0;redraw=1;
    int r=pxa_fs_request_open(S_WRITE,slot_tmp,strlen(slot_tmp),PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_EXCLUSIVE);
    if(r)slot_error("Cannot create snapshot");
}
static __attribute__((noinline,cold)) void slot_delete(void) {
    if(slot_phase || !slot_total || slot_selected<slot_base || slot_selected-slot_base>=slot_cached)return;
    if(!slot_delete_pending){slot_delete_pending=1;slot_status=UI_DELETE_CONFIRM;redraw=1;return;}
    snprintf(slot_path,sizeof(slot_path),"states/%s",slot_names[slot_selected-slot_base]);
    slot_delete_pending=0;slot_phase=8;slot_status=UI_LOADING;redraw=1;
    if(pxa_fs_request_path(PXA_FS_REMOVE,S_DELETE,slot_path,strlen(slot_path)))slot_error("Cannot remove snapshot");
}
static __attribute__((noinline,cold)) void slot_load(uint64_t index) {
    if(slot_phase || index<slot_base || index-slot_base>=slot_cached)return;
    slot_delete_pending=0;snprintf(slot_path,sizeof(slot_path),"states/%s",slot_names[index-slot_base]);
    slot_selected=index;slot_phase=5;slot_status=UI_LOADING;slot_failed=0;redraw=1;
    if(pxa_fs_request_open(S_READ,slot_path,strlen(slot_path),PXA_FS_OPEN_READ))slot_error("Cannot open snapshot");
}
static __attribute__((noinline,cold)) void slots_tick(void) {
    if(slot_open_pending) {
        if(mode!=PAUSED)slot_open_pending=0;
        else if(!save_pending && !writing_save && !save_replace_pending)slots_open();
    }
    if(slot_phase!=3 && slot_phase!=7)return;
    for(unsigned n=0;n<4 && (slot_phase==3 || slot_phase==7);++n) {
        unsigned chunk=slot_bytes-slot_used;if(chunk>4096)chunk=4096;
        int r=slot_phase==3?pxa_fs_write(slot_file,slot_data+slot_used,chunk):pxa_fs_read(slot_file,slot_data+slot_used,chunk);
        if(r==PXA_STATUS_WOULD_BLOCK)break;
        if(r<=0 || (unsigned)r>chunk){slot_error("Snapshot I/O failed");break;}
        slot_used+=r;if(slot_used!=slot_bytes)continue;
        close_handle(&slot_file);
        if(slot_phase==3) {
            uint8_t packet[240];uint32_t bytes;slot_phase=4;
            if(!pxa_fs_build_rename(packet,sizeof(packet),S_RENAME,slot_tmp,strlen(slot_tmp),slot_path,strlen(slot_path),&bytes) || pxa_control_packet(packet,bytes))slot_error("Snapshot commit failed");
        } else {
            const char *error=app_state_load(slot_data,slot_bytes);
            if(error){slot_error(error);break;}
            app_state_log(APP_STATE_TAG " STATE LOADED file=%s bytes=%u",slot_path,(unsigned)slot_bytes);
            slot_cleanup();release_input();app_state_reset_output();
            mode=PAUSED;pause_selected=0;pause_top=0;
            slot_status=UI_STATE_LOADED;redraw=1;
        }
    }
}
static __attribute__((noinline,cold)) int slots_event(const pxa_event_t *e) {
    if(e->service!=PXA_FS_SERVICE || e->token<S_DIR || e->token>S_DELETE)return 0;
    if(e->token==S_MKDIR) {
        if(pxa_fs_request_open(S_DIR,"states",6,PXA_FS_OPEN_READ|PXA_FS_OPEN_DIRECTORY))slot_error("Cannot open snapshot directory");
    } else if(e->token==S_DIR) {
        pxa_fs_open_result_t r;
        if(!pxa_fs_parse_open(e,S_DIR,&r) || r.status){slot_error("Snapshot listing failed");return 1;}
        slot_dir=r.handle;if(pxa_fs_request_read_directory(S_ENTRY,slot_dir))slot_error("Snapshot iterator failed");
    } else if(e->token==S_ENTRY) {
        pxa_fs_directory_result_t r;
        if(!pxa_fs_parse_directory(e,S_ENTRY,&r) || r.status){slot_error("Snapshot directory read failed");return 1;}
        if(r.end) {
            slot_total=slot_seen;close_handle(&slot_dir);slot_phase=0;if(!slot_total)slot_focus=1;
            slot_status=slot_failed?UI_STATE_ERROR:(slot_total?UI_STATES:UI_NO_STATES);redraw=1;
        } else {
            if(r.kind==PXA_FS_KIND_REGULAR && r.name_size==41 &&
               !memcmp(r.name,slot_prefix,12) && !memcmp(r.name+37,APP_STATE_EXTENSION,4)) {
                if(slot_seen>=slot_base && slot_cached<SLOT_CACHE) {
                    memcpy(slot_names[slot_cached],r.name,r.name_size);slot_names[slot_cached++][r.name_size]=0;
                }
                ++slot_seen;
            }
            if(pxa_fs_request_read_directory(S_ENTRY,slot_dir))slot_error("Snapshot iterator failed");
        }
    } else if(e->token==S_WRITE || e->token==S_READ) {
        pxa_fs_open_result_t r;
        if(!pxa_fs_parse_open(e,e->token,&r) || r.status){slot_error("Snapshot open failed");return 1;}
        slot_file=r.handle;slot_used=0;
        if(e->token==S_WRITE)slot_phase=3;
        else {slot_phase=6;if(pxa_fs_request_seek(S_SIZE,slot_file,0,PXA_FS_SEEK_END))slot_error("Snapshot size failed");}
    } else if(e->token==S_SIZE) {
        pxa_fs_seek_result_t r;size_t expected;
        if(!pxa_fs_parse_seek(e,S_SIZE,&r) || r.status || r.position<32 || r.position>SIZE_MAX){slot_error("Wrong snapshot length");return 1;}
        expected=(size_t)r.position;slot_bytes=expected;slot_data=malloc(expected);
        if(!slot_data){slot_error("No snapshot memory");return 1;}
        if(pxa_fs_request_seek(S_REWIND,slot_file,0,PXA_FS_SEEK_START))slot_error("Cannot rewind snapshot");
    } else if(e->token==S_REWIND) {
        pxa_fs_seek_result_t r;
        if(!pxa_fs_parse_seek(e,S_REWIND,&r) || r.status)slot_error("Cannot rewind snapshot");else slot_phase=7;
    } else if(e->token==S_DELETE) {
        int32_t status;
        if(!pxa_fs_parse_status(e,S_DELETE,PXA_FS_REMOVE,&status) || status)slot_error("Snapshot remove failed");
        else {slot_phase=0;slot_selected=slot_top=0;slot_failed=0;slots_scan();}
    } else if(e->token==S_RENAME) {
        int32_t status;
        if(!pxa_fs_parse_status(e,S_RENAME,PXA_FS_RENAME,&status) || status)slot_error("Atomic snapshot commit failed");
        else {app_state_log(APP_STATE_TAG " STATE SAVED file=%s bytes=%u",slot_path,(unsigned)slot_bytes);slot_cleanup();slot_top=slot_selected=0;slots_scan();}
    }
    return 1;
}
static __attribute__((noinline,cold)) void paint_slots(uint16_t *out) {
    unsigned w=menu_width(),h=menu_height(),rows=list_rows(64,50,22);
    text(out,32,36,slot_status?slot_status:UI_STATES,(slot_failed || slot_delete_pending)?UI_DANGER:UI_MUTED,32);
    for(unsigned row=0;row<rows;++row) {
        uint64_t index=slot_top+row;if(index>=slot_total || index<slot_base || index-slot_base>=slot_cached)break;
        unsigned y=64+row*22;char label[64];const char *name=slot_names[index-slot_base];
        uint64_t stamp=0;
        for(unsigned n=12;n<28;++n){unsigned c=(unsigned char)name[n];unsigned digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:0;stamp=(stamp<<4)|digit;}
        time_t seconds=stamp/1000;struct tm *tm=localtime(&seconds);char date[32]={0};
        if(tm)snprintf(date,sizeof(date),"%02d-%02d %02d:%02d:%02d",tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
        snprintf(label,sizeof(label),"%llu  %s",(unsigned long long)(index+1),date);
        card(out,24,y,w-48,20,index==slot_selected && !slot_focus?UI_FOCUS:UI_CARD);
        if(menu_commands)menu_label(32,y,w-64,20,label,UI_INK,0);else text(out,32,y+2,label,UI_INK,32);
    }
    if(slot_total<=UINT32_MAX)scroll_bar(out,slot_top,rows,slot_total,64,rows*22-2);
    unsigned bw=(w-56)/3;
    button(out,24,h-39,bw,UI_NEW_STATE,slot_focus==1?UI_ACCENT:UI_CARD);
    button(out,28+bw,h-39,bw,UI_DELETE_STATE,slot_focus==2?UI_ACCENT:UI_CARD);
    button(out,32+2*bw,h-39,bw,UI_BACK,slot_focus==3?UI_ACCENT:UI_CARD);
}
