#define PXA_LAB_MODULE_PREFIX pxa_lab_audio_
#include "pxa_lab_module.h"

#include "pxa_audio.h"
#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_permission.h"

#define AUDIO_NODE 2u
#define PERMISSION_REQUEST 1u
#define OPEN_REQUEST 2u
#define GRAPH_REQUEST 3u
#define QUERY_REQUEST 4u
#define FLUSH_REQUEST 5u
#define AUDIO_SAMPLE_RATE 16000u
#define AUDIO_CHANNELS 1u
#define AUDIO_FRAME_MS 20u
#define AUDIO_FRAME_SAMPLES 320u
#define TONE_PHASE_STEP 1802u

static uint8_t packet[1400];
static uint8_t payload[128];
static uint32_t permission_handle;
static uint32_t session_handle;
static uint16_t tone_phase;
static int16_t tone_frame[AUDIO_FRAME_SAMPLES];
static uint8_t waiting;
static uint8_t denied;
static uint8_t committed;
static uint8_t playing;
static uint8_t unavailable;
static uint8_t query_pending;
static uint8_t flush_pending;
static uint8_t query_tick;
static uint8_t state_valid;
static pxa_audio_state_result_t audio_state;
static char state_text[96];

static size_t append_text(size_t offset, const char *text) {
    size_t index = 0;
    while (text[index] != '\0' && offset + 1u < sizeof(state_text))
        state_text[offset++] = text[index++];
    state_text[offset] = '\0';
    return offset;
}

static size_t append_u64(size_t offset, uint64_t value) {
    char reverse[20];
    size_t count = 0;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reverse));
    while (count != 0 && offset + 1u < sizeof(state_text))
        state_text[offset++] = reverse[--count];
    state_text[offset] = '\0';
    return offset;
}

static void format_audio_state(void) {
    size_t offset = 0;
    offset = append_text(offset, "submitted ");
    offset = append_u64(offset, audio_state.submitted_samples);
    offset = append_text(offset, " / accepted ");
    offset = append_u64(offset, audio_state.accepted_samples);
    offset = append_text(offset, " / queued ");
    (void)append_u64(offset, audio_state.queued_samples);
}

static int request_query(void) {
    if (session_handle == 0 || query_pending) return 1;
    query_pending = 1;
    if (pxa_audio_query_state(QUERY_REQUEST, session_handle, packet,
                              sizeof(packet)))
        return 1;
    query_pending = 0;
    return 0;
}

static int submit_tone_frame(void) {
    uint16_t phase_before = tone_phase;
    size_t index;
    int32_t result;
    for (index = 0; index < AUDIO_FRAME_SAMPLES; ++index) {
        const uint16_t ramp = (tone_phase & UINT16_C(0x8000)) != 0
                                  ? (uint16_t)(UINT16_MAX - tone_phase)
                                  : tone_phase;
        tone_frame[index] = (int16_t)(((int32_t)ramp - 16384) / 2);
        tone_phase = (uint16_t)(tone_phase + TONE_PHASE_STEP);
    }
    result = pxa_audio_write_pcm(session_handle, (uint8_t *)tone_frame,
                                 sizeof(tone_frame));
    if (result == PXA_STATUS_WOULD_BLOCK) {
        tone_phase = phase_before;
        return 1;
    }
    return result == (int32_t)sizeof(tone_frame);
}

static int render(void) {
    pxa_ui_demo_page_t page;
    static const char start[] = "PLAY TEST TONE";
    static const char stop[] = "STOP TEST TONE";
    static const char resume[] = "RESUME TEST TONE";
    static const char ready[] = "Tap to request playback";
    static const char loading[] = "Opening audio session...";
    static const char active[] = "Test tone playing";
    static const char paused[] = "Test tone paused";
    static const char no[] = "Playback permission denied";
    static const char unsupported[] = "Audio output unavailable";
    static const char flushing[] = "Flushing queued PCM...";
    const char *action = playing ? stop : committed ? resume : start;
    const char *state = unavailable ? unsupported : denied ? no :
                        flush_pending ? flushing : state_valid ? state_text :
                        playing ? active : committed ? paused :
                        waiting ? loading : ready;
    page = (pxa_ui_demo_page_t){
        "Audio Playback Lab", "Speaker graph / 16 kHz mono", state, action,
        NULL, PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_SWITCH | PXA_UI_DEMO_PAGE_HAS_PROGRESS,
        11, playing ? 100 : 0, playing, 0,
        (uint8_t)(!waiting && !denied && !unavailable)};
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int request_permission(void) {
    static const char name[] = "audio.playback";
    static const uint8_t scope[] = "media";
    waiting = 1; denied = 0; unavailable = 0; committed = 0; playing = 0;
    query_pending = 0; flush_pending = 0; state_valid = 0; query_tick = 0;
    return pxa_permission_acquire(PERMISSION_REQUEST, name, sizeof(name) - 1,
                                  scope, sizeof(scope) - 1, payload, sizeof(payload),
                                  packet, sizeof(packet));
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config; (void)config_length;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) && !waiting &&
        ((ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
          ui_event.kind == PXA_UI_EVENT_CLICK_KIND) ||
         (ui_event.node == PXA_UI_DEMO_PAGE_NODE_SWITCH &&
          ui_event.kind == PXA_UI_EVENT_VALUE_CHANGED_KIND))) {
        if (committed) {
            if (playing) {
                playing = 0;
                flush_pending = 1;
                if (!pxa_clock_set_period(0) ||
                    !pxa_audio_flush(FLUSH_REQUEST, session_handle, packet,
                                     sizeof(packet))) {
                    flush_pending = 0;
                    return PXA_STATUS_INTERNAL;
                }
            } else {
                playing = 1;
                if (!pxa_clock_set_period(AUDIO_FRAME_MS) ||
                    !submit_tone_frame() || !request_query()) {
                    playing = 0;
                    unavailable = 1;
                }
            }
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return request_permission() && render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_PERMISSION && parsed.opcode == PXA_PERMISSION_ACQUIRE &&
        parsed.request_id == PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            waiting = 0; denied = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        permission_handle = result.handle;
        return pxa_audio_open_media(OPEN_REQUEST, permission_handle, payload, sizeof(payload),
                                    packet, sizeof(packet)) ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_AUDIO && parsed.opcode == PXA_AUDIO_OPEN_SESSION &&
        parsed.request_id == OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            waiting = 0; unavailable = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (result.sample_rate != AUDIO_SAMPLE_RATE ||
            result.channels != AUDIO_CHANNELS ||
            result.frame_ms != AUDIO_FRAME_MS) {
            waiting = 0; unavailable = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        session_handle = result.session_handle;
        return pxa_audio_commit_speaker_graph(GRAPH_REQUEST, session_handle, -256, 1500, 256, 256,
                                              payload, sizeof(payload), packet, sizeof(packet))
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_AUDIO && parsed.opcode == PXA_AUDIO_COMMIT_GRAPH &&
        parsed.request_id == GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(&parsed, PXA_AUDIO_COMMIT_GRAPH, &status)) return PXA_STATUS_INTERNAL;
        waiting = 0;
        committed = status == PXA_STATUS_OK;
        playing = committed;
        unavailable = !committed;
        if (playing &&
            (!pxa_clock_set_period(AUDIO_FRAME_MS) || !submit_tone_frame() ||
             !request_query())) {
            playing = 0;
            unavailable = 1;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_AUDIO &&
        parsed.opcode == PXA_AUDIO_QUERY_STATE &&
        parsed.request_id == QUERY_REQUEST) {
        query_pending = 0;
        if (!pxa_audio_parse_state(&parsed, &audio_state) ||
            audio_state.status != PXA_STATUS_OK) {
            unavailable = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        state_valid = 1;
        format_audio_state();
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_AUDIO &&
        parsed.opcode == PXA_AUDIO_FLUSH && parsed.request_id == FLUSH_REQUEST) {
        int32_t status;
        flush_pending = 0;
        if (!pxa_audio_parse_status(&parsed, PXA_AUDIO_FLUSH, &status) ||
            status != PXA_STATUS_OK || !request_query()) {
            unavailable = 1;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        playing) {
        if (!submit_tone_frame()) {
            playing = 0;
            unavailable = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (++query_tick >= 5u) {
            query_tick = 0;
            if (!request_query()) {
                playing = 0;
                unavailable = 1;
                return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
            }
        }
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
}
