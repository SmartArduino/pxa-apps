/* SPDX-License-Identifier: MIT. Standalone adapter native protocol/scope checks. */
#include <assert.h>
#include <stdio.h>
#include "core.h"
#include "scheduler.h"
#include "controls.h"
static unsigned checks;
#define CHECK(name,c) do{assert(c);printf("PASS %s\n",name);checks++;}while(0)
int main(void){
    saf_boot();uint8_t before[1024],after[1024];unsigned bytes=saf_pack_state(before);
    CHECK("standalone_typed_wire",bytes==(SAF_ENTRY_ID==1?637:567));
    CHECK("original_menu_boot",saf_current_state()==0&&!saf_io.fault);
    for(unsigned i=0;i<256;i++){uint8_t r,g,b;SAF_colorToRGB(i,&r,&g,&b);assert(saf_rgb565(i)==((r>>3)<<11|(g>>2)<<5|(b>>3)));}
    CHECK("all256_original_palette",saf_rgb565(3)==31&&saf_rgb565(255)==65535);
    saf_resume();saf_pack_state(after);CHECK("continue_preserves_all_original_state",memcmp(before,after,bytes)==0);
    saf_pointer_mask(16,0);saf_pointer_mask(0,1);saf_frame();CHECK("short_tap_actual_applied_one_frame",saf_io.applied==16&&SAF_buttonJustPressed(4)&&!saf_io.pending);
    saf_frame();CHECK("short_tap_releases_next_tick",!SAF_buttonPressed(4));
    saf_pointer_mask(8,0);saf_release_all();saf_frame();CHECK("cancel_clears_pending_native_only",!saf_io.applied&&!SAF_buttonPressed(3));
    saf_controller_mask(32);saf_pointer_mask(1,0);saf_frame();CHECK("pointer_controller_combination",saf_io.applied==33);saf_release_all();
    saf_controller_mask(16);saf_controller_mask(0);saf_pointer_mask(32,0);saf_frame();CHECK("pointer_does_not_erase_short_controller_pulse",saf_io.applied==48);saf_release_all();
    saf_pointer_mask(16,0);saf_pointer_mask(0,1);saf_controller_mask(32);saf_controller_mask(0);saf_frame();CHECK("controller_does_not_erase_short_pointer_pulse",saf_io.applied==48);saf_release_all();
    saf_new_game();uint32_t clock=_SAF_frame;uint8_t rng=_SAF_currentRandom;SAF_save(7,42);saf_new_game();
    CHECK("new_original_menu_retains_time_rng",saf_current_state()==0&&_SAF_frame==clock&&_SAF_currentRandom==rng);
#if SAF_ENTRY_ID == 1
    CHECK("real_touch_combo_hit_bounds",saf_touch_mask(230,326)==36&&saf_touch_mask(331,326)==40&&saf_touch_mask(278,326)==0&&saf_touch_mask(230,336)==0);
    saf_pointer_mask(saf_touch_mask(230,326),0);saf_frame();CHECK("touch_B_LEFT_applies_original_two_keys",saf_io.applied==36&&SAF_buttonPressed(5)&&SAF_buttonPressed(2));
    saf_pointer_mask(saf_touch_mask(331,326),0);saf_frame();CHECK("touch_B_RIGHT_changes_only_pointer_owner",saf_io.applied==40&&SAF_buttonPressed(5)&&SAF_buttonPressed(3)&&!SAF_buttonPressed(2));
    saf_pointer_mask(0,1);saf_frame();CHECK("combo_up_releases_both_original_keys",!saf_io.applied&&!SAF_buttonPressed(5)&&!SAF_buttonPressed(3));
    clock=_SAF_frame;
    CHECK("new_retains_session_save",SAF_load(7)==42);saf_restart_game();CHECK("restart_original_map",UTD_gameState==1&&UTD_mapIndex==0&&UTD_round==0&&_SAF_frame==clock);
    saf_pointer_mask(32,0);for(unsigned i=0;i<260;i++)assert(saf_frame());CHECK("B_held_original_health_not_minigames_menu",UTD_gameState==1&&SAF_buttonPressed(5)==255);saf_release_all();
    CHECK("UTD_field_only_574_and_renderer_scratch",saf_pack_state(before)==637&&!saf_io.fault);
#else
    CHECK("Chess_no_microtd_combo_hit",!saf_touch_mask(230,326)&&!saf_touch_mask(331,326));
    CHECK("original_chess_backend_saves_disabled",SAF_load(7)==42&&saf_io.save_writes==0);saf_restart_game();CHECK("restart_original_chess_init",state==STATE_PLAYING&&game.ply==0&&_SAF_frame==clock);
    saf_pointer_mask(32,0);saf_frame();CHECK("B_original_chess_menu_first_edge",state==STATE_MENU&&SAF_buttonJustPressed(5));saf_release_all();
    CHECK("Chess_no_pointer_or_padding_wire",saf_pack_state(before)==567&&!saf_io.fault);
#endif
    saf_clock_t timer={0};assert(!saf_clock_due(&timer,0));assert(!saf_clock_due(&timer,39));CHECK("40ms_tick",saf_clock_due(&timer,40)==1);
    CHECK("bounded_catchup_four",saf_clock_due(&timer,10040)==4&&timer.dropped==246);saf_clock_pause(&timer);CHECK("pause_clock_discards_backlog",!saf_clock_due(&timer,20040));
    timer=(saf_clock_t){0};saf_clock_due(&timer,UINT32_MAX-15);CHECK("clock_unsigned_wrap",saf_clock_due(&timer,24)==1);
    CHECK("buttons_release",!saf_io.pointer&&!saf_io.controller&&!saf_io.pending&&!SAF_buttonPressed(5));
    printf("FINAL entry%u %uchecks wire%u frame%u fault%u\n",SAF_ENTRY_ID,checks,bytes,_SAF_frame,saf_io.fault);return 0;
}
