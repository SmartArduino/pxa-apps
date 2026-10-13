void play_state_sounds(void) {
    if (g_game.state == J3_STATE_CHARGING && g_last_state != J3_STATE_CHARGING) {
        /* The original starts both the swell and the loop on touch down. */
        j3_audio_play(&g_audio, J3_CHANNEL_CHARGE, J3_CLIP_SCALE_INTRO,
                      J3_GAIN_FULL, 0);
        j3_audio_play(&g_audio, J3_CHANNEL_SUSTAIN, J3_CLIP_SCALE_LOOP,
                      J3_GAIN_SOFT, 1);
    }
    if (g_last_state == J3_STATE_CHARGING && g_game.state != J3_STATE_CHARGING) {
        j3_audio_stop(&g_audio, J3_CHANNEL_CHARGE);
        j3_audio_stop(&g_audio, J3_CHANNEL_SUSTAIN);
    }
    if (g_game.jump_count != g_last_jumps) {
        j3_audio_play(&g_audio, J3_CHANNEL_LAND, J3_CLIP_SUCCESS, J3_GAIN_FULL,
                      0);
        j3_audio_play(&g_audio, J3_CHANNEL_POP, J3_CLIP_POP, J3_GAIN_SOFT, 0);
        if (g_game.combo > 0u) {
            const uint8_t note = (uint8_t)(J3_CLIP_COMBO1 +
                                           ((g_game.combo - 1u) % 8u));
            j3_audio_play(&g_audio, J3_CHANNEL_COMBO, note, J3_GAIN_LOUD, 0);
        } else {
            j3_audio_stop(&g_audio, J3_CHANNEL_COMBO);
        }
        g_bonus_repeat_timer = 3.0F;
    }
    if (g_game.bonus_popup_timer > 0.0F && g_bonus_played == 0u) {
        g_bonus_played = 1;
        g_bonus_repeat_timer = 3.0F;
        switch (g_game.bonus_kind) {
        case J3_KIND_MUSIC:
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_SING,
                          J3_GAIN_FULL, 0);
            break;
        case J3_KIND_STORE:
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_STORE,
                          J3_GAIN_FULL, 0);
            break;
        case J3_KIND_WELL:
            /* The original stops every other music voice for the manhole. */
            j3_audio_stop(&g_audio, J3_CHANNEL_COMBO);
            j3_audio_stop(&g_audio, J3_CHANNEL_POP);
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_WATER,
                          J3_GAIN_FULL, 0);
            break;
        default:
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_POP,
                          J3_GAIN_FULL, 0);
            break;
        }
    }
    if (g_bonus_repeat_timer > 0.0F) {
        g_bonus_repeat_timer -= (float)TICK_MS * 0.001F;
        if (g_bonus_repeat_timer <= 0.0F && g_bonus_played != 0u &&
            g_game.state == J3_STATE_READY && g_game.bonus_popup_timer <= 0.0F &&
            j3_block_bonus(&g_game.blocks[0]) != 0u &&
            j3_audio_channel_active(&g_audio, J3_CHANNEL_BONUS) == 0) {
            /* Keep the melody going while the player stays, like the
             * original's onEnded timers. */
            g_bonus_repeat_timer = 3.0F;
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_SING,
                          J3_GAIN_SOFT, 0);
        }
    }
    if ((g_game.state == J3_STATE_TIP || g_game.state == J3_STATE_FALL) &&
        g_last_state == J3_STATE_FLYING) {
        j3_audio_play(&g_audio, J3_CHANNEL_LAND,
                      g_game.land_result == J3_LAND_MISS ? J3_CLIP_FALL_2
                                                         : J3_CLIP_FALL,
                      J3_GAIN_FULL, 0);
    }
    if (g_game.bonus_popup_timer <= 0.0F) g_bonus_played = 0;
    g_last_state = g_game.state;
    g_last_score = g_game.score;
    g_last_jumps = g_game.jump_count;
#if J3_BGM_MODE != 0
    if (g_audio.state == J3_AUDIO_READY && !g_bgm_started) {
        g_bgm_started = 1;
        j3_audio_play(&g_audio, J3_CHANNEL_BGM, J3_CLIP_ICON, J3_GAIN_BGM,
                      (uint8_t)J3_BGM_LOOP);
    }
#endif
}

