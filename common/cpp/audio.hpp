#pragma once
#include <pxa/app.hpp>
#include <array>
#include <bitset>
namespace arcade {
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
    bool loading=false,unavailable=false;
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
        if(sound){sounds[slot].emplace(std::move(*sound));ids[slot]=id;ages[slot]=++age;}
        else {failed[id]=true;(void)context->log().write(pxa::LogLevel::warning,"C++ game sound preparation failed");}
        loading=false;co_return pxa::Result<void>{};
    }
    const pxa::Asset* prepare(uint8_t id,const char* path) {
        if(!session||unavailable||failed[id])return nullptr;
        for(unsigned i=0;i<sounds.size();++i)if(sounds[i]&&ids[i]==id){ages[i]=++age;return &*sounds[i];}
        if(loading)return nullptr;
        unsigned slot=0;
        for(unsigned i=0;i<sounds.size();++i){if(!sounds[i]){slot=i;break;}if(ages[i]<ages[slot])slot=i;}
        sounds[slot].reset();loading=true;
        auto result=context->tasks().start(load(id,slot,path));if(!result)loading=false;
        return nullptr;
    }
    void pause(){if(session)(void)session->control(pxa::MusicAction::pause);}
    void resume(){if(session)(void)session->control(pxa::MusicAction::resume);}
};
} // namespace arcade
