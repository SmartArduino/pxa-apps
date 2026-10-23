/* PicoDrive license in vendor/picodrive/COPYING. Reuse native chunk packers,
 * with bounded memory callbacks and an identity/integrity envelope. */
#include "../common/state/snapshot.h"
#include "vendor/picodrive/pico/state.h"
#include <limits.h>
#include "pxa_md_state_fields.h"
#include "vendor/picodrive/pico/sound/ym2612.h"
#include "vendor/picodrive/pico/sound/mix.h"
size_t SN76496SnapshotSize(void);
void SN76496SnapshotSave(void *);
void SN76496SnapshotLoad(const void *);
typedef struct {
    uint32_t core_bytes,ram_bytes,width,height,stride,line,status;
    struct PicoTiming timing;
    struct PicoSound sound;
    YM2612 fm;
    uint32_t psg_bytes;
} md_state_metadata_t;
#define MD_STATE_META ((size_t)sizeof(md_state_metadata_t)+6*sizeof(int))
typedef struct {uint8_t *data;size_t size,pos;unsigned error;} md_state_stream_t;
static size_t md_state_write(void *data,size_t unit,size_t count,void *context) {
    md_state_stream_t *s=context;
    if(unit && count>SIZE_MAX/unit){s->error=1;return 0;}
    size_t n=unit*count;if(s->pos>s->size || n>s->size-s->pos){s->error=1;return 0;}
    if(s->data && n)memcpy(s->data+s->pos,data,n);s->pos+=n;return n;
}
static size_t md_state_read(void *data,size_t unit,size_t count,void *context) {
    md_state_stream_t *s=context;
    if(unit && count>SIZE_MAX/unit){s->error=1;return 0;}
    size_t n=unit*count;if(s->pos>s->size || n>s->size-s->pos){s->error=1;return 0;}
    if(n)memcpy(data,s->data+s->pos,n);s->pos+=n;return n;
}
static size_t md_state_eof(void *context) {md_state_stream_t *s=context;return s->pos==s->size;}
static int md_state_seek(void *context,long offset,int origin) {
    md_state_stream_t *s=context;size_t base=origin==SEEK_SET?0:origin==SEEK_CUR?s->pos:s->size;
    if(origin!=SEEK_SET && origin!=SEEK_CUR && origin!=SEEK_END)return -1;
    if((offset<0 && (uint64_t)(-(int64_t)offset)>base) || (offset>=0 && (uint64_t)offset>s->size-base)){s->error=1;return -1;}
    s->pos=offset<0?base-(size_t)(-(int64_t)offset):base+(size_t)offset;return 0;
}
static int md_state_pack(md_state_stream_t *s,unsigned save) {
    return PicoStateFP(s,save,md_state_read,md_state_write,md_state_eof,md_state_seek);
}
static uint32_t md_state_layout(void) {
    return 0x10000000u ^ (uint32_t)sizeof(Pico.m) ^ ((uint32_t)sizeof(Pico.video)<<8) ^ ((uint32_t)sizeof(void*)<<24) ^ (uint32_t)MD_STATE_META;
}
size_t emu_state_size(void) {
    if(!loaded)return 0;md_state_stream_t s={NULL,SIZE_MAX,0,0};
    if(md_state_pack(&s,1) || s.error || s.pos>UINT32_MAX-PXA_SNAPSHOT_HEADER-MD_STATE_META-emu_core_save_size()-SN76496SnapshotSize())return 0;
    return PXA_SNAPSHOT_HEADER+MD_STATE_META+s.pos+SN76496SnapshotSize()+emu_core_save_size();
}
const char *emu_state_save(void *data,size_t size) {
    size_t need=emu_state_size();if(!need || !data || size!=need)return "No loaded MD cartridge or wrong snapshot buffer";
    pxa_snapshot_begin(data,size,3,emu_core_checksum(),md_state_layout());
    uint8_t *p=(uint8_t*)data+PXA_SNAPSHOT_HEADER;size_t ram=emu_core_save_size(),psg=SN76496SnapshotSize(),bytes=size-PXA_SNAPSHOT_HEADER-MD_STATE_META-ram-psg;
    md_state_metadata_t *meta=(md_state_metadata_t*)p;
    meta->core_bytes=bytes;meta->ram_bytes=ram;meta->width=emu_core_info.width;meta->height=emu_core_info.height;
    meta->stride=emu_core_info.stride;meta->line=first_line;meta->status=Pico.video.status;pxa_md_state_copy_PicoTiming(&meta->timing,&Pico.t);pxa_md_state_copy_PicoSound(&meta->sound,&Pico.snd);meta->fm=ym2612;meta->psg_bytes=psg;
    mix_save_state((int *)(p+sizeof(*meta)));
    md_state_stream_t stream={p+MD_STATE_META,bytes,0,0};
    if(md_state_pack(&stream,1) || stream.error || stream.pos!=bytes)return "Cannot pack MD machine state";
    SN76496SnapshotSave(p+MD_STATE_META+bytes);
    if(ram && !emu_core_save_export(p+MD_STATE_META+bytes+psg,ram))return "Cannot pack MD cartridge RAM";
    pxa_snapshot_finish(data,size,emu_port_crc32);return NULL;
}
static int md_state_chunks_valid(const uint8_t *p,size_t size) {
    if(size<17 || memcmp(p,"PicoSEXT",8))return 0;
    size_t pos=12;uint32_t mandatory=0;
    while(size-pos>=5) {
        unsigned chunk=p[pos];uint32_t bytes=pxa_snapshot_u32(p+pos+1);pos+=5;
        if(bytes>size-pos || bytes>512u*1024)return 0;
        if(!chunk)return !bytes && pos==size && (mandatory&0x7fe)==0x7fe;
        if(chunk<11)mandatory|=1u<<chunk;
        pos+=bytes;
    }
    return 0;
}
const char *emu_state_load(const void *data,size_t size) {
    if(!loaded)return "No loaded MD cartridge";
    /* Chunk sizes can vary with the mapper/FM serializer. The envelope records
     * the actual length; validate it before the native parser changes anything. */
    size_t metadata=MD_STATE_META;
    uint32_t layout=md_state_layout();
    uint32_t legacy_layout=layout^(uint32_t)MD_STATE_META^(uint32_t)sizeof(md_state_metadata_t);
    if(data && size>=PXA_SNAPSHOT_HEADER && pxa_snapshot_u32((const uint8_t *)data+20)==legacy_layout){
        layout=legacy_layout;metadata=sizeof(md_state_metadata_t);
    }
    const char *error=pxa_snapshot_check(data,size,size,3,emu_core_checksum(),layout,emu_port_crc32);
    if(error)return error;if(size<PXA_SNAPSHOT_HEADER+metadata)return "Wrong MD snapshot length";
    const uint8_t *p=(const uint8_t*)data+PXA_SNAPSHOT_HEADER;
    const md_state_metadata_t *meta=(const md_state_metadata_t*)p;
    const int *mixer=metadata==MD_STATE_META?(const int *)(p+sizeof(*meta)):NULL;
    size_t bytes=meta->core_bytes,ram=meta->ram_bytes,psg=meta->psg_bytes;
    unsigned w=meta->width,h=meta->height,stride=meta->stride,line=meta->line;
    if(bytes>size-PXA_SNAPSHOT_HEADER-metadata || ram!=emu_core_save_size() || psg!=SN76496SnapshotSize() || bytes+ram+psg!=size-PXA_SNAPSHOT_HEADER-metadata ||
       !w || w>320 || !h || h>240 || stride!=w || line+h>256 || !md_state_chunks_valid(p+metadata,bytes) ||
       meta->fm.OPN.ST.clock!=ym2612.OPN.ST.clock || meta->fm.OPN.ST.rate!=ym2612.OPN.ST.rate || meta->sound.len!=Pico.snd.len || meta->sound.len_e_cnt<0 || meta->sound.len_e_cnt>=65536 ||
       (mixer && (mixer[0]!=4096 || mixer[3]!=4096)))
        return "Invalid MD snapshot machine state";
    pxa_md_state_copy_PicoTiming(&Pico.t,&meta->timing);
    md_state_stream_t stream={(uint8_t*)p+metadata,bytes,0,0};
    if(md_state_pack(&stream,0) || stream.error)return "Cannot restore MD machine state";
    if(ram && !emu_core_save_import(p+metadata+bytes+psg,ram))return "Cannot restore MD cartridge RAM";
    ym2612=meta->fm;SN76496SnapshotLoad(p+metadata+bytes);
    /* Earlier development states omitted filter history. Keep game progress
     * loadable; initialize only the missing output-filter history. */
    if(mixer)mix_load_state(mixer);else mix_reset(0);
    pxa_md_state_copy_PicoTiming(&Pico.t,&meta->timing);pxa_md_state_copy_PicoSound(&Pico.snd,&meta->sound);Pico.video.status=meta->status;
    emu_core_info.width=w;emu_core_info.height=h;emu_core_info.stride=stride;first_line=line;
    PicoDrawSetOutBuf(video,w*2);audio_frames=0;return NULL;
}
