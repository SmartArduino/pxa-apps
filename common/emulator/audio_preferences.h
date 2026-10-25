/* Per-app sound preference. GPL-2.0-or-later. */
#pragma once
#include <stdbool.h>
#include "pxa_storage.h"

/* Separate from frontend tokens, including save-slot and flush requests. */
#define EMULATOR_AUDIO_LOAD UINT64_C(0x534e4401)
#define EMULATOR_AUDIO_SAVE UINT64_C(0x534e4402)
static const char emulator_audio_key[] = "emulator.sound.v1";
typedef struct {
    bool enabled, ready, changed, loading, saving, writing;
    bool persisted, persisted_valid;
    int32_t error;
} emulator_audio_preferences_t;
typedef void (*emulator_audio_apply_fn)(bool enabled);

static inline int32_t emulator_audio_preferences_flush(emulator_audio_preferences_t *p) {
    if (p->saving || !p->changed ||
        (p->persisted_valid && p->enabled == p->persisted)) return 0;
    uint8_t packet[96], value = p->enabled ? 1 : 0;
    int32_t status = pxa_storage_request_set(packet, sizeof(packet), EMULATOR_AUDIO_SAVE,
        emulator_audio_key, sizeof(emulator_audio_key)-1, &value, 1);
    p->error = status;
    if (!status) { p->saving = true; p->writing = p->enabled; }
    return status;
}

static inline void emulator_audio_preferences_start(emulator_audio_preferences_t *p,
                                                     emulator_audio_apply_fn apply) {
    *p = (emulator_audio_preferences_t){.enabled = true, .loading = true};
    p->error = pxa_storage_request_get(EMULATOR_AUDIO_LOAD,
        emulator_audio_key, sizeof(emulator_audio_key)-1);
    if (p->error) { p->loading = false; p->ready = true; apply(true); }
}

/* Called only for a user choice. Audio/device failures never write preferences. */
static inline int32_t emulator_audio_preferences_set(emulator_audio_preferences_t *p,
                                                      bool enabled) {
    p->enabled = enabled;
    p->ready = p->changed = true;
    return emulator_audio_preferences_flush(p);
}

static inline bool emulator_audio_preferences_event(emulator_audio_preferences_t *p,
    const pxa_event_t *event, emulator_audio_apply_fn apply) {
    if (event->service != PXA_STORAGE_SERVICE) return false;
    if (event->token == EMULATOR_AUDIO_LOAD) {
        if (!p->loading) return true;
        p->loading = false;
        pxa_storage_get_result_t r;
        bool valid = pxa_storage_parse_get(event, EMULATOR_AUDIO_LOAD, &r);
        /* A late GET must not undo a choice or a SET made since launch. */
        if (!p->changed) {
            p->enabled = !(valid && !r.status && r.value.size == 1 && r.value.data[0] == 0);
            p->persisted_valid = valid && !r.status && r.value.size == 1 && r.value.data[0] <= 1;
            p->persisted = p->enabled;
            p->error = !valid ? PXA_STATUS_PROTOCOL_ERROR :
                (r.status == PXA_STATUS_NOT_FOUND ? 0 : r.status);
            p->ready = true;
            apply(p->enabled);
        }
        return true;
    }
    if (event->token == EMULATOR_AUDIO_SAVE) {
        if (!p->saving) return true;
        p->saving = false;
        int32_t status;
        p->error = pxa_storage_parse_status(event, EMULATOR_AUDIO_SAVE, PXA_STORAGE_SET, &status)
            ? status : PXA_STATUS_PROTOCOL_ERROR;
        if (!p->error) {
            p->persisted = p->writing;
            p->persisted_valid = true;
            emulator_audio_preferences_flush(p);
        }
        return true;
    }
    return false;
}
