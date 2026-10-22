/* GPL-2.0-or-later. Frontend adapter for the shared unlimited snapshot list. */
static void app_state_reset_output(void) {
    emu_schedule_reset(&schedule);emu_pacer_reset(&pacer);emu_audio_fifo_clear(&audio_fifo);
    if(audio){emu_audio_start(&audio_state,audio_rate,audio_channels);pxa_audio_flush(++save_cookie+1000,audio);}
    emu_video_set_fit(&video_renderer,menu_area,video_renderer.fit);clear_frames=3;
}
#define app_state_size emu_state_size
#define app_state_save emu_state_save
#define app_state_load emu_state_load
#define app_state_checksum emu_core_checksum
#define app_state_log emu_port_log
#define APP_STATE_TAG "EMU"
#define APP_STATE_PREFIX "st"
#define APP_STATE_EXTENSION ".pst"
#include "state_slots.h"
