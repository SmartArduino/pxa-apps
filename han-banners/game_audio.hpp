#pragma once
#include <pxa/audio.hpp>
#include "../common/cpp/audio.hpp"

namespace han {

enum class Score {title, atlas, battle};
enum Clip : uint8_t {
    confirmSound, marchSound, formationSound, clashSound, arrowSound, drumSound,
    victorySound, defeatSound, chargeVoice, holdVoice, flankVoice, skillVoice,
    breakVoice, rallyVoice, challengeVoice, thrustVoice, parryVoice, feintVoice,
    victoryVoice, defeatVoice, noClip
};

struct GameAudio {
    static constexpr const char* settingsKey="sound-settings-v1";
    static constexpr unsigned musicBit=1,effectsBit=2,voiceBit=4;
    static constexpr const char* scores[]={
        "assets/audio/title.ogg","assets/audio/atlas.ogg","assets/audio/battle.ogg"
    };
    static constexpr const char* clips[]={
        "assets/audio/confirm.s16","assets/audio/march.s16","assets/audio/formation.s16",
        "assets/audio/clash.s16","assets/audio/arrow.s16","assets/audio/drum.s16",
        "assets/audio/victory.s16","assets/audio/defeat.s16","assets/audio/voice-charge.s16",
        "assets/audio/voice-hold.s16","assets/audio/voice-flank.s16","assets/audio/voice-skill.s16",
        "assets/audio/voice-break.s16","assets/audio/voice-rally.s16","assets/audio/voice-challenge.s16",
        "assets/audio/voice-thrust.s16","assets/audio/voice-parry.s16","assets/audio/voice-feint.s16",
        "assets/audio/voice-won.s16","assets/audio/voice-lost.s16"
    };
    static_assert(std::size(clips)==noClip);
    struct Cue {
        Clip clip=noClip;
        uint64_t expires=0;
    };

    arcade::AudioBank bank;
    std::array<Cue,3> pending;
    uint8_t options=musicBit|effectsBit|voiceBit;
    bool loaded=false, opening=false, writing=false, settingsDirty=false, paused=false;
    Score score=Score::title, playing=Score::title;
    uint64_t now=0, instance=0, retryAt=0, voiceUntil=0, gainAt=0;
    int currentGain=-26;
    bool musicReady=false;
    static constexpr uint32_t soundBytesPerSecond=16000*2; // S16LE mono

    bool enabled(unsigned bit) const {return (options&bit)!=0;}

    void log(const char* action,const char* name) {
        char message[110];
        std::snprintf(message,sizeof message,"han-banners audio %s %s",action,name);
        (void)bank.context->log().write(pxa::LogLevel::info,message);
    }
    pxa::Task<void> initialize() {
        std::array<std::byte,1> data{};
        const auto n=co_await bank.context->storage().get(settingsKey,data);
        if (n && *n==data.size() && uint8_t(data[0])<=7) options=uint8_t(data[0]);
        loaded=true;
        if (options) co_await bank.open(0,{225,-2*256,192});
        opening=false;
        co_return pxa::Result<void>{};
    }
    void start(pxa::Context& context) {
        bank.context=&context;
        opening=true;
        if (!context.tasks().start(initialize())) {opening=false;loaded=true;}
    }
    pxa::Task<void> reopen() {
        co_await bank.open(0,{225,-2*256,192});
        opening=false;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> save() {
        while (settingsDirty) {
            settingsDirty=false;
            const std::array data{std::byte(options)};
            auto result=co_await bank.context->storage().set(settingsKey,data);
            if (!result) log("settings save failed",settingsKey);
        }
        writing=false;
        co_return pxa::Result<void>{};
    }
    void clearCue(unsigned track) {
        if (pending[track].clip!=noClip) bank.release(pending[track].clip);
        pending[track]={};
        if (bank.session) (void)bank.session->control_sound(pxa::SoundTrack(track),pxa::MusicAction::stop);
    }
    void clearCues() {
        for (unsigned i=0;i<pending.size();++i) clearCue(i);
        voiceUntil=0;
    }
    void stopMusic() {
        if (bank.session && instance) (void)bank.session->control_music(pxa::MusicAction::stop);
        instance=0;
        musicReady=false;
        retryAt=0;
    }
    void toggle(unsigned bit) {
        if (!loaded) return;
        options^=uint8_t(bit);
        if (!enabled(bit)) {
            if (bit==musicBit) stopMusic();
            if (bit==effectsBit) {clearCue(0);clearCue(1);}
            if (bit==voiceBit) {clearCue(2);voiceUntil=0;}
        } else if (!bank.session && !opening) {
            bank.unavailable=false;
            opening=true;
            if (!bank.context->tasks().start(reopen())) opening=false;
        }
        settingsDirty=true;
        if (!writing) {
            writing=true;
            if (!bank.context->tasks().start(save())) writing=false;
        }
    }
    void scene(Score next) {
        if (score==next) return;
        score=next;
        retryAt=0;
        clearCues();
    }
    void pause(bool value) {
        if (paused==value) return;
        paused=value;
        if (value) clearCues();
        if (bank.session && instance) (void)bank.session->control_music(
            value?pxa::MusicAction::pause:pxa::MusicAction::resume);
    }
    void cue(Clip clip) {
        const bool voice=clip>=chargeVoice;
        if (clip==noClip || !bank.session || paused || !enabled(voice?voiceBit:effectsBit)) return;
        auto& next=pending[voice?2:clip==confirmSound?0:1];
        const bool ambient=clip==arrowSound || clip==clashSound;
        if (ambient && next.clip!=noClip && next.clip!=arrowSound && next.clip!=clashSound) return;
        if (next.clip!=noClip && next.clip!=clip) bank.release(next.clip);
        const bool result=clip==victorySound || clip==defeatSound ||
            clip==victoryVoice || clip==defeatVoice;
        next={clip,now+(result?1800000:clip==marchSound?1000000:voice?650000:350000)};
    }
    void warmOpening(bool defense) {
        if (paused || !bank.session) return;
        const Clip voice=defense?holdVoice:chargeVoice;
        if (enabled(effectsBit) && pending[1].clip==noClip)
            bank.prepare(drumSound,clips[drumSound]);
        if (enabled(voiceBit) && pending[2].clip==noClip)
            bank.prepare(voice,clips[voice]);
    }
    void releaseOpening(bool defense) {
        bank.release(drumSound);
        bank.release(defense?holdVoice:chargeVoice);
    }
    bool event(const pxa::Event& event) {
        if (auto revoked=pxa::decode_permission_revoked(event);revoked && revoked->name=="audio.playback") {
            clearCues();
            stopMusic();
            bank.session.reset();
            bank.permission.reset();
            return true;
        }
        auto update=pxa::decode_playback(event);
        if (!update || !bank.session || update->session!=bank.session->handle()) return false;
        if (update->instance!=instance) return true;
        if (update->state==pxa::PlaybackState::ready) {
            musicReady=true;
            log("music",scores[unsigned(playing)]);
        } else if (update->state!=pxa::PlaybackState::replaced) {
            if (update->state==pxa::PlaybackState::error)
                log("music decode failed",scores[unsigned(playing)]);
            instance=0;
            musicReady=false;
            retryAt=now+3000000;
        }
        return true;
    }
    void tick(uint64_t timestamp) {
        now=timestamp;
        if (paused || !bank.session) return;
        for (int track=int(pending.size())-1;track>=0;--track) {
            auto& next=pending[track];
            if (next.clip==noClip) continue;
            if (now>next.expires) {bank.release(next.clip);next={};continue;}
            const auto* asset=bank.prepare(next.clip,clips[next.clip]);
            if (!asset) continue;
            const bool ambience=next.clip==arrowSound || next.clip==clashSound;
            const int gain=track==2?-4:track==0?-16:ambience && now<voiceUntil?-14:-10;
            const auto played=bank.session->sound_track(*asset,pxa::SoundTrack(track),false,gain*256);
            if (played) {
                if (track==2) {
                    voiceUntil=now+uint64_t(asset->descriptor().decoded_bytes)*1000000/soundBytesPerSecond;
                    gainAt=now;
                }
                // The host keeps its own pin until this clip finishes.
                log(track==2?"voice":"effect",clips[next.clip]);
            } else if (arcade::transient_audio_error(played.error())) continue;
            bank.release(next.clip);
            next={};
        }
        if (!enabled(musicBit)) return;
        if (!instance || playing!=score) {
            if (now<retryAt) return;
            stopMusic();
            auto started=bank.session->music(scores[unsigned(score)],true,-26*256);
            if (!started) {retryAt=now+1500000;return;}
            instance=*started;
            playing=score;
            currentGain=-26;
        }
        if (!musicReady || now<gainAt) return;
        const bool spoken=now<voiceUntil;
        const int sceneGain=playing==Score::battle?-14:-12;
        const int goal=sceneGain-(spoken?9:0);
        const int gain=currentGain+std::clamp(goal-currentGain,-5,spoken?2:1);
        if (gain!=currentGain && bank.session->control_music(pxa::MusicAction::gain,gain*256))
            currentGain=gain;
        gainAt=now+50000;
    }
};

}
