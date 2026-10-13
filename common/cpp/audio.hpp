#pragma once
#include <pxa/app.hpp>
#include <algorithm>
#include <array>
#include <bitset>
#include <cstdio>
namespace arcade {
inline bool transient_audio_error(pxa::Error error) {
    return error==pxa::Error::quota_exceeded || error==pxa::Error::resource_limit ||
        error==pxa::Error::busy || error==pxa::Error::would_block ||
        error==pxa::Error::timed_out || error==pxa::Error::unavailable || error==pxa::Error::io_error;
}
// The Host owns decoding, playback clocks, voices and streaming music. This
// optional bank holds eight prepared assets and at most one load coroutine.
struct AudioBank {
    pxa::Context* context=nullptr;
    std::optional<pxa::Permission> permission;
    std::optional<pxa::AudioSession> session;
    std::array<std::optional<pxa::Asset>,8> sounds;
    std::array<uint8_t,8> ids{};
    std::array<uint32_t,8> ages{};
    uint32_t age=0;
    std::bitset<256> failed{};
    bool loading=false,unavailable=false,discard_loading=false;
    uint8_t loading_id=255,retries=0,retry_delay=0;
    [[gnu::noinline]] pxa::Task<void> open(int16_t gain,pxa::EqBand band) {
        std::array<std::byte,64> packet{};constexpr std::string_view scope="media";
        auto grant=co_await context->permissions().acquire("audio.playback",std::as_bytes(std::span{scope.data(),scope.size()}),packet);
        if(!grant){unavailable=true;co_return pxa::Result<void>{};}
        permission.emplace(std::move(*grant));auto opened=co_await context->audio().open(*permission);
        if(!opened){unavailable=true;permission.reset();co_return pxa::Result<void>{};}
        auto configured=co_await opened->graph(gain,{&band,1});
        if(!configured){unavailable=true;opened=std::unexpected(pxa::Error::internal);permission.reset();co_return pxa::Result<void>{};}
        session.emplace(std::move(*opened));
        (void)context->log().write(pxa::LogLevel::info,"C++ game audio ready");
        co_return pxa::Result<void>{};
    }
    void start(pxa::Context& ctx,int16_t gain,pxa::EqBand band) {
        context=&ctx;auto result=ctx.tasks().start(open(gain,band));if(!result)unavailable=true;
    }
    [[gnu::noinline]] pxa::Task<void> load(uint8_t id,unsigned slot,const char* path) {
        auto sound=co_await context->assets().load(pxa::AssetKind::audio,path);
        // release() may invalidate this request while the Host is loading it.
        // Let the returned handle close here instead of resurrecting a pin.
        if(discard_loading){discard_loading=false;retries=retry_delay=0;}
        else if(sound){sounds[slot].emplace(std::move(*sound));ids[slot]=id;ages[slot]=++age;retries=retry_delay=0;}
        else {
            const auto error=sound.error();
            const bool transient=transient_audio_error(error);
            // Stopped Host voices may retain an asset until the mixer drains
            // their final block. Back off shared loading work; temporary
            // pressure must never permanently blacklist a valid sound.
            if(transient){
                if(retries<8)++retries;
                retry_delay=uint8_t(4u<<std::min<unsigned>(retries-1,5));
            }else {failed[id]=true;retries=retry_delay=0;}
            if(!transient||retries==1){char message[160];
                std::snprintf(message,sizeof(message),"C++ game sound %s id=%u error=%d path=%s",
                    transient?"deferred":"preparation failed",id,int(error),path);
                (void)context->log().write(pxa::LogLevel::warning,message);}
        }
        loading=false;co_return pxa::Result<void>{};
    }
    // Drop a prepared handle and reject a pending completion for the same id.
    // Host playback retains its own pin; this does not stop an active voice.
    void release(uint8_t id) {
        for(unsigned i=0;i<sounds.size();++i)if(sounds[i]&&ids[i]==id)sounds[i].reset();
        if(loading&&loading_id==id)discard_loading=true;
    }
    const pxa::Asset* prepare(uint8_t id,const char* path) {
        if(!session||unavailable||failed[id])return nullptr;
        for(unsigned i=0;i<sounds.size();++i)if(sounds[i]&&ids[i]==id){ages[i]=++age;return &*sounds[i];}
        if(loading)return nullptr;
        if(retry_delay){--retry_delay;return nullptr;}
        unsigned slot=0;
        for(unsigned i=0;i<sounds.size();++i){if(!sounds[i]){slot=i;break;}if(ages[i]<ages[slot])slot=i;}
        sounds[slot].reset();loading=true;discard_loading=false;loading_id=id;
        auto result=context->tasks().start(load(id,slot,path));if(!result)loading=false;
        return nullptr;
    }
    void pause(){if(session)(void)session->control(pxa::MusicAction::pause);}
    void resume(){if(session)(void)session->control(pxa::MusicAction::resume);}
};
} // namespace arcade
