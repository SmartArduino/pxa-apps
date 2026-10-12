#include "audio.h"

#include "sfx_profiles.h"
#include "world.h"

#define AUDIO_PERMISSION_REQUEST UINT32_C(0x4D5A0001)
#define AUDIO_OPEN_REQUEST UINT32_C(0x4D5A0002)
#define AUDIO_GRAPH_REQUEST UINT32_C(0x4D5A0003)
#define AUDIO_SAMPLE_RATE 16000u
#define AUDIO_GRAPH_GAIN_DB_Q8 (-256)

static void zero_bytes(uint8_t *data, uint32_t size) {
    uint32_t index;
    for (index = 0; index < size; ++index) {
        data[index] = 0;
    }
}

static const tone_spec_t *profile_for(uint8_t sound_id, uint32_t *count) {
    switch (sound_id) {
        case SND_SHOTGUN:
            *count = sizeof(kShotgun) / sizeof(kShotgun[0]);
            return kShotgun;
        case SND_EMPTY_CLICK:
            *count = sizeof(kEmptyClick) / sizeof(kEmptyClick[0]);
            return kEmptyClick;
        case SND_IMP_ALERT:
            *count = sizeof(kImpAlert) / sizeof(kImpAlert[0]);
            return kImpAlert;
        case SND_IMP_FIREBALL:
            *count = sizeof(kImpFireball) / sizeof(kImpFireball[0]);
            return kImpFireball;
        case SND_IMP_MELEE:
            *count = sizeof(kImpMelee) / sizeof(kImpMelee[0]);
            return kImpMelee;
        case SND_IMP_PAIN:
            *count = sizeof(kImpPain) / sizeof(kImpPain[0]);
            return kImpPain;
        case SND_IMP_DEATH:
            *count = sizeof(kImpDeath) / sizeof(kImpDeath[0]);
            return kImpDeath;
        case SND_FIREBALL_EXPLODE:
            *count = sizeof(kFireballExplode) / sizeof(kFireballExplode[0]);
            return kFireballExplode;
        case SND_PLAYER_PAIN:
            *count = sizeof(kPlayerPain) / sizeof(kPlayerPain[0]);
            return kPlayerPain;
        case SND_PICKUP_HEALTH:
            *count = sizeof(kPickupHealth) / sizeof(kPickupHealth[0]);
            return kPickupHealth;
        case SND_PICKUP_AMMO:
            *count = sizeof(kPickupAmmo) / sizeof(kPickupAmmo[0]);
            return kPickupAmmo;
        case SND_DOOR_OPEN:
            *count = sizeof(kDoorOpen) / sizeof(kDoorOpen[0]);
            return kDoorOpen;
        case SND_DOOR_CLOSE:
            *count = sizeof(kDoorClose) / sizeof(kDoorClose[0]);
            return kDoorClose;
        case SND_EXIT_SEALED:
            *count = sizeof(kExitSealed) / sizeof(kExitSealed[0]);
            return kExitSealed;
        case SND_WIN:
            *count = sizeof(kWin) / sizeof(kWin[0]);
            return kWin;
        case SND_DIE:
            *count = sizeof(kDie) / sizeof(kDie[0]);
            return kDie;
        default:
            *count = 0;
            return 0;
    }
}

static int16_t host_gain_db_q8(uint16_t volume_per_mille) {
    static const uint16_t thresholds[] = {
        708, 501, 355, 251, 178, 126, 89, 63, 45, 32, 22, 16, 11, 8,
    };
    uint32_t index;
    if (volume_per_mille >= 1000) return 0;
    for (index = 0; index < sizeof(thresholds) / sizeof(thresholds[0]);
         ++index) {
        if (volume_per_mille >= thresholds[index]) {
            return (int16_t)(-(int32_t)(index + 1u) * 3 * 256);
        }
    }
    return (int16_t)(-48 * 256);
}

/* Four replaceable categories fit the Host's four slots without keeping a
 * stale event backlog: weapon, enemies, world, player. The wire bridge here
 * serves this app's existing session; new apps use AudioSession::effect. */
static uint8_t effect_track(uint8_t sound_id) {
    if (sound_id <= SND_EMPTY_CLICK) return 0;
    if (sound_id <= SND_FIREBALL_EXPLODE) return 1;
    if (sound_id >= SND_DOOR_OPEN && sound_id <= SND_EXIT_SEALED) return 2;
    return 3;
}
static int32_t submit_host_effect(game_audio_t *audio, const tone_spec_t *specs,
    uint32_t count, uint8_t gain, uint8_t track) {
    if (!count || count > 8) return PXA_STATUS_INVALID_ARGUMENT;
    uint8_t command[4 + 14 * 8] = {0};
    command[0] = (uint8_t)count; command[1] = track;
    for (uint32_t i = 0; i < count; ++i) {
        const tone_spec_t *s = &specs[i];
        const uint16_t volume = (uint16_t)(((uint32_t)s->volume_per_mille * gain + 127u) / 255u);
        uint8_t *p = command + 4 + 14 * i;
        pxa_store_u16(p, s->frequency_hz < 40 ? 40 : s->frequency_hz);
        pxa_store_u16(p + 2, s->duration_ms);
        pxa_store_u16(p + 4, (uint16_t)host_gain_db_q8(volume));
        p[6] = s->waveform - 1;
        pxa_store_u16(p + 8, s->attack_ms);
        pxa_store_u16(p + 10, s->release_ms);
        pxa_store_u16(p + 12, s->delay_ms);
    }
    return pxa_io(audio->session_handle, 0x107, command, 4 + 14 * count);
}

void audio_init(game_audio_t *audio) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    uint8_t packet[64];
    zero_bytes((uint8_t *)audio, (uint32_t)sizeof(*audio));
    audio->state = AUDIO_WAIT_PERMISSION;
    uint32_t packet_size = 0;
    if (!pxa_permission_build(
            packet, sizeof(packet), PXA_PERMISSION_ACQUIRE,
            AUDIO_PERMISSION_REQUEST, permission_name,
            sizeof(permission_name) - 1u, permission_scope,
            sizeof(permission_scope) - 1u, &packet_size) ||
        pxa_control_packet(packet, packet_size) != PXA_STATUS_OK) {
        audio->state = AUDIO_UNAVAILABLE;
    }
}

void audio_play(game_audio_t *audio, uint8_t sound_id, uint8_t gain) {
    if (audio->state != AUDIO_READY || gain < 8u) return;
    uint32_t count = 0;
    const tone_spec_t *tones = profile_for(sound_id, &count);
    if (!tones) return;
    const int32_t status = submit_host_effect(audio, tones, count, gain, effect_track(sound_id));
    if (status == PXA_STATUS_UNSUPPORTED) {
        audio->state = AUDIO_UNAVAILABLE;
        (void)pxa_log_write(3, "maze-evil requires Audio 0.9 effects");
    } else if (status < 0) {
        (void)pxa_log_write(3, "maze-evil audio effect rejected");
    }
}

int audio_handle_event(game_audio_t *audio, const pxa_event_t *event,
                       uint8_t *packet, size_t packet_capacity) {
    (void)packet;
    (void)packet_capacity;
    if (event->service == PXA_PERMISSION_SERVICE &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->token == AUDIO_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event, AUDIO_PERMISSION_REQUEST,
                                             &result) ||
            result.status != PXA_STATUS_OK) {
            audio->state = AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->permission_handle = result.handle;
        audio->state = AUDIO_WAIT_OPEN;
        if (pxa_audio_open_media(AUDIO_OPEN_REQUEST,
                                    audio->permission_handle) != PXA_STATUS_OK) {
            audio->state = AUDIO_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_AUDIO_SERVICE &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->token == AUDIO_OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(event, AUDIO_OPEN_REQUEST, &result) ||
            result.status != PXA_STATUS_OK ||
            result.sample_rate != AUDIO_SAMPLE_RATE || result.channels != 1 ||
            result.frame_ms != 20) {
            audio->state = AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->session_handle = result.handle;
        audio->state = AUDIO_WAIT_GRAPH;
        const pxa_audio_eq_band_t band = {1500, 256, 256};
        const pxa_audio_graph_t graph = {AUDIO_GRAPH_GAIN_DB_Q8,
                                             &band, 1};
        if (pxa_audio_commit_graph(AUDIO_GRAPH_REQUEST,
                                      audio->session_handle,
                                      &graph) != PXA_STATUS_OK) {
            audio->state = AUDIO_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_AUDIO_SERVICE &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->token == AUDIO_GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(event, AUDIO_GRAPH_REQUEST,
                                       PXA_AUDIO_COMMIT_GRAPH, &status) ||
            status != PXA_STATUS_OK) {
            audio->state = AUDIO_UNAVAILABLE;
        } else {
            audio->state = AUDIO_READY;
            (void)pxa_log_write(2, "maze-evil audio ready");
        }
        return 1;
    }
    return 0;
}

void audio_stop(game_audio_t *audio) {
    if (audio->session_handle != 0) {
        (void)pxa_close_handle(audio->session_handle);
        audio->session_handle = 0;
    }
    if (audio->permission_handle != 0) {
        (void)pxa_close_handle(audio->permission_handle);
        audio->permission_handle = 0;
    }
    audio->state = AUDIO_OFF;
}
