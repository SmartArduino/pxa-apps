// Diagnostic: renders the real Host mixer using the game's tone profiles.
// Link libpxa/src/services/audio/audio_mixer.c; no simulator or device needed.
#include "pxa/audio_mixer.h"
#include "../sfx_profiles.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

struct Profile { const char* name; const tone_spec_t* tones; unsigned count; };
#define PROFILE(name) {#name, k##name, sizeof(k##name)/sizeof(k##name[0])}
static const Profile profiles[] = {
    PROFILE(Shotgun), PROFILE(EmptyClick), PROFILE(ImpAlert),
    PROFILE(ImpFireball), PROFILE(ImpMelee), PROFILE(ImpPain),
    PROFILE(ImpDeath), PROFILE(FireballExplode), PROFILE(PlayerPain),
    PROFILE(PickupHealth), PROFILE(PickupAmmo), PROFILE(DoorOpen),
    PROFILE(DoorClose), PROFILE(ExitSealed), PROFILE(Win), PROFILE(Die),
};

int main(int argc, char**) {
    const bool effects = argc > 1;
    std::puts("{\"scope\":\"Host sample clock; excludes scheduling, sink and speaker\",\"profiles\":[");
    bool comma = false;
    for (const auto& profile : profiles) {
        pxa_audio_mixer_t mixer{};
        auto& voice = mixer.voices[0];
        pxa_audio_mixer_reset(&voice);
        pxa_audio_graph_t graph{};
        graph.route = PXA_AUDIO_ROUTE_SPEAKER;
        assert(pxa_audio_mixer_commit(&voice, &graph) == PXA_STATUS_OK);
        unsigned accepted = 0, desired_end = 0;
        pxa_audio_tone_t layers[PXA_AUDIO_EFFECT_LAYERS]{};
        for (unsigned i = 0; i < profile.count; ++i) {
            const auto& spec = profile.tones[i];
            desired_end = std::max(desired_end, unsigned(spec.delay_ms + spec.duration_ms));
            pxa_audio_tone_t tone{};
            tone.frequency_hz = std::max<unsigned>(40, spec.frequency_hz);
            tone.duration_ms = spec.duration_ms;
            tone.gain_db_q8 = -12*256;
            tone.attack_ms = spec.attack_ms;
            tone.release_ms = spec.release_ms;
            tone.delay_ms = spec.delay_ms;
            tone.waveform = spec.waveform-1;
            layers[i] = tone;
            const auto status = effects ? PXA_STATUS_OK : pxa_audio_mixer_tone(&voice, &tone);
            assert(status == PXA_STATUS_OK || status == PXA_STATUS_WOULD_BLOCK);
            accepted += status == PXA_STATUS_OK;
        }
        if (effects) assert(pxa_audio_mixer_effect(&voice, layers, profile.count, 0) == PXA_STATUS_OK);
        unsigned samples = 0, onset[PXA_AUDIO_EFFECT_LAYERS]{};
        while (voice.count) {
            if (effects) {
                const auto& packet = voice.packets[0];
                for (unsigned i = 0; i < profile.count; ++i)
                    if (packet.position == layers[i].delay_ms * 16u) onset[i] = samples;
            } else {
                auto& packet = voice.packets[voice.read];
                if (packet.position == packet.meta.tone.delay_ms*16u)
                    onset[voice.read] = samples;
            }
            int16_t output;
            pxa_audio_mixer_render(&mixer, &output, 1);
            ++samples;
        }
        std::printf("%s{\"name\":\"%s\",\"desired_duration_ms\":%u,\"rendered_duration_ms\":%.3f,"
                    "\"accepted\":%u,\"dropped\":%u,\"actual_onsets_ms\":[",
            comma ? ",\n" : "", profile.name, desired_end, samples/16.,
            accepted, profile.count-accepted);
        for (unsigned i=0; i<accepted; ++i)
            std::printf("%s%.3f", i ? "," : "", onset[i]/16.);
        std::printf("],\"desired_onsets_ms\":[");
        for (unsigned i=0; i<profile.count; ++i)
            std::printf("%s%u", i ? "," : "", profile.tones[i].delay_ms);
        std::printf("]}");
        comma = true;
    }
    std::puts("\n]}");
}
