/* GPL-2.0-or-later. Frontend adapter for the shared unlimited snapshot list. */
static void app_state_reset_output(void) {
    nes_schedule_reset(&schedule);nes_pacer_reset(&pacer);nes_audio_fifo_clear(&audio_fifo);
    if(audio)pxa_audio_flush(++save_cookie+1000,audio);nes_video_reset(&video_renderer);
}
#define app_state_size nes_state_size
#define app_state_save nes_state_save
#define app_state_load nes_state_load
#define app_state_checksum nes_core_checksum
#define app_state_log nes_port_log
#define APP_STATE_TAG "NES"
#define APP_STATE_PREFIX "ns"
#define APP_STATE_EXTENSION ".pns"
#include "../common/emulator/state_slots.h"
