/* SPDX-License-Identifier: MIT. Backend/clock/known safety edges, not replacement rules. */
#include <assert.h>
#include <stdio.h>
#include "core.h"
#include "scheduler.h"
static unsigned checks;
#define CHECK(name,condition) do { assert(condition);printf("PASS %s\n",name);checks++; } while(0)
static void no_rule_step(void) {}
int main(void) {
    uint32_t hash;
    saf_boot();CHECK("official_selected_entry",saf_current_game()==SAF_GAME_ID&&!saf_io.fault);
    CHECK("full_original_rgb_primary",saf_rgb565(0)==0&&saf_rgb565(255)==65535&&saf_rgb565(224)==63488&&saf_rgb565(28)==2016&&saf_rgb565(3)==31);
    for(unsigned i=0;i<256;i++){uint8_t r,g,b;SAF_colorToRGB(i,&r,&g,&b);assert(saf_rgb565(i)==((r>>3)<<11| (g>>2)<<5 | (b>>3)));}
    CHECK("all256_original_palette",1);
    stepFunction=no_rule_step;saf_io.sampled=127;
    for(unsigned frame=1;frame<=260;frame++){SAF_FE_loop();for(unsigned i=0;i<7;i++)assert(SAF_buttonPressed(i)==(frame<255?frame:255));}
    CHECK("seven_original_held_counts_saturate",_SAF_frame==260);
    saf_io.sampled=0;SAF_FE_loop();CHECK("release_all_counts_zero",saf_io.sampled==0&&SAF_buttonPressed(0)==0);saf_select_game(SAF_GAME_ID);
    saf_pointer_mask(16,0);saf_pointer_mask(0,1);saf_frame();CHECK("short_tap_latched_one_original_edge",SAF_buttonJustPressed(4)&&saf_io.pending==0);saf_frame();CHECK("short_tap_released_next_frame",SAF_buttonPressed(4)==0);
    saf_pointer_mask(8,0);saf_release_all();saf_frame();CHECK("cancel_discards_pending_and_held",saf_io.sampled==0&&SAF_buttonPressed(3)==0);
    saf_controller_mask(32);saf_pointer_mask(1,0);saf_frame();CHECK("controller_and_pointer_combine",saf_io.sampled==33&&SAF_buttonJustPressed(0)&&SAF_buttonJustPressed(5));
    saf_clock_t clock={0};assert(saf_clock_due(&clock,0)==0);assert(saf_clock_due(&clock,39)==0);assert(saf_clock_due(&clock,40)==1);
    CHECK("exact_40ms_virtual_frames",saf_clock_due(&clock,80)==1);
    CHECK("bounded_four_frame_catchup",saf_clock_due(&clock,10080)==4&&clock.dropped==246);
    saf_clock_pause(&clock);CHECK("pause_resume_no_stale_catchup",saf_clock_due(&clock,20080)==0);
    clock=(saf_clock_t){0};saf_clock_due(&clock,UINT32_MAX-15);CHECK("unsigned_clock_wrap",saf_clock_due(&clock,24)==1);
    saf_boot();SAF_save(7,42);uint32_t writes=saf_io.save_writes;SAF_save(7,42);SAF_save(32,99);
    CHECK("32_session_save_cached_and_bounded",SAF_load(7)==42&&SAF_load(32)==0&&saf_io.save_writes==writes);
    saf_select_game(4);CHECK("original_new_retains_session_save",SAF_load(7)==42);
    saf_boot();saf_pointer_mask(32,0);for(unsigned i=0;i<17;i++)assert(saf_frame());CHECK("original_B18_requests_wrapper_pause",!saf_frame()&&saf_io.menu_requested&&saf_current_game()==5&&saf_io.sampled==0);
    hash=saf_hash(memory,512);uint32_t frame=_SAF_frame;uint8_t rng=_SAF_currentRandom;saf_resume_selected();
    CHECK("continue_restores_step_without_reset",saf_current_game()==SAF_GAME_ID&&!saf_io.menu_requested&&saf_hash(memory,512)==hash&&_SAF_frame==frame&&_SAF_currentRandom==rng);
    saf_boot();for(unsigned i=0;i<928;i++)setMemoryHalfByte(i,SNAKE_SQUARE_SR);SNAKE_SCORE=50;snakeSpawnFood();
    CHECK("full_board_food_terminal_bounded",SNAKE_FOOD_SQUARE==-1&&SNAKE_GAME_STATE==2&&getHiScore(0)==50&&!saf_io.fault);
    saf_boot();blocksInit();memset(memory,0,BLOCKS_BOARD_SQUARES);CHECK("empty_active_piece_cannot_fall",blocksFallStep()==0);
    saf_boot();runnerInit();for(unsigned i=0;i<=254;i+=2)memory[i]=1;hash=saf_hash(memory,512);runnerAddObstacle(1);
    CHECK("missing_runner_sentinel_fault_bounded",saf_io.fault==2&&saf_hash(memory,512)==hash);
    saf_boot();runnerInit();for(unsigned i=0;i<252;i+=2){memory[i]=1;memory[i+1]=72;}memory[252]=0;runnerAddObstacle(3);
    CHECK("runner_126_to127_preserves_sentinel",saf_io.fault==0&&memory[252]==3&&memory[253]==72&&memory[254]==0);
    runnerAddObstacle(2);CHECK("runner_capacity_rejects_uint8_wrap",saf_io.fault==2&&memory[0]==1&&memory[254]==0);
    printf("SAF native backend %u/%u PASS\n",checks,checks);return 0;
}
