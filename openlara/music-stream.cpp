#include "music-stream.h"
#include "pxa_assets.h"
#include "pxa_fs_roots.h"
#include <string.h>

static constexpr unsigned CAPACITY=64*1024, CHUNK=PXA_ASSETS_READ_MAX_BYTES;
static constexpr uint64_t TOKEN_PREFIX=UINT64_C(0x4d55000000000000);
static uint8_t fifo[CAPACITY];
static uint64_t root, file, generation=TOKEN_PREFIX;
static uint32_t bank_size, begin_offset, size, received, consumed;
static bool opening, seeking, asset_pending, assets, failed;

static void close_file() {if(file)pxa_close_handle(file);file=0;}
void music_stream_config(uint64_t media_root,uint32_t bytes) {root=media_root;bank_size=bytes;}
void music_stream_stop() {
    ++generation;close_file();opening=seeking=asset_pending=false;
    received=consumed=size=0;failed=false;
}
static void fail() {
    failed=true;opening=seeking=asset_pending=false;close_file();
    pxa_log_write(3,"OpenLara: music media read failed");
}
static void request_asset() {
    if(asset_pending || failed || received==size)return;
    unsigned space=CAPACITY-(received-consumed), count=size-received;
    if(count>CHUNK)count=CHUNK;
    if(count>space)return;
    asset_pending=true;
    if(pxa_assets_read(generation,"assets/game/tracks.ad4",begin_offset+received,count))fail();
}
void music_stream_begin(uint32_t offset,uint32_t bytes) {
    music_stream_stop();begin_offset=offset;size=bytes;
    if(!bytes || offset<2048 || offset>bank_size || bytes>bank_size-offset) {failed=true;return;}
    assets=!root;
    if(assets)request_asset();
    else {
        opening=true;
        if(pxa_fs_request_open_at(generation,root,"tracks.ad4",10,PXA_FS_OPEN_READ))fail();
    }
}
void music_stream_tick() {
    if(failed || opening || seeking || received==size)return;
    if(assets) {request_asset();return;}
    if(!file)return;
    unsigned space=CAPACITY-(received-consumed), count=size-received;
    if(count>CHUNK)count=CHUNK;
    unsigned contiguous=CAPACITY-received%CAPACITY;
    if(count>contiguous)count=contiguous;
    if(count>space)count=space;
    if(!count)return;
    int n=pxa_fs_read(file,fifo+received%CAPACITY,count);
    if(n==PXA_STATUS_WOULD_BLOCK)return;
    if(n<=0 || unsigned(n)>count) {fail();return;}
    received+=n;
    if(received==size)close_file();
}
bool music_stream_event(const pxa_event_t &e) {
    if((e.token&UINT64_C(0xffff000000000000))!=TOKEN_PREFIX)return false;
    if(e.token!=generation) {
        // A level change can cancel an open while its reply is in flight.
        if(e.service==PXA_FS_SERVICE && e.opcode==PXA_FS_OPEN_AT) {
            pxa_fs_at_result_t r;
            if(pxa_fs_parse_open_at(&e,e.token,&r) && !r.status)pxa_close_handle(r.handle);
        }
        return true;
    }
    if(e.service==PXA_FS_SERVICE && e.opcode==PXA_FS_OPEN_AT) {
        opening=false;pxa_fs_at_result_t r;
        if(!pxa_fs_parse_open_at(&e,generation,&r) || r.status) {
            assets=true;request_asset();return true;
        }
        if(r.kind!=PXA_FS_KIND_REGULAR || r.size!=bank_size) {pxa_close_handle(r.handle);fail();return true;}
        file=r.handle;seeking=true;
        if(pxa_fs_request_seek(generation,file,begin_offset,PXA_FS_SEEK_START))fail();
    } else if(e.service==PXA_FS_SERVICE && e.opcode==PXA_FS_SEEK) {
        seeking=false;pxa_fs_seek_result_t r;
        if(!pxa_fs_parse_seek(&e,generation,&r) || r.status || r.position!=begin_offset)fail();
        else music_stream_tick();
    } else if(e.service==PXA_ASSETS_SERVICE) {
        asset_pending=false;pxa_asset_read_result_t r;
        if(!pxa_assets_parse_read(&e,generation,&r) || r.status || r.total_bytes!=bank_size ||
           r.offset!=begin_offset+received || !r.bytes || r.bytes>size-received ||
           r.bytes>CAPACITY-(received-consumed)) {fail();return true;}
        unsigned pos=received%CAPACITY, first=CAPACITY-pos;
        if(first>r.bytes)first=r.bytes;
        memcpy(fifo+pos,r.data,first);memcpy(fifo,r.data+first,r.bytes-first);
        received+=r.bytes;request_asset();
    }
    return true;
}
unsigned music_stream_read(uint8_t *out,unsigned count) {
    if(count>received-consumed)return 0;
    unsigned pos=consumed%CAPACITY,first=CAPACITY-pos;
    if(first>count)first=count;
    memcpy(out,fifo+pos,first);memcpy(out+first,fifo,count-first);consumed+=count;
    return count;
}
bool music_stream_failed() {return failed;}
