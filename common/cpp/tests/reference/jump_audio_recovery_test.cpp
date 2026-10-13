#include "pxa_control_capture.h"
#include "../../../../jump-jump-3d/jump3d_audio.hpp"
#include <cassert>
#include <cstring>

using namespace jump;
static constexpr std::uint64_t session_handle=UINT64_C(0x100000012);
static constexpr std::uint64_t long_handle=UINT64_C(0x100000099);
static std::uint64_t pending_token;
static unsigned loads, closed_long;
static unsigned music_calls;
static std::array<unsigned,J3_CHANNEL_BGM> plays{};
static j3_audio_t audio;
static pxa::Context* context;
static pxa::RequestTable session_requests;

extern "C" std::int32_t pxa_test_control(const std::uint8_t* data,std::uint32_t size) {
    const auto* p=reinterpret_cast<const std::byte*>(data);
    const auto service=pxa::wire::get16(p),opcode=pxa::wire::get16(p+2);
    if(service==1){
        if(opcode==2&&size>=28&&pxa::wire::get64(p+20)==long_handle)++closed_long;
        return 0;
    }
    if(service==19){std::printf("%.*s\n",int(size-21),data+21);return 0;}
    assert(service==21&&opcode==2&&!pending_token);
    pending_token=pxa::wire::get64(p+4);++loads;return 0;
}
extern "C" std::int32_t pxa_io(std::uint64_t handle,std::uint32_t operation,
    std::uint8_t* data,std::uint32_t size) {
    assert(handle==session_handle);
    if(operation==0x103){assert(size==16);++plays[data[10]];}
    else if(operation==0x104){++music_calls;pxa::wire::put64(reinterpret_cast<std::byte*>(data),music_calls);}
    else assert(operation==0x105||operation==0x102);
    return size;
}
struct Fixture {
    pxa::Result<void> on_start(pxa::Context& ctx){
        context=&ctx;audio.bank.context=&ctx;
        audio.bank.session.emplace(ctx.transport(),session_requests,
            session_handle,pxa::AudioFormat{16000,1,20});
        audio.preload_index=4;
        return {};
    }
};
PXA_APPLICATION(Fixture)

static void complete(pxa::Error error=pxa::Error{},std::uint64_t handle=UINT64_C(0x100000077)){
    assert(pending_token);
    std::array<std::byte,52> packet{};
    pxa::wire::put16(packet.data(),21);pxa::wire::put16(packet.data()+2,2);
    pxa::wire::put64(packet.data()+4,std::exchange(pending_token,0));
    const unsigned bytes=error==pxa::Error{}?32:4;
    pxa::wire::put32(packet.data()+12,bytes);
    pxa::wire::put32(packet.data()+20,static_cast<std::uint32_t>(error));
    if(bytes==32){
        pxa::wire::put64(packet.data()+24,handle);
        packet[32]=std::byte{3};packet[33]=std::byte{4};
        pxa::wire::put32(packet.data()+40,320);
        pxa::wire::put32(packet.data()+44,320);
        pxa::wire::put32(packet.data()+48,320);
    }
    assert(pxa_app_on_event(reinterpret_cast<const uint8_t*>(packet.data()),20+bytes)==1);
}
static std::uint64_t now=1000000;
static void tick(){context->transport().phase(pxa::Phase::event);now+=20000;j3_audio_tick(&audio,now);context->tasks().reap();}
static void await_load(){
    for(unsigned i=0;i<1024&&!pending_token;++i)tick();
    if(!pending_token)std::fprintf(stderr,"loads=%u failed=%d retries=%u requested=%u started=%u\n",
        loads,int(audio.bank.failed[J3_CLIP_SCALE_INTRO]),audio.bank.retries,
        audio.voices[J3_CHANNEL_CHARGE].requested,audio.voices[J3_CHANNEL_CHARGE].started);
    assert(pending_token);
}

int main(int argc,char** argv){
    assert(argc==2&&pxa_app_start(nullptr,0)==0);
    context->transport().phase(pxa::Phase::event);
    if(std::strcmp(argv[1],"pressure")==0){
        j3_audio_play(&audio,J3_CHANNEL_CHARGE,J3_CLIP_SCALE_INTRO,J3_GAIN_FULL,0);
        for(unsigned i=0;i<8;++i){
            await_load();complete(i%2?pxa::Error::resource_limit:pxa::Error::quota_exceeded);
            assert(!audio.bank.failed[J3_CLIP_SCALE_INTRO]);
        }
        await_load();complete();tick();
        assert(plays[J3_CHANNEL_CHARGE]==1&&audio.voices[J3_CHANNEL_CHARGE].started);
        // Broken paths remain suppressed; a temporary shortage never becomes one.
        assert(!audio.bank.prepare(J3_CLIP_FALL,"missing.s16"));await_load();
        complete(pxa::Error::not_found);assert(audio.bank.failed[J3_CLIP_FALL]);
        const auto previous=loads;
        for(unsigned i=0;i<100;++i)assert(!audio.bank.prepare(J3_CLIP_FALL,"missing.s16"));
        assert(loads==previous);
        std::puts("eight temporary failures recover; missing file is suppressed");
    }else if(std::strcmp(argv[1],"music")==0){
        auto event=[&](uint64_t instance,pxa::PlaybackState state,pxa::Error status=pxa::Error{}){
            context->transport().phase(pxa::Phase::event);
            std::array<std::byte,24> bytes{};
            pxa::wire::put64(bytes.data(),session_handle);pxa::wire::put64(bytes.data()+8,instance);
            bytes[16]=std::byte(static_cast<uint8_t>(state));
            pxa::wire::put32(bytes.data()+20,static_cast<uint32_t>(status));
            j3_audio_event(&audio,{10,0x8001,0,bytes});
        };
        j3_audio_play(&audio,J3_CHANNEL_BGM,J3_CLIP_ICON,J3_GAIN_BGM,0);tick();
        assert(music_calls==1);event(1,pxa::PlaybackState::ready);
        event(1,pxa::PlaybackState::error,pxa::Error::io_error);
        assert(audio.voices[J3_CHANNEL_BGM].requested&&!audio.music_ready);
        for(unsigned i=0;i<9;++i)tick();assert(music_calls==1);
        tick();assert(music_calls==2&&audio.music_instance==2);
        event(1,pxa::PlaybackState::ended);assert(audio.music_instance==2);
        event(2,pxa::PlaybackState::ready);assert(audio.music_ready);
        event(2,pxa::PlaybackState::ended);
        for(unsigned i=0;i<100;++i)tick();assert(music_calls==2);
        assert(!audio.voices[J3_CHANNEL_BGM].requested);
        std::puts("stream I/O failure retries after backoff; stale event ignored; normal one-shot end stays ended");
    }else{
        assert(std::strcmp(argv[1],"late")==0);
        j3_audio_play(&audio,J3_CHANNEL_BONUS,J3_CLIP_SING,J3_GAIN_FULL,0);await_load();
        j3_audio_stop(&audio,J3_CHANNEL_BONUS);
        complete(pxa::Error{},long_handle);tick();
        for(unsigned i=0;i<audio.bank.sounds.size();++i)
            assert(!audio.bank.sounds[i]||audio.bank.ids[i]!=J3_CLIP_SING);
        assert(closed_long==1&&!plays[J3_CHANNEL_BONUS]);
        j3_audio_play(&audio,J3_CHANNEL_CHARGE,J3_CLIP_SCALE_INTRO,J3_GAIN_FULL,0);
        j3_audio_play(&audio,J3_CHANNEL_SUSTAIN,J3_CLIP_SCALE_LOOP,J3_GAIN_SOFT,1);
        await_load();complete();tick();await_load();complete();tick();
        assert(plays[J3_CHANNEL_CHARGE]==1&&plays[J3_CHANNEL_SUSTAIN]==1);
        std::puts("music-box load completed after stop is released; both charge tracks play");
    }
    pxa_app_stop(0);
}
