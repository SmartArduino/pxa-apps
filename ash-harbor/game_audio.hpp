// SPDX-License-Identifier: MIT
#pragma once
#include "battle_feedback.hpp"
#include "audio_data.hpp"
#include "voice_data.hpp"
#include "../common/motion-games/sound.hpp"

namespace ash {
struct GameAudio:motion::Sounds {
    using Sfx=audio::Sfx;using Voice=audio::Voice;
    static constexpr unsigned clipCount=unsigned(Sfx::count),voiceCount=unsigned(Voice::count);
    std::array<std::optional<pxa::Asset>,clipCount> clips;
    std::optional<pxa::Asset> hot;
    std::array<std::optional<pxa::Asset>,2> voices;
    std::array<int,2> voiceId{-1,-1},voicePending{-1,-1};
    std::array<unsigned,2> voiceSequence{},voicePriority{};
    std::array<uint64_t,2> voiceExpires{},voiceBusy{};
    std::array<bool,2> voiceLoading{};
    std::array<uint64_t,4> nextEffect{};
    uint64_t milliseconds=0,hotExpires=0,musicInstance=0,musicRetry=0;
    unsigned hotSequence=0,playedEffects=0,playedVoices=0;
    int hotId=-1,hotPending=-1,musicTrack=-1;
    int hotInFlight=-1;std::array<int,2> voiceInFlight{-1,-1};
    uint64_t hotBusy=0;
    bool hotLoading=false,musicReady=false,musicPaused=false,musicEnded=false;
    bool reopening=false;
    float musicGain=-40;
    void* assetOwner=nullptr;bool (*reserveAssets)(void*,unsigned)=nullptr;
    struct Pose {Action action=Action::idle;int age=0,walk=0;bool grounded=true;};
    std::array<Pose,2> previous;
    int previousDamage=0,previousBlocks=0,previousRound=0;
    uint32_t previousEscapes=0;
    Phase previousPhase=Phase::intro;

    unsigned resident_bytes()const{
        unsigned bytes=0;
        for(unsigned i=0;i<clipCount;++i)if(clips[i])bytes+=audio::sfx_bytes[i];
        if(hot&&hotId>=0)bytes+=audio::sfx_bytes[hotId];
        if(hotInFlight>=0)bytes+=audio::sfx_bytes[hotInFlight];
        for(unsigned i=0;i<2;++i)if(voices[i]&&voiceId[i]>=0)
            bytes+=audio::voice_bytes[voiceId[i]/voiceCount][voiceId[i]%voiceCount];
        for(int id:voiceInFlight)if(id>=0)bytes+=audio::voice_bytes[id/voiceCount][id%voiceCount];
        return bytes;
    }
    bool room(unsigned bytes){
        if(!reserveAssets||reserveAssets(assetOwner,bytes))return true;
        if(hot&&milliseconds>=hotBusy){hot.reset();hotId=-1;}
        for(unsigned i=0;i<2;++i)if(!voiceLoading[i]&&milliseconds>=voiceBusy[i]){voices[i].reset();voiceId[i]=-1;}
        return reserveAssets(assetOwner,bytes);
    }
    pxa::Task<void> prepare(){
        if(session){
            for(Sfx id:{Sfx::ui_select,Sfx::hit_light,Sfx::hit_heavy,Sfx::block,Sfx::whoosh_punch,Sfx::whoosh_kick}){
                char path[80];unsigned n=unsigned(id);std::snprintf(path,sizeof path,"assets/audio/%s",audio::sfx_files[n]);
                if(!room(audio::sfx_bytes[n]))continue;
                auto loaded=co_await context->assets().load(pxa::AssetKind::audio,path);
                if(loaded)clips[n].emplace(std::move(*loaded));else report("Ash Harbor core audio unavailable");
            }
            report("Ash Harbor generated audio ready");
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> open(pxa::Context& ctx){
        auto result=co_await motion::Sounds::open(ctx);if(session)co_await prepare();co_return result;
    }
    pxa::Task<void> reopen(){
        auto grant=co_await context->permissions().acquire("audio.playback","media");
        if(grant){permission.emplace(std::move(*grant));auto opened=co_await context->audio().open(*permission);
            if(opened){auto graph=co_await opened->graph(0);if(graph){session.emplace(std::move(*opened));denied=false;co_await prepare();}}}
        reopening=false;co_return pxa::Result<void>{};
    }
    void toggle(){
        enabled=!enabled;if(!enabled)stop();
        else if(!session&&!reopening){reopening=true;if(!context->tasks().start(reopen()))reopening=false;}
        else effect(Sfx::ui_select);
        save_pending=true;if(!saving){saving=true;if(!context->tasks().start(persist()))saving=false;}
        report(enabled?"Ash Harbor audio enabled=1":"Ash Harbor audio enabled=0");
    }
    static unsigned track(Sfx id){
        if(id==Sfx::ui_select)return 0;
        if(id==Sfx::hit_light||id==Sfx::hit_heavy||id==Sfx::block||id==Sfx::throw_hit||id==Sfx::super_hit||id==Sfx::ko)return 1;
        if(unsigned(id)>=unsigned(Sfx::whoosh_punch)&&unsigned(id)<=unsigned(Sfx::evade))return 2;
        return 3;
    }
    void submit_clip(Sfx id,const pxa::Asset& asset){
        if(!audible())return;unsigned category=track(id);
        if(milliseconds<nextEffect[category])return;
        nextEffect[category]=milliseconds+(category==0?60:category==1?35:25);
        int gain=id==Sfx::ui_select?-12:id==Sfx::step_0||id==Sfx::step_1?-10:category==3?-7:-5;
        if(session->sound_track(asset,pxa::SoundTrack(category),false,gain*256)){
            if(hot && &asset==&*hot)hotBusy=milliseconds+audio::sfx_ms[unsigned(id)];
            ++playedEffects;if(playedEffects<5){char b[100];std::snprintf(b,sizeof b,"Ash Harbor effect=%s",audio::sfx_files[unsigned(id)]);report(b);}
        }
    }
    pxa::Task<void> load_hot(int id,unsigned sequence,uint64_t expires){
        hot.reset();hotId=-1;
        if(room(audio::sfx_bytes[id])){
            hotInFlight=id;
            char path[80];std::snprintf(path,sizeof path,"assets/audio/%s",audio::sfx_files[id]);
            auto loaded=co_await context->assets().load(pxa::AssetKind::audio,path);
            if(loaded){hot.emplace(std::move(*loaded));hotId=id;}
            hotInFlight=-1;
        }
        if(hot&&sequence==hotSequence&&milliseconds<=expires)submit_clip(Sfx(id),*hot);
        hotLoading=false;co_return pxa::Result<void>{};
    }
    void effect(Sfx id){
        if(!audible())return;unsigned n=unsigned(id);
        if(clips[n]){submit_clip(id,*clips[n]);return;}
        if(hot&&hotId==int(n)){submit_clip(id,*hot);return;}
        hotPending=n;hotExpires=milliseconds+250;++hotSequence;pump();
    }
    void play(motion::Cue cue,unsigned=0){effect(cue==motion::Cue::click?Sfx::ui_select:cue==motion::Cue::impact?Sfx::hit_light:Sfx::round);}
    void play_voice(unsigned actor,int id){
        if(!audible()||!voices[actor])return;
        if(session->sound_track(*voices[actor],pxa::SoundTrack(4+actor),false,-2*256)){
            voiceBusy[actor]=milliseconds+audio::voice_ms[id/voiceCount][id%voiceCount];
            ++playedVoices;char b[112];std::snprintf(b,sizeof b,"Ash Harbor voice=%s",audio::voice_files[id/voiceCount][id%voiceCount]);report(b);
        }
    }
    pxa::Task<void> load_voice(unsigned actor,int id,unsigned sequence,uint64_t expires){
        if(session)(void)session->control_sound(pxa::SoundTrack(4+actor),pxa::MusicAction::stop);
        voices[actor].reset();voiceId[actor]=-1;
        if(room(audio::voice_bytes[id/voiceCount][id%voiceCount])){
            voiceInFlight[actor]=id;
            char path[112];std::snprintf(path,sizeof path,"assets/audio/voices/%s",audio::voice_files[id/voiceCount][id%voiceCount]);
            auto loaded=co_await context->assets().load(pxa::AssetKind::audio,path);
            if(loaded){voices[actor].emplace(std::move(*loaded));voiceId[actor]=id;}
            voiceInFlight[actor]=-1;
        }
        if(voices[actor]&&sequence==voiceSequence[actor]&&milliseconds<=expires)play_voice(actor,id);
        voiceLoading[actor]=false;co_return pxa::Result<void>{};
    }
    void speak(unsigned actor,unsigned character,Voice cue,unsigned priority=1){
        if(character>=std::size(audio::voice_files))return;
        if(!audible()||(milliseconds<voiceBusy[actor]&&priority<voicePriority[actor]))return;
        int id=character*voiceCount+unsigned(cue);voicePriority[actor]=priority;
        ++voiceSequence[actor];
        if(voices[actor]&&voiceId[actor]==id&&!voiceLoading[actor]){play_voice(actor,id);return;}
        voicePending[actor]=id;voiceExpires[actor]=milliseconds+(priority>=2?1200:350);pump();
    }
    void pump(){
        if(!audible())return;
        if(!hotLoading&&hotPending>=0){int id=hotPending;hotPending=-1;
            if(milliseconds<=hotExpires){hotLoading=true;if(!context->tasks().start(load_hot(id,hotSequence,hotExpires)))hotLoading=false;}}
        for(unsigned i=0;i<2;++i)if(!voiceLoading[i]&&voicePending[i]>=0){int id=voicePending[i];voicePending[i]=-1;
            if(milliseconds<=voiceExpires[i]){voiceLoading[i]=true;if(!context->tasks().start(load_voice(i,id,voiceSequence[i],voiceExpires[i])))voiceLoading[i]=false;}}
    }
    void silence(){
        hotPending=-1;++hotSequence;
        for(unsigned i=0;i<2;++i){voicePending[i]=-1;++voiceSequence[i];voiceBusy[i]=0;}
        if(session)for(unsigned i=0;i<6;++i)(void)session->control_sound(pxa::SoundTrack(i),pxa::MusicAction::stop);
        for(unsigned i=0;i<2;++i){voices[i].reset();voiceId[i]=-1;}
    }
    void stop(){silence();motion::Sounds::stop();if(session)(void)session->control_music(pxa::MusicAction::stop);musicInstance=0;musicTrack=-1;musicReady=musicPaused=musicEnded=false;}
    void suspend(){silence();motion::Sounds::suspend();if(session&&musicInstance){(void)session->control_music(pxa::MusicAction::pause);musicPaused=true;}}
    void resume(){motion::Sounds::resume();}
    void pause(){silence();if(session&&musicInstance){(void)session->control_music(pxa::MusicAction::pause);musicPaused=true;}}
    void begin(const Arena& arena,unsigned local=0){
        silence();previousDamage=arena.damageEvents;previousBlocks=arena.blocks;previousEscapes=arena.escapes;previousRound=arena.round;previousPhase=arena.phase;
        for(unsigned i=0;i<2;++i)previous[i]={arena.f[i].action,arena.f[i].age,arena.f[i].walkFrame(),arena.f[i].grounded()};
        effect(Sfx::round);speak(local,arena.f[local].character,Voice::intro,2);
    }
    void step(const Arena& arena,unsigned local=0){
        if(!audible())return;
        for(unsigned i=0;i<2;++i){const auto& p=arena.f[i];auto old=previous[i];
            bool changed=p.action!=old.action||(p.age<old.age&&attack(p.action));
            if(changed){
                switch(p.action){
                case Action::punch:effect(Sfx::whoosh_punch);speak(i,p.character,Voice(unsigned(Voice::attack_0)+arena.tick%3));break;
                case Action::kick:case Action::low:case Action::air:effect(Sfx::whoosh_kick);speak(i,p.character,Voice(unsigned(Voice::attack_0)+arena.tick%3));break;
                case Action::fire:speak(i,p.character,Voice::fire,2);break;
                case Action::rise:speak(i,p.character,Voice::rise,2);break;
                case Action::rush:speak(i,p.character,Voice::rush,2);break;
                case Action::super:effect(Sfx::charge);speak(i,p.character,Voice::super,3);break;
                case Action::throwing:speak(i,p.character,Voice::throw_move,2);break;
                case Action::dodge:effect(Sfx::evade);speak(i,p.character,Voice::dodge);break;
                case Action::jump:effect(Sfx::jump);break;
                default:break;
                }
            }
            if(p.action==Action::fire||p.action==Action::rise||p.action==Action::rush||p.action==Action::super){
                int start=moveFor(p).startup;
                if(p.age>=start&&(changed||old.age<start))effect(p.action==Action::fire?(roster[p.character].effect?Sfx::wave:Sfx::fire):p.action==Action::rise?Sfx::rise:p.action==Action::rush?Sfx::rush:roster[p.character].effect?Sfx::super_wave:Sfx::super_fire);
            }
            if(!old.grounded&&p.grounded()&&p.action!=Action::down)effect(Sfx::land);
            if(p.action==Action::walk&&old.walk!=p.walkFrame()&&p.walkFrame()%4==0)effect(p.walkFrame()?Sfx::step_1:Sfx::step_0);
            previous[i]={p.action,p.age,p.walkFrame(),p.grounded()};
        }
        if(arena.blocks!=previousBlocks)effect(Sfx::block);
        if(arena.escapes!=previousEscapes)effect(Sfx::block);
        if(arena.damageEvents!=previousDamage){
            auto action=arena.hitAction;
            effect(action==Action::super?Sfx::super_hit:action==Action::throwing?Sfx::throw_hit:action==Action::punch?Sfx::hit_light:Sfx::hit_heavy);
            unsigned victim=1-arena.hitOwner;
            if(arena.f[victim].hp)speak(victim,arena.f[victim].character,action==Action::punch?Voice::hurt_0:Voice::hurt_1,3);
        }
        if(arena.phase!=previousPhase){
            if(arena.phase==Phase::fight)effect(Sfx::round);
            if(arena.phase==Phase::round_end){effect(Sfx::ko);unsigned loser=arena.winner==0?1:0;speak(loser,arena.f[loser].character,Voice::ko,4);}
            if(arena.phase==Phase::match_end)speak(local,arena.f[local].character,localMatchWon(arena,local)?Voice::win:Voice::lose,4);
        }
        previousDamage=arena.damageEvents;previousBlocks=arena.blocks;previousEscapes=arena.escapes;previousPhase=arena.phase;previousRound=arena.round;
    }
    void event(const pxa::Event& event){
        motion::Sounds::event(event);
        if(!session){musicInstance=0;musicTrack=-1;for(auto& clip:clips)clip.reset();hot.reset();hotId=-1;for(auto& voice:voices)voice.reset();voiceId={-1,-1};return;}
        auto played=pxa::decode_playback(event);
        if(!played||played->session!=session->handle()||played->instance!=musicInstance)return;
        if(played->state==pxa::PlaybackState::ready){musicReady=true;char text[70];std::snprintf(text,sizeof text,"Ash Harbor music ready track=%d",musicTrack);report(text);}
        else if(played->state==pxa::PlaybackState::ended){musicInstance=0;musicReady=false;musicEnded=musicTrack==2||musicTrack==3;}
        else if(played->state==pxa::PlaybackState::error){musicInstance=0;musicReady=false;musicRetry=milliseconds+5000;report("Ash Harbor music decode failed");}
    }
    void tick(uint64_t now,float dt,int target,bool paused){
        milliseconds=now/1000;if(!audible())return;
        for(unsigned i=0;i<2;++i)if(!voiceLoading[i]&&voices[i]&&milliseconds>voiceBusy[i]+150){voices[i].reset();voiceId[i]=-1;}
        if(!hotLoading&&hot&&milliseconds>hotBusy+1200){hot.reset();hotId=-1;}
        pump();
        if(paused){if(!musicPaused)pause();return;}
        if(musicPaused){if(musicInstance)(void)session->control_music(pxa::MusicAction::resume);musicPaused=false;}
        if(musicTrack!=target)musicEnded=false;
        if(musicInstance&&musicTrack!=target){musicGain=std::max(-44.f,musicGain-dt*90);(void)session->control_music(pxa::MusicAction::gain,int16_t(musicGain*256));
            if(musicGain<=-44){(void)session->control_music(pxa::MusicAction::stop);musicInstance=0;musicReady=false;}}
        if(!musicInstance&&!musicEnded&&milliseconds>=musicRetry){
            const char* files[]={"assets/audio/title.ogg","assets/audio/battle.ogg","assets/audio/victory.ogg","assets/audio/defeat.ogg","assets/audio/battle-bamboo.ogg","assets/audio/battle-snow.ogg","assets/audio/battle-lantern.ogg"};
            auto playing=session->music(files[target],target<2||target>=4,-40*256);
            if(playing){musicInstance=*playing;musicTrack=target;musicGain=-40;musicReady=false;}
            else{musicRetry=milliseconds+5000;report("Ash Harbor music start deferred");}
        }
        if(musicInstance&&musicReady&&musicTrack==target){
            bool talking=milliseconds<std::max(voiceBusy[0],voiceBusy[1]);float goal=talking?-18.f:target!=0&&target!=2&&target!=3?-9.f:-8.f;
            float gain=musicGain<goal?std::min(goal,musicGain+dt*20):std::max(goal,musicGain-dt*90);
            if(gain!=musicGain){(void)session->control_music(pxa::MusicAction::gain,int16_t(gain*256));musicGain=gain;}
        }
    }
};
}
