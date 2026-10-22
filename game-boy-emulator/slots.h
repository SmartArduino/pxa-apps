/* GPL-2.0-or-later. Frontend adapter for the shared unlimited snapshot list. */
static void app_state_reset_output(void) {
    gb_schedule_reset(&schedule);gb_pacer_reset(&pacer);gb_audio_fifo_clear(&audio_fifo);
    if(audio){gb_audio_start(&audio_state,audio_rate,audio_channels);pxa_audio_flush(++save_cookie+1000,audio);}
    gb_video_set_fit(&video_renderer,menu_area,video_renderer.fit);clear_frames=3;
}
#define app_state_size gb_state_size
#define app_state_save gb_state_save
#define app_state_load gb_state_load
#define app_state_checksum gb_core_checksum
#define app_state_log gb_port_log
#define APP_STATE_TAG "GB"
#define APP_STATE_PREFIX "st"
#define APP_STATE_EXTENSION ".pst"
#include "../common/emulator/state_slots.h"
