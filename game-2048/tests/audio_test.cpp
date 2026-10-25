// SPDX-License-Identifier: CC0-1.0
// Native SDK wire/PCM test: no emulator, device or package build is required.
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <vector>
#include <algorithm>
static uint64_t test_us = 1000000;
static int test_clock(clockid_t, timespec *t) {
    t->tv_sec = time_t(test_us / 1000000); t->tv_nsec = long(test_us % 1000000) * 1000; return 0;
}
#define clock_gettime test_clock
#include "../../common/pocket-game/runtime.cpp"
#undef clock_gettime
#include "../../common/pocket-game/music.hpp"

namespace {
struct Request { uint16_t service, opcode; uint64_t token; };
std::vector<Request> requests;
std::vector<int16_t> captured;
std::vector<uint8_t> blocked_pcm;
int io_status = 0, blocks = 0, actions = 0;
unsigned flushes = 0, audio_writes = 0;
constexpr uint64_t PERMISSION = UINT64_C(0x100000001), AUDIO = UINT64_C(0x200000001);
int reply(uint16_t service, uint16_t opcode, uint64_t token, const uint8_t *payload, size_t size) {
    uint8_t message[256]; uint32_t n;
    assert(pxa_build_message(message,sizeof(message),service,opcode,token,payload,size,&n));
    return handle_event(message,n);
}
void status(uint16_t service, uint16_t opcode, uint64_t token, int32_t value) {
    uint8_t payload[4]; pxa_store_u32(payload,uint32_t(value));
    assert(reply(service,opcode,token,payload,4)==PXA_EVENT_HANDLED);
}
void permission(int32_t result = 0) {
    uint8_t payload[12]{}; pxa_store_u32(payload,uint32_t(result)); pxa_store_u64(payload+4,PERMISSION);
    assert(reply(PXA_PERMISSION_SERVICE,PXA_PERMISSION_ACQUIRE,SOUND_PERMISSION,payload,result?4:12)==PXA_EVENT_HANDLED);
}
void open_reply(bool legacy = false) {
    uint8_t payload[56]{}; size_t offset=4, used;
    const auto record=[&](uint16_t tag,uint64_t value,size_t size) {
        uint8_t bytes[8];pxa_store_u64(bytes,value);
        assert(pxa_wire_record_encode(payload+offset,sizeof(payload)-offset,tag,bytes,size,&used)); offset+=used;
    };
    record(3,AUDIO,8);record(4,16000,4);record(5,1,1);record(6,20,2);
    if (!legacy) {record(7,48000,4);record(8,2,1);record(9,960,4);}
    assert(offset==(legacy?35:56));
    assert(reply(PXA_AUDIO_SERVICE,legacy?PXA_AUDIO_OPEN_SESSION:PXA_AUDIO_OPEN_PCM,
        legacy?SOUND_LEGACY:SOUND_OPEN,payload,offset)==PXA_EVENT_HANDLED);
}
void reset_audio() {
    running=true;finished=paused=background=exit_requested=false;
    epoch=last_tick=test_us;paused_us=pause_at=0;
    sound_handle=sound_pending_handle=sound_permission=sound_cursor=0;
    sound_pending=false;memset(tones,0,sizeof(tones));
    requests.clear();captured.clear();blocked_pcm.clear();io_status=blocks=0;
    save_dirty=save_pending=preferences_dirty=preferences_pending=false;
}
void ready(bool legacy = false) {
    reset_audio();request_sound();assert(requests.back().service==PXA_PERMISSION_SERVICE);
    assert(!sound_handle);permission();assert(sound_permission==PERMISSION && !sound_handle);
    assert(requests.back().opcode==PXA_AUDIO_OPEN_PCM);
    if (legacy) {
        status(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_PCM,SOUND_OPEN,PXA_STATUS_UNSUPPORTED);
        assert(requests.back().opcode==PXA_AUDIO_OPEN_SESSION);
    }
    open_reply(legacy);assert(sound_pending_handle==AUDIO && !sound_handle);
    assert(requests.back().opcode==PXA_AUDIO_COMMIT_GRAPH);
    status(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,SOUND_GRAPH,0);assert(sound_handle==AUDIO);
}
void check_samples() {
    assert(std::any_of(captured.begin(),captured.end(),[](int16_t s){return s>0;}));
    assert(std::any_of(captured.begin(),captured.end(),[](int16_t s){return s<0;}));
    for (int16_t s:captured) assert(s>=-20000 && s<=20000);
}
void denied_and_failures() {
    reset_audio();request_sound();permission(PXA_STATUS_DENIED);
    assert(!sound_handle && !sound_permission);pocket::tone(440,.1f);service_sound();
    assert(captured.empty());dispatch_key(pocket::Key::A);assert(actions==1);
    ready();io_status=PXA_STATUS_INTERNAL;pocket::tone(440,.1f);service_sound();assert(!sound_handle);
    dispatch_key(pocket::Key::A);assert(actions==2);
    reset_audio();permission();open_reply();
    status(PXA_AUDIO_SERVICE,PXA_AUDIO_COMMIT_GRAPH,SOUND_GRAPH,PXA_STATUS_INTERNAL);
    assert(!sound_handle);service_sound();assert(captured.empty());
    reset_audio();permission();status(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_PCM,SOUND_OPEN,PXA_STATUS_UNSUPPORTED);
    status(PXA_AUDIO_SERVICE,PXA_AUDIO_OPEN_SESSION,SOUND_LEGACY,PXA_STATUS_UNSUPPORTED);
    assert(!sound_handle);dispatch_key(pocket::Key::A);assert(actions==3);
}
void backpressure_and_lifecycle() {
    ready();blocks=1;pocket::tone(440,.3f);service_sound();
    assert(sound_pending && sound_cursor==0 && captured.empty() && blocked_pcm.size()==640);
    service_sound();assert(!sound_pending && captured.size()>=320);
    assert(!memcmp(captured.data(),blocked_pcm.data(),640));check_samples();
    const unsigned before=audio_writes, old_flush=flushes;
    toggle_pause();assert(paused && flushes==old_flush+1);
    for (const auto &t:tones) assert(!t.end);
    pocket::tone(440,.1f);test_us+=100000;service_sound();assert(audio_writes==before);
    toggle_pause();pocket::tone(660,.1f);service_sound();assert(audio_writes>before);
    uint8_t life=PXA_SYSTEM_LIFECYCLE_BACKGROUND;
    assert(reply(PXA_SERVICE_SYSTEM,PXA_SYSTEM_LIFECYCLE_EVENT,0,&life,1)==PXA_EVENT_HANDLED);
    const unsigned bg=audio_writes;assert(background);test_us+=1000000;
    pocket::tone(440,.1f);service_sound();assert(audio_writes==bg);
    life=PXA_SYSTEM_LIFECYCLE_FOREGROUND;
    assert(reply(PXA_SERVICE_SYSTEM,PXA_SYSTEM_LIFECYCLE_EVENT,0,&life,1)==PXA_EVENT_HANDLED);
    pocket::tone(440,.1f);service_sound();assert(audio_writes>bg);
    request_exit();const unsigned exiting=audio_writes;
    pocket::tone(440,.1f);test_us+=100000;service_sound();assert(audio_writes==exiting && exit_requested);
    ready();uint8_t payload[32]{};size_t n=0, used=0;
    assert(pxa_wire_record_encode(payload,sizeof(payload),1,reinterpret_cast<const uint8_t *>("audio.playback"),14,&used));n+=used;
    assert(pxa_wire_record_encode(payload+n,sizeof(payload)-n,2,reinterpret_cast<const uint8_t *>("media"),5,&used));n+=used;
    assert(reply(PXA_PERMISSION_SERVICE,PXA_PERMISSION_REVOKED,0,payload,n)==PXA_EVENT_HANDLED);
    assert(!sound_handle && !sound_permission);service_sound();assert(captured.empty());
}
void idle_and_cues() {
    using namespace pocket_game_sfx;
    for(unsigned legacy=0;legacy<2;++legacy) {
        ready(legacy);
        for(unsigned i=0;i<500;++i){test_us+=20000;service_sound();}
        assert(captured.size()>=160000);
        assert(std::all_of(captured.begin(),captured.end(),[](int16_t s){return s==0;}));
        for(unsigned event=0;event<=unsigned(Cue::Push);++event) {
            pocket::silence();captured.clear();cue(Cue(event));
            for(unsigned i=0;i<25;++i){test_us+=20000;service_sound();}
            check_samples();
            captured.clear();
            for(unsigned i=0;i<100;++i){test_us+=20000;service_sound();}
            assert(!captured.empty());
            assert(std::all_of(captured.begin(),captured.end(),[](int16_t s){return s==0;}));
        }
    }
}
}
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *,uint32_t) {
    requests.push_back({uint16_t(route),uint16_t(route>>16),token});
    if(uint16_t(route)==PXA_AUDIO_SERVICE && uint16_t(route>>16)==PXA_AUDIO_FLUSH)++flushes;
    return 0;
}
extern "C" int32_t pxa_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t length) {
    if(handle==AUDIO && operation==PXA_AUDIO_IO_WRITE) {
        assert(length==640);++audio_writes;
        if(blocks>0){--blocks;blocked_pcm.assign(data,data+length);return PXA_STATUS_WOULD_BLOCK;}
        if(io_status)return io_status;
        const int16_t *samples=reinterpret_cast<int16_t *>(data);
        captured.insert(captured.end(),samples,samples+length/2);return int32_t(length);
    }
    return PXA_STATUS_WOULD_BLOCK;
}
#ifndef POCKET_AUDIO_GAME
namespace game {
const char *title(){return "Audio test";}const char *app_id(){return "audio-test";}
void init(){}void reset(){}void key(pocket::Key){++actions;}void tick(uint32_t){}
void pointer(pocket::Phase,int,int){}void draw(){}
size_t serialize(uint8_t*,size_t){return 0;}void load(const uint8_t*,size_t){}
}
#endif
int main() {
    denied_and_failures();backpressure_and_lifecycle();idle_and_cues();
    puts("PASS actual Pocket Game runtime: async permission/PCM/graph, legacy, denied/open/write/graph failure, exact WOULD_BLOCK retry, pause/background/exit/revoke silence, resume, 10s idle zero PCM and all 10 cues followed by zero PCM in both audio modes");
    return 0;
}
