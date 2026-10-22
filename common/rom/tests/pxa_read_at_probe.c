/* GPL-2.0-or-later. Device smoke probe for legacy private-file read-at.
 * Run with fs/log services in an isolated test identity, never a game identity. */
#include "pxa.h"
#include "pxa_fs_roots.h"
#include <string.h>
static uint8_t data[4096],packet[24+4096];
static uint64_t file;
static int check(int ok){if(!ok)pxa_log_write(2,"ROM PRIVATE READ_AT PROBE FAIL");return ok;}
int32_t pxa_app_start(const uint8_t *config,uint32_t size){
    (void)config;(void)size;
    return pxa_fs_request_open(1,"probe.bin",9,PXA_FS_OPEN_READ|PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE);
}
int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t size){
    pxa_event_t e;if(!pxa_parse_event(bytes,size,&e))return PXA_EVENT_UNHANDLED;
    if(e.service!=PXA_FS_SERVICE || e.token!=1)return PXA_EVENT_UNHANDLED;
    pxa_fs_open_result_t opened;
    if(!check(pxa_fs_parse_open(&e,1,&opened) && !opened.status))return PXA_EVENT_HANDLED;
    file=opened.handle;
    for(unsigned i=0;i<sizeof(data);++i)data[i]=(uint8_t)(i*37+(i>>7));
    if(!check(pxa_fs_write(file,data,sizeof(data))==sizeof(data)))return PXA_EVENT_HANDLED;
    if(!check(pxa_fs_read_at(file,1023,1,packet,24+2048)==2048 && !memcmp(packet+24,data+1023,2048)))return PXA_EVENT_HANDLED;
    if(!check(pxa_fs_read(file,packet,1)==0))return PXA_EVENT_HANDLED;
    if(!check(pxa_fs_read_at(file,4090,2,packet,24+64)==6 && !memcmp(packet+24,data+4090,6)))return PXA_EVENT_HANDLED;
    if(!check(pxa_fs_cancel_read_at(file,3)==0 && pxa_fs_read_at(file,0,3,packet,28)==PXA_STATUS_CANCELLED))return PXA_EVENT_HANDLED;
    pxa_close_handle(file);file=0;pxa_log_write(2,"ROM PRIVATE READ_AT PROBE PASS bytes=4096 cursor=preserved short_read=6 cancellation=ok");
    return PXA_EVENT_HANDLED;
}
void pxa_app_stop(uint32_t reason){(void)reason;if(file)pxa_close_handle(file);file=0;}
