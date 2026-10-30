// SPDX-License-Identifier: MIT
// Exercise the production media runtime through real SDK wire codecs.
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <vector>
static uint64_t test_us=1000000;
static int test_clock(clockid_t,timespec *t){t->tv_sec=time_t(test_us/1000000);t->tv_nsec=long(test_us%1000000)*1000;return 0;}
#define clock_gettime test_clock
#include "../runtime.cpp"
#undef clock_gettime
namespace {
struct Call{uint16_t service,opcode;};std::vector<Call> calls;std::vector<unsigned> operations;
constexpr uint64_t permission_handle=0x100000001,audio_handle=0x200000001,asset_handle=0x300000001;
int reply(uint16_t service,uint16_t op,uint64_t token,const uint8_t *p,size_t size){uint8_t msg[256];uint32_t length;assert(pxa_build_message(msg,sizeof(msg),service,op,token,p,size,&length));return handle_event(msg,length);}
unsigned plays(){unsigned count=0;for(auto op:operations)count+=op==PXA_AUDIO_IO_PLAY_SOUND;return count;}
void ready(){
    running=true;background=paused=exit_requested=false;epoch=test_us;
    uint8_t permission[12]{};pxa_store_u64(permission+4,permission_handle);
    assert(reply(PXA_PERMISSION_SERVICE,PXA_PERMISSION_ACQUIRE,SOUND_PERMISSION,permission,12)==PXA_EVENT_HANDLED);
    assert(calls.back().opcode==PXA_AUDIO_OPEN_SESSION);
    uint8_t data[64]{};size_t offset=4,used;
    auto record=[&](uint16_t tag,uint64_t value,size_t n){uint8_t v[8];pxa_store_u64(v,value);assert(pxa_wire_record_encode(data+offset,sizeof(data)-offset,tag,v,n,&used));offset+=used;};
    record(3,audio_handle,8);record(4,16000,4);record(5,1,1);record(6,20,2);
    assert(reply(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_SESSION,SOUND_LEGACY,data,offset)==PXA_EVENT_HANDLED);
    uint8_t status[4]{};assert(reply(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,SOUND_GRAPH,status,4)==PXA_EVENT_HANDLED);assert(sound_handle==audio_handle);
}
void asset(){uint8_t payload[32]{};pxa_store_u64(payload+4,asset_handle);payload[12]=PXA_ASSET_AUDIO;payload[13]=PXA_ASSET_ENCODING_PCM_S16LE_16K_MONO;pxa_store_u16(payload+14,1);pxa_store_u32(payload+20,32000);pxa_store_u32(payload+24,32000);pxa_store_u32(payload+28,32000);uint8_t query[24]{};memcpy(query+4,payload+12,20);assert(reply(PXA_ASSETS_SERVICE,PXA_ASSETS_QUERY,RECORDING_QUERY,query,24)==PXA_EVENT_HANDLED);service_sound();assert(recording_loading);assert(reply(PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,RECORDING_LOAD,payload,32)==PXA_EVENT_HANDLED);}
}
extern "C" int32_t pxa_call(uint32_t route,uint64_t,const uint8_t *,uint32_t){calls.push_back({uint16_t(route),uint16_t(route>>16)});return 0;}
extern "C" int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *,uint32_t size){operations.push_back(op);return handle==audio_handle?int32_t(size):PXA_STATUS_WOULD_BLOCK;}
namespace game {
const char *title(){return "test";}const char *app_id(){return "pxa-milky-test";}void init(){}void reset(){}void key(pocket::Key){}void pointer(pocket::Phase,int,int){}void tick(uint32_t){}void draw(){}size_t serialize(uint8_t *,size_t){return 0;}void load(const uint8_t *,size_t){}
}
int main(){
    ready();const char *paths[]={"assets/laugh.s16"};milky::sounds(paths,1);service_sound();assert(recording_querying);asset();
    milky::play(0,0,true);service_sound();assert(plays()==1&&recording_duration[0]==16000);
    test_us+=250000;milky::finish(0);assert(recording_stop_at[0]==16000);test_us+=800000;service_sound();assert(!recording_stop_at[0]);
    milky::play(0);toggle_pause();unsigned before=plays();service_sound();assert(plays()==before&&!requested_sounds[0].pending);
    toggle_pause();milky::volume(0,0);milky::play(0);service_sound();assert(plays()==before);
    milky::volume(100,0);milky::play(0);service_sound();assert(plays()==before+1);
    milky::play(0);uint8_t life=PXA_SYSTEM_LIFECYCLE_BACKGROUND;assert(reply(PXA_SERVICE_SYSTEM,PXA_SYSTEM_LIFECYCLE_EVENT,0,&life,1)==PXA_EVENT_HANDLED);assert(!requested_sounds[0].pending);before=plays();service_sound();assert(plays()==before);
    canvas=logical_frame;system_text_begin();pocket::text(33,6,"2048",milky::BROWN,3);pocket::rect(4,33,186,186,milky::SOFT);
    assert(system_text_count==1&&system_texts[0].visible);milky::center(20,80,120,"test",milky::BROWN);assert(system_texts[1].align==1);
    assert(system_text_present()&&system_text_bytes[0]==12);pocket::rect(10,70,150,40,milky::PAPER);assert(!system_texts[1].visible);canvas=nullptr;
    // A completed image can arrive after navigation; it must be retired.
    milky::NativeImage source{"assets/test.png",64,64,0,0,64,64};
    milky::Sprite sprite{1,1,nullptr,source,source,source,source};system_text_begin();assert(!native_image_add(sprite,10,30,50,50));assert(system_text_present()&&image_load_token);
    const uint64_t pending=image_load_token;system_text_begin();assert(system_text_present());
    uint8_t image[32]{};pxa_store_u64(image+4,0x400000001);image[12]=PXA_ASSET_IMAGE;image[13]=PXA_ASSET_ENCODING_BGRA8888_PREMULTIPLIED;
    pxa_store_u16(image+16,64);pxa_store_u16(image+18,64);pxa_store_u32(image+24,64*64*4);
    assert(reply(PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,pending,image,32)==PXA_EVENT_HANDLED);assert(system_text_present());
    for(const auto &r:image_resources)assert(!r.path&&!r.handle);
    puts("PASS: media session, S16 recording tracks, loop tail, mute, pause and background cancellation.");
}
