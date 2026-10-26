/* Exercise the real FS bridge: only complete snapshots prove absence. */
#include "port.h"
#include "pxa_fs.h"
#include "rpc.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static uint64_t now=1;
static unsigned directory_opens,private_opens,private_stats,media_opens,closes,entry;
static int directory_status,read_error,truncated,too_many,large_names;
static const char *override;
static pxa_event_t event;
static uint8_t reply[512];
static const uint64_t directory_handle=UINT64_C(0x10000000b),
    private_handle=UINT64_C(0x10000000c),media_handle=UINT64_C(0x10000000d);
static const pxa_event_t *response(unsigned opcode,int status,unsigned bytes) {
    memset(reply,0,sizeof(reply));pxa_store_u32(reply,(uint32_t)status);
    event=(pxa_event_t){PXA_FS_SERVICE,opcode,1,reply,bytes};return &event;
}
static void record(unsigned *offset,unsigned tag,const void *data,unsigned n) {
    pxa_store_u16(reply+*offset,tag);pxa_store_u16(reply+*offset+2,n);
    if(n)memcpy(reply+*offset+4,data,n);*offset+=4+n;
}
const pxa_event_t *port_rpc_packet(const uint8_t *p,uint32_t n) {
    pxa_wire_view_t v;assert(pxa_wire_decode(p,n,PXA_MAX_CONTROL_BYTES,&v));
    assert(v.service==PXA_FS_SERVICE);
    if(v.opcode==PXA_FS_REMOVE)return response(v.opcode,0,4);
    pxa_wire_record_view_t name;size_t consumed;
    assert(pxa_wire_record_decode(v.payload,v.payload_size,&name,&consumed));
    char path[256];assert(name.payload_size<sizeof(path));
    memcpy(path,name.payload,name.payload_size);path[name.payload_size]=0;
    if(v.opcode==PXA_FS_STAT) {
        ++private_stats;return response(v.opcode,PXA_STATUS_NOT_FOUND,4);
    }
    assert(v.opcode==PXA_FS_OPEN);
    if(!strcmp(path,"data")) {
        ++directory_opens;entry=0;
        response(v.opcode,directory_status,directory_status?4:12);
        if(!directory_status)pxa_store_u64(reply+4,directory_handle);
    } else {
        ++private_opens;
        int exists=override && !strcmp(path+5,override);
        response(v.opcode,exists?0:PXA_STATUS_NOT_FOUND,exists?12:4);
        if(exists)pxa_store_u64(reply+4,private_handle);
    }
    return &event;
}
const pxa_event_t *port_rpc(uint16_t s,uint16_t o,const void *p,uint32_t n) {
    assert(s==PXA_FS_SERVICE && o==PXA_FS_READ_DIRECTORY && n==8);
    assert(pxa_load_u64(p)==directory_handle);
    if(read_error)return response(o,PXA_STATUS_IO_ERROR,4);
    if(truncated)return response(o,0,5);
    response(o,0,4);unsigned offset=4;
    if((!override || entry++) && !too_many && !large_names)record(&offset,7,NULL,0);
    else {
        char name[256];
        if(large_names) {memset(name,'x',255);name[255]=0;}
        else if(too_many)snprintf(name,sizeof(name),"entry-%u",entry++);
        else snprintf(name,sizeof(name),"%s",override);
        record(&offset,4,name,strlen(name));
        uint8_t kind=PXA_FS_KIND_REGULAR,size[8]={0};
        record(&offset,5,&kind,1);record(&offset,6,size,8);
    }
    event.payload_size=offset;return &event;
}
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *p,uint32_t n) {
    (void)token;(void)p;assert(route==(PXA_CORE_SERVICE|(PXA_CORE_CLOSE_HANDLE<<16)) && n==8);
    ++closes;return 0;
}
int port_external_open(const char *p,uint64_t *h,uint32_t *size,uint64_t *features) {
    (void)p;++media_opens;*h=media_handle;*size=1;*features=0;return 1;
}
int port_external_open_status(void) {return 0;}
int port_external_stat(const char *p,uint8_t *kind,uint64_t *size) {
    (void)p;*kind=PXA_FS_KIND_REGULAR;*size=1;return 1;
}
uint64_t port_time_us(void) {return now;}
extern int __wrap_open(const char *,int,...),__wrap_close(int),__wrap_unlink(const char *);
extern int __wrap_stat(const char *,struct stat *);
static void read_file(const char *name) {int fd=__wrap_open(name,O_RDONLY);assert(fd>=0);assert(!__wrap_close(fd));}
static void invalidate(void) {assert(!__wrap_unlink("/data/unused"));}
int main(void) {
    port_filesystem_loading_cache(1);override="config.ini";
    read_file("/DATA/CONFIG.INI");assert(directory_opens==1 && private_opens==1 && !media_opens);
    unsigned before=private_opens;
    for(unsigned i=0;i<200;++i) {char s[80];snprintf(s,sizeof(s),"optional-%u.shp",i);read_file(s);}
    assert(private_opens==before && media_opens==200 && directory_opens==1);
    struct stat st;assert(!__wrap_stat("optional.ini",&st) && private_stats==0);
    read_file("nested/file.ini");assert(private_opens==before+1);
    override="override.ini";invalidate();read_file(override);
    assert(directory_opens==2 && private_opens==before+2 && media_opens==201);
    assert(__wrap_open("created.ini",O_WRONLY|O_CREAT)<0);
    read_file("after-create.ini");assert(directory_opens==3);
    for(unsigned mode=0;mode<5;++mode) {
        directory_status=mode==0?PXA_STATUS_IO_ERROR:0;
        read_error=mode==1;truncated=mode==2;too_many=mode==3;large_names=mode==4;
        invalidate();before=private_opens;read_file("fallback.ini");read_file("fallback.ini");
        assert(private_opens==before+2);
    }
    directory_status=read_error=truncated=too_many=large_names=0;
    /* A failed snapshot is retried after one second. */
    now+=1000000;before=private_opens;read_file("retry.ini");assert(private_opens==before);
    directory_status=PXA_STATUS_NOT_FOUND;invalidate();
    read_file("root-absent.ini");assert(private_opens==before);
    directory_status=0;override="arrived.ini";now+=300000000;
    read_file("arrived.ini");assert(private_opens==before+1);
    port_filesystem_loading_cache(0);read_file("normal.ini");assert(private_opens==before+2);
    assert(closes==media_opens+3+directory_opens-2);
    puts("PASS: 200 unique resource names skip private opens; private overrides, nested paths and writes retain priority");
    puts("PASS: mutation, TTL, missing root, I/O, malformed/truncated replies, entry/name capacity and gameplay fallback");
}
