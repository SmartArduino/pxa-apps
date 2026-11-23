#pragma once

#include <pxa/audio.hpp>
#include "../common/cpp/audio.hpp"

namespace tide {

enum class Score { Harbor, Voyage, Battle };
enum class Sound : std::uint8_t {
    CastOff, Trim, Shoal, Anchor, Sighting, Broadside, Steel, Page,
    LandCall, AnchorCall, FireCall, None
};

struct GameAudio {
    enum : std::uint8_t { Music=1, Effects=2, Voices=4, All=7 };
    static constexpr const char* Settings="audio-v1";
    static constexpr const char* Scores[]{
        "assets/audio/harbor.ogg", "assets/audio/voyage.ogg", "assets/audio/battle.ogg"
    };
    static constexpr const char* Clips[]{
        "assets/audio/castoff.s16", "assets/audio/trim.s16", "assets/audio/shoal.s16",
        "assets/audio/anchor.s16", "assets/audio/sighting.s16", "assets/audio/broadside.s16",
        "assets/audio/steel.s16", "assets/audio/page.s16", "assets/audio/voice-land.s16",
        "assets/audio/voice-anchor.s16", "assets/audio/voice-fire.s16"
    };
    static_assert(std::size(Clips)==unsigned(Sound::None));

    struct Cue {
        Sound sound=Sound::None;
        std::uint64_t expires=0;
    };

    arcade::AudioBank bank;
    std::array<Cue,2> pending{};
    std::array<Sound,2> prepared{Sound::None,Sound::None};
    std::uint64_t now=0, instance=0, retry_at=0, scene_since=0;
    Score playing=Score::Harbor, scene=Score::Harbor;
    std::uint8_t options=All;
    bool loaded=false, opening=false, active=false, dirty=false, writing=false;

    bool enabled(unsigned bit) const { return options&bit; }

    static unsigned lane(Sound sound) { return sound>=Sound::LandCall?1:0; }

    void log(const char* action, const char* name) {
        char message[112];
        std::snprintf(message,sizeof(message),"tide-pact audio %s %s",action,name);
        (void)bank.context->log().write(pxa::LogLevel::info,message);
    }

    pxa::Task<void> open() {
        co_await bank.open(0,{225,-2*256,192});
        opening=false;
        co_return pxa::Result<void>{};
    }

    pxa::Task<void> initialize() {
        std::array<std::byte,1> bytes{};
        const auto got=co_await bank.context->storage().get(Settings,bytes);
        if (got && *got==bytes.size() && std::to_integer<unsigned>(bytes[0])<=All)
            options=std::to_integer<std::uint8_t>(bytes[0]);
        loaded=true;
        if (options) co_await bank.open(0,{225,-2*256,192});
        opening=false;
        co_return pxa::Result<void>{};
    }

    void start(pxa::Context& context) {
        bank.context=&context;
        opening=true;
        if (!context.tasks().start(initialize())) { opening=false; loaded=true; }
    }

    pxa::Task<void> persist(std::uint8_t value) {
        const std::array bytes{std::byte(value)};
        const auto result=co_await bank.context->storage().set(Settings,bytes);
        writing=false;
        co_return result;
    }

    void discard(unsigned track) {
        const auto sound=pending[track].sound;
        if (sound!=Sound::None && sound!=prepared[track]) bank.release(std::uint8_t(sound));
        pending[track]={};
    }

    void stop_track(unsigned track) {
        discard(track);
        if (prepared[track]!=Sound::None) bank.release(std::uint8_t(prepared[track]));
        prepared[track]=Sound::None;
        if (bank.session) (void)bank.session->control_sound(pxa::SoundTrack(track),pxa::MusicAction::stop);
    }

    void stop_music() {
        if (bank.session && instance) (void)bank.session->control_music(pxa::MusicAction::stop);
        instance=0;
    }

    void foreground(bool value) {
        if (active==value) return;
        active=value;
        if (!active) {
            stop_track(0);
            stop_track(1);
        }
        if (bank.session && instance)
            (void)bank.session->control_music(value?pxa::MusicAction::resume:pxa::MusicAction::pause);
    }

    void toggle(unsigned bit) {
        if (!loaded) return;
        options^=std::uint8_t(bit);
        dirty=true;
        if (enabled(bit)) {
            if (bit==Music) retry_at=0;
            if (!bank.session && !opening) {
                bank.unavailable=false;
                opening=true;
                if (!bank.context->tasks().start(open())) opening=false;
            }
        } else if (bit==Music) stop_music();
        else stop_track(bit==Effects?0:1);
    }

    void cue(Sound sound) {
        const unsigned track=lane(sound);
        if (sound==Sound::None || !bank.session || !active || !enabled(track?Voices:Effects)) return;
        if (pending[track].sound!=sound) discard(track);
        pending[track]={sound,now+850000};
    }

    bool event(const pxa::Event& event) {
        if (auto revoked=pxa::decode_permission_revoked(event); revoked && revoked->name=="audio.playback") {
            stop_track(0);
            stop_track(1);
            stop_music();
            for (auto& clip : bank.sounds) clip.reset();
            if (bank.loading) bank.release(bank.loading_id);
            bank.session.reset();
            bank.permission.reset();
            return true;
        }
        const auto update=pxa::decode_playback(event);
        if (!update || !bank.session || update->session!=bank.session->handle()) return false;
        if (update->instance==instance) {
            if (update->state==pxa::PlaybackState::ready) log("music",Scores[unsigned(playing)]);
            else if (update->state!=pxa::PlaybackState::replaced) {
                if (update->state==pxa::PlaybackState::error) {
                    char message[112];
                    std::snprintf(message,sizeof(message),"tide-pact audio music error=%ld %s",
                                  static_cast<long>(update->status),Scores[unsigned(playing)]);
                    (void)bank.context->log().write(pxa::LogLevel::warning,message);
                }
                instance=0;
                retry_at=now+2000000;
            }
        }
        return true;
    }

    void tick(std::uint64_t timestamp, Score score) {
        now=timestamp;
        if (dirty && !writing && bank.context->tasks().start(persist(options))) {
            dirty=false;
            writing=true;
        }
        if (!active || !bank.session) return;
        if (score!=scene) {
            scene=score;
            scene_since=now;
            retry_at=0;
        }
        for (unsigned track=0;track<pending.size();++track) {
            const auto cue=pending[track];
            if (cue.sound==Sound::None) continue;
            if (now>cue.expires) { discard(track); continue; }
            const auto id=std::uint8_t(cue.sound);
            const auto* clip=bank.prepare(id,Clips[id]);
            if (!clip) continue;
            const auto started=bank.session->sound_track(*clip,pxa::SoundTrack(track),false,(track?-4:-8)*256);
            if (started) {
                // Keep one ready clip per track for consecutive uses of the same cue.
                const auto previous=prepared[track];
                if (previous!=Sound::None && previous!=cue.sound) bank.release(std::uint8_t(previous));
                prepared[track]=cue.sound;
                log(track?"voice":"effect",Clips[id]);
            } else if (arcade::transient_audio_error(started.error())) continue;
            discard(track);
        }
        if (!enabled(Music) || now<retry_at || (instance && (playing==score || now-scene_since<320000))) return;
        // Accepted replacements are atomic. Retain the old track if the Host is busy.
        const auto started=bank.session->music(Scores[unsigned(score)],true,(score==Score::Battle?-14:-13)*256);
        if (!started) { retry_at=now+1500000; return; }
        instance=*started;
        playing=score;
    }
};

} // namespace tide
