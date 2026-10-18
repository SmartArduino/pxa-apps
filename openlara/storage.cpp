#include "storage.h"
#include "pxa_fs.h"
#include <string.h>

static const char *const paths[]={"data/settings.bin","data/save.bin"};
static const unsigned caps[]={128,8192};
static uint8_t cache[2][8192], work[8192+17];
static unsigned sizes[2], kind, used, length;
static uint64_t handle;
enum State { MKDIR, OPEN_READ, READING, IDLE, OPEN_WRITE, WRITING, COMMIT };
static State state=MKDIR;
static bool initialized;
static const char *message="";
static uint32_t crc(const uint8_t *p,unsigned n) {
    uint32_t c=~0u;
    while(n--) {c^=*p++;for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&-(c&1u));}
    return ~c;
}
static void close_file() {if(handle)pxa_close_handle(handle);handle=0;}
static void read_next() {
    close_file(); used=0;
    if(kind==2) {initialized=true;state=IDLE;return;}
    state=OPEN_READ;
    if(pxa_fs_request_open(100,paths[kind],strlen(paths[kind]),PXA_FS_OPEN_READ)) {
        ++kind;read_next();
    }
}
static void read_done() {
    if(used>=16 && !memcmp(work,"OLPX",4) && pxa_load_u32(work+4)==1 &&
       pxa_load_u32(work+8)==used-16 && used-16<=caps[kind] &&
       crc(work+16,used-16)==pxa_load_u32(work+12)) {
        sizes[kind]=used-16;memcpy(cache[kind],work+16,sizes[kind]);
    } else if(used) pxa_log_write(3,"OpenLara: rejected incomplete or corrupt private file");
    ++kind;read_next();
}
static void fail() {
    close_file();state=IDLE;message="SAVE FAILED - previous file kept";
    pxa_log_write(4,"OpenLara persistence failed; previous committed file retained");
}
void store_start() {
    state=MKDIR;initialized=false;kind=0;used=0;
    if(pxa_fs_request_path(PXA_FS_MAKE_DIRECTORY,101,"data",4))read_next();
}
bool store_ready() {return initialized;}
bool store_busy() {return initialized && state!=IDLE;}
const char *store_message() {return message;}
bool store_read(unsigned k,void *dest,unsigned capacity,unsigned *size) {
    if(k>1 || !sizes[k] || sizes[k]>capacity)return false;
    memcpy(dest,cache[k],sizes[k]);*size=sizes[k];return true;
}
bool store_write(unsigned k,const void *data,unsigned n) {
    if(!initialized || state!=IDLE || k>1 || !n || n>caps[k])return false;
    kind=k;length=n+16;used=0;memcpy(work,"OLPX",4);
    pxa_store_u32(work+4,1);pxa_store_u32(work+8,n);
    memcpy(work+16,data,n);pxa_store_u32(work+12,crc(work+16,n));
    message=k?"Saving game...":"Saving settings...";state=OPEN_WRITE;
    const char *tmp=k?"data/save.tmp":"data/settings.tmp";
    if(pxa_fs_request_open(102,tmp,strlen(tmp),PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE)) {fail();return false;}
    return true;
}
void store_tick() {
    if(state==READING) {
        int n=pxa_fs_read(handle,work+used,sizeof(work)-used);
        if(n==PXA_STATUS_WOULD_BLOCK)return;
        if(n<0 || n>int(sizeof(work)-used)) {used=0;read_done();return;}
        used+=unsigned(n);
        if(!n || used==sizeof(work))read_done();
    } else if(state==WRITING) {
        unsigned n=length-used;if(n>1024)n=1024;
        int r=pxa_fs_write(handle,work+used,n);
        if(r==PXA_STATUS_WOULD_BLOCK)return;
        if(r<=0 || unsigned(r)>n) {fail();return;}
        used+=unsigned(r);
        if(used==length) {
            close_file();uint8_t p[128];uint32_t bytes;
            const char *tmp=kind?"data/save.tmp":"data/settings.tmp";
            state=COMMIT;
            if(!pxa_fs_build_replace(p,sizeof(p),103,tmp,strlen(tmp),paths[kind],strlen(paths[kind]),&bytes) || pxa_control_packet(p,bytes))fail();
        }
    }
}
bool store_event(const pxa_event_t &e) {
    if(e.service!=PXA_FS_SERVICE || e.token<100 || e.token>103)return false;
    if(e.token==101 && state==MKDIR) {read_next();return true;}
    if(e.token==100 && state==OPEN_READ) {
        pxa_fs_open_result_t r;
        if(!pxa_fs_parse_open(&e,100,&r) || r.status) {++kind;read_next();}
        else {handle=r.handle;state=READING;}
    } else if(e.token==102 && state==OPEN_WRITE) {
        pxa_fs_open_result_t r;
        if(!pxa_fs_parse_open(&e,102,&r) || r.status)fail();
        else {handle=r.handle;state=WRITING;}
    } else if(e.token==103 && state==COMMIT) {
        int32_t status;
        if(!pxa_fs_parse_status(&e,103,PXA_FS_REPLACE,&status) || status)fail();
        else {
            sizes[kind]=length-16;memcpy(cache[kind],work+16,sizes[kind]);
            state=IDLE;message=kind?"Game saved":"Settings saved";
            pxa_log_write(2,kind?"OpenLara save committed":"OpenLara settings committed");
        }
    }
    return true;
}
void store_stop() {close_file();}
