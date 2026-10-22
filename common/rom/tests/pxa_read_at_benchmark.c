/* GPL-2.0-or-later. Run only as an isolated test identity with fs/log.
 * Put an >= 8 MiB file named probe.bin in that app's SD media folder.
 * ABBA order separates cursor restoration from storage/cache warm-up. */
#include "pxa.h"
#include "pxa_fs_roots.h"
#include <stdio.h>
#include <time.h>
static uint64_t root,file,file_size,cookie;
static unsigned phase;
static uint8_t packet[24+4096];
static uint32_t crc_table[256],reference_crc;
static uint64_t micros(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static int at_end(void){return phase%4==0 || phase%4==3;}
static void log_failure(const char *why){char s[160];snprintf(s,sizeof(s),"ROM READ_AT BENCH FAIL phase=%u %s",phase,why);pxa_log_write(2,s);}
static int next_phase(void){return pxa_fs_request_seek(3,file,0,at_end()?PXA_FS_SEEK_END:PXA_FS_SEEK_START);}
int32_t pxa_app_start(const uint8_t *config,uint32_t size){
    (void)config;(void)size;phase=0;cookie=0;reference_crc=0;
    for(unsigned i=0;i<256;++i){uint32_t c=i;for(unsigned k=0;k<8;++k)c=c&1?(c>>1)^0xedb88320u:c>>1;crc_table[i]=c;}
    return pxa_fs_request_app_media(1);
}
int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t size){
    pxa_event_t e;if(!pxa_parse_event(bytes,size,&e) || e.service!=PXA_FS_SERVICE)return PXA_EVENT_UNHANDLED;
    if(e.token==1){
        pxa_fs_root_result_t r;
        if(!pxa_fs_parse_root(&e,1,&r) || r.status){log_failure("media root");return PXA_EVENT_HANDLED;}
        root=r.handle;pxa_fs_request_open_at(2,root,"probe.bin",9,PXA_FS_OPEN_READ);
    }else if(e.token==2){
        pxa_fs_at_result_t r;
        if(!pxa_fs_parse_open_at(&e,2,&r) || r.status || r.size<8u*1024u*1024u){log_failure("open/size");return PXA_EVENT_HANDLED;}
        file=r.handle;file_size=r.size;next_phase();
    }else if(e.token==3){
        pxa_fs_seek_result_t r;
        if(!pxa_fs_parse_seek(&e,3,&r) || r.status || r.position!=(at_end()?file_size:0)){log_failure("seek");return PXA_EVENT_HANDLED;}
        uint64_t sum=0,maximum=0;uint32_t crc=~0u;
        for(unsigned i=0;i<32;++i){
            uint64_t offset=(uint64_t)((i*67+29)%2048)*4096,t=micros();
            int n=pxa_fs_read_at(file,offset,++cookie,packet,sizeof(packet));
            t=micros()-t;sum+=t;if(t>maximum)maximum=t;
            if(n!=4096){log_failure("read");return PXA_EVENT_HANDLED;}
            for(unsigned k=24;k<sizeof(packet);++k)crc=crc_table[(crc^packet[k])&255]^(crc>>8);
        }
        crc=~crc;if(!phase)reference_crc=crc;
        if(crc!=reference_crc){log_failure("CRC mismatch");return PXA_EVENT_HANDLED;}
        char s[192];snprintf(s,sizeof(s),"ROM READ_AT BENCH phase=%u cursor=%s calls=32 bytes=131072 avg_us=%llu max_us=%llu crc=%08x",phase,at_end()?"end":"start",(unsigned long long)(sum/32),(unsigned long long)maximum,crc);pxa_log_write(2,s);
        pxa_fs_request_seek(4,file,0,PXA_FS_SEEK_CURRENT);
    }else if(e.token==4){
        pxa_fs_seek_result_t r;
        if(!pxa_fs_parse_seek(&e,4,&r) || r.status || r.position!=(at_end()?file_size:0)){log_failure("cursor changed");return PXA_EVENT_HANDLED;}
        if(++phase<8)next_phase();
        else {pxa_log_write(2,"ROM READ_AT BENCH PASS phases=8 cursor=preserved CRC=identical");pxa_close_handle(file);file=0;pxa_close_handle(root);root=0;}
    }else return PXA_EVENT_UNHANDLED;
    return PXA_EVENT_HANDLED;
}
void pxa_app_stop(uint32_t reason){(void)reason;if(file)pxa_close_handle(file);if(root)pxa_close_handle(root);file=root=0;}
