/* Host-selected fixed app media root. GPL-3.0-or-later. */
#include "external.h"
#include "port.h"
#include "rpc.h"
#include "pxa_fs_roots.h"
#include <limits.h>
#include <string.h>
#ifdef PORT_EXTERNAL_GAME_DATA
static uint64_t data_root;
static int open_status;
int port_external_open_status(void) { return open_status; }
int port_external_prepare(void) {
    uint8_t payload[28]={PXA_FS_ROOT_APP_MEDIA};
    pxa_fs_root_result_t root;
    const pxa_event_t *e=port_rpc(5,PXA_FS_OPEN_ROOT,payload,sizeof(payload));
    if(!e || !pxa_fs_parse_root(e,e->token,&root) || root.status) {
        port_error("Game data storage is unavailable.\nCheck the app's SD card and fixed data folder.");
        return 0;
    }
    data_root=root.handle;
    port_log("PXA SD fixed app media root ready; no directory picker");
    return 1;
}
static int status(const pxa_event_t *e) {
    return e && e->payload_size>=4 ? (int32_t)pxa_load_u32(e->payload) : PXA_STATUS_IO_ERROR;
}
static int relative(const char *path,uint8_t *out,size_t *size) {
    if(!data_root || strncmp(path,"data/",5) || !path[5]) return 0;
    path+=5;*size=strlen(path);if(*size>255) return 0;
    for(size_t i=0;i<*size;++i) out[i]=(path[i]>='A' && path[i]<='Z')?path[i]+('a'-'A'):path[i];
    return 1;
}
int port_external_open(const char *path,uint64_t *handle,uint32_t *size,uint64_t *features) {
    uint8_t payload[270];size_t length;pxa_fs_at_result_t file;
    open_status=PXA_STATUS_IO_ERROR;
    if(!relative(path,payload+14,&length)) return 0;
    pxa_store_u64(payload,data_root);pxa_store_u32(payload+8,1);
    pxa_store_u16(payload+12,length);
    const pxa_event_t *e=port_rpc(5,11,payload,14+length);
    if(!e || !pxa_fs_parse_open_at(e,e->token,&file)) return 0;
    open_status=file.status;
    if(file.status)return 0;
    if(file.kind!=1 || file.size>INT32_MAX) {pxa_close_handle(file.handle);return 0;}
    *handle=file.handle;*size=(uint32_t)file.size;*features=file.features;return 1;
}
int port_external_stat(const char *path,uint8_t *kind,uint64_t *size) {
    uint8_t payload[266];size_t length;
    if(!relative(path,payload+10,&length)) return 0;
    pxa_store_u64(payload,data_root);pxa_store_u16(payload+8,length);
    const pxa_event_t *e=port_rpc(5,16,payload,10+length);
    if(status(e) || e->payload_size!=20) return 0;
    *kind=e->payload[4];*size=pxa_load_u64(e->payload+12);return *kind==1 || *kind==2;
}
void port_external_shutdown(void) {
    if(data_root) pxa_close_handle(data_root);
    data_root=0;
}
#else
int port_external_prepare(void) {return 1;}
int port_external_open_status(void) {return PXA_STATUS_UNSUPPORTED;}
void port_external_shutdown(void) {}
int port_external_open(const char *p,uint64_t *h,uint32_t *s,uint64_t *f) {(void)p;(void)h;(void)s;(void)f;return 0;}
int port_external_stat(const char *p,uint8_t *k,uint64_t *s) {(void)p;(void)k;(void)s;return 0;}
#endif
