static void reset_game_for_level(uint8_t level_index){
    const level_def_t* lvl=&levels[level_index];
    sun_count=lvl->sun_initial;
    defeated=0;
    zombies_spawned=0;
    spawn_timer=0;
    sky_sun_timer=0;
    selected_plant=PLANT_EMPTY;
    if(chosen_count>0) selected_plant=chosen_plants[0];
    shovel_mode=0;
    seed_scroll=0;
    seed_drag_active=0;
    seed_dragged=0;
    win_reward_plant=0;
    state=STATE_READY;
    last_tick_us=0;
    game_remainder_us=0;
    render_ticks=0;
    last_empty_tap_us=0;
    for(uint8_t t=0;t<PLANT_TYPE_COUNT;++t) plant_cooldowns[t]=0;
    for(int r=0;r<BOARD_ROWS;++r){
        mower_state[r]=MOWER_READY;
        mower_x[r]=MOWER_HOME_X;
        for(int c=0;c<BOARD_COLS;++c){
            plants[r][c].type=PLANT_EMPTY;
            plants[r][c].health=0;
            plants[r][c].timer=0;
            plants[r][c].stage=0;
            plants[r][c].grow_timer=0;
            crater_timer[r][c]=0;
        }
    }
    for(int i=0;i<MAX_PROJECTILES;++i) projectiles[i].active=0;
    for(int i=0;i<MAX_ZOMBIES;++i) zombies[i].active=0;
    for(int i=0;i<MAX_SUNS;++i) suns[i].active=0;
    for(int i=0;i<MAX_BLASTS;++i) blasts[i].active=0;
    if(chosen_count>0){
        for(uint8_t i=0;i<chosen_count;++i){
            uint8_t pt=chosen_plants[i];
            if(pt<PLANT_TYPE_COUNT) plant_cooldowns[pt]=0;
        }
    }
}
static int has_zombie_ahead(int row,int col,uint8_t plant_type){
    int16_t plant_x=cell_x(col);
    int16_t range=300;
    if(plant_type==PLANT_PUFF) range=PUFF_RANGE_PX;
    else if(plant_type==PLANT_FUME){
        int16_t left=fume_start_x(col);
        int16_t right=fume_end_x(col);
        for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row==row && zombies[i].x+24>left && zombies[i].x<right) return 1;
        return 0;
    }
    else if(plant_type==PLANT_MAGNET) range=MAGNET_RANGE_PX;
    else if(plant_type==PLANT_GLOOM) range=GLOOM_RANGE_PX;
    else if(plant_type==PLANT_SPIKE) return 0;
    else if(plant_type==PLANT_DOOM) return 0;
    for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row==row){
        int16_t dx=zombies[i].x - plant_x;
        if(dx>0 && dx<range) return 1;
    }
    return 0;
}
static void spawn_projectile(int row,int col,int16_t offset,uint8_t kind){
    for(int i=0;i<MAX_PROJECTILES;++i) if(!projectiles[i].active){
        projectiles[i].active=1;
        projectiles[i].row=(uint8_t)row;
        projectiles[i].x=(int16_t)(cell_x(col)+offset);
        projectiles[i].kind=kind;
        play_sfx(PXA_GAME_SFX_FIRE);
        return;
    }
}
static void spawn_sun(int16_t x,int16_t y,int16_t target_y,uint8_t value){
    for(int i=0;i<MAX_SUNS;++i) if(!suns[i].active){
        suns[i].active=1;
        suns[i].x=x;
        suns[i].y=y;
        suns[i].target_y=target_y;
        suns[i].lifetime=SUN_LIFETIME_TICKS;
        suns[i].value=value;
        return;
    }
}
static void defeat_zombie(zombie_t* zombie){
    if(!zombie->active) return;
    zombie->active=0;
    const level_def_t* lvl=current_level_def();
    if(defeated < lvl->wave_goal) ++defeated;
    play_sfx(PXA_GAME_SFX_DEATH);
    play_sfx(PXA_GAME_SFX_SCORE);
}
static void start_blast(int row,int col){
    for(int i=0;i<MAX_BLASTS;++i) if(!blasts[i].active){
        blasts[i].active=1;blasts[i].row=(uint8_t)row;blasts[i].col=(uint8_t)col;blasts[i].timer=BOMB_BLAST_TICKS;return;
    }
    blasts[0].active=1;blasts[0].row=(uint8_t)row;blasts[0].col=(uint8_t)col;blasts[0].timer=BOMB_BLAST_TICKS;
}
static void explode_bomb(int row,int col){
    int first_row=row>0?row-1:0;
    int last_row=row+1<BOARD_ROWS?row+1:BOARD_ROWS-1;
    int first_col=col>0?col-1:0;
    int last_col=col+1<BOARD_COLS?col+1:BOARD_COLS-1;
    int16_t left=cell_x(first_col);
    int16_t right=(int16_t)(cell_x(last_col)+CELL_W);
    start_blast(row,col);
    for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row>=first_row && zombies[i].row<=last_row && zombies[i].x+24>left && zombies[i].x<right) defeat_zombie(&zombies[i]);
    plants[row][col].type=PLANT_EMPTY;
    plants[row][col].health=0;
    plants[row][col].timer=0;
    plants[row][col].stage=0;
    plants[row][col].grow_timer=0;
    play_sfx(PXA_GAME_SFX_EXPLODE);
}
static void update_plants(void){
    for(int row=0;row<BOARD_ROWS;++row) for(int col=0;col<BOARD_COLS;++col){
        plant_t* plant=&plants[row][col];
        if(plant->type==PLANT_EMPTY) continue;
        if(plant->type==PLANT_SHOOTER){
            if(++plant->timer>=SHOOTER_FIRE_TICKS){
                plant->timer=0;
                if(has_zombie_ahead(row,col,PLANT_SHOOTER)) spawn_projectile(row,col,24,PROJECTILE_PEA);
            }
        } else if(plant->type==PLANT_REPEATER){
            if(++plant->timer>=REPEATER_FIRE_TICKS){
                plant->timer=0;
                if(has_zombie_ahead(row,col,PLANT_REPEATER)){ spawn_projectile(row,col,24,PROJECTILE_PEA); spawn_projectile(row,col,14,PROJECTILE_PEA); }
            }
        } else if(plant->type==PLANT_ICE){
            if(++plant->timer>=ICE_FIRE_TICKS){
                plant->timer=0;
                if(has_zombie_ahead(row,col,PLANT_ICE)) spawn_projectile(row,col,24,PROJECTILE_ICE_PEA);
            }
        } else if(plant->type==PLANT_PUFF){
            if(plant->stage>0) --plant->stage;
            if(++plant->timer>=PUFF_FIRE_TICKS){
                plant->timer=0;
                if(has_zombie_ahead(row,col,PLANT_PUFF)){
                    spawn_projectile(row,col,20,PROJECTILE_SPORE);
                    plant->stage=PUFF_FLASH_TICKS;
                }
            }
        } else if(plant->type==PLANT_FUME){
            if(++plant->timer>=FUME_FIRE_TICKS){
                plant->timer=0;
                if(has_zombie_ahead(row,col,PLANT_FUME)){
                    int16_t left=fume_start_x(col);
                    int16_t right=fume_end_x(col);
                    for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row==row && zombies[i].x+24>left && zombies[i].x<right){
                        zombies[i].slow_timer=0;
                        if(zombies[i].health>1) { --zombies[i].health; play_sfx(PXA_GAME_SFX_HIT); } else defeat_zombie(&zombies[i]);
                    }
                    play_sfx(PXA_GAME_SFX_FIRE);
                }
            }
        } else if(plant->type==PLANT_SUNFLOWER){
            if(++plant->timer>=SUNFLOWER_PRODUCE_TICKS){
                plant->timer=0;
                spawn_sun((int16_t)(cell_x(col)+CELL_W/2),(int16_t)(cell_y(row)+11),(int16_t)(cell_y(row)+11),25);
            }
        } else if(plant->type==PLANT_SUNSHROOM){
            if(plant->stage<2){
                if(++plant->grow_timer>=SUNSHROOM_GROW_TICKS){
                    plant->grow_timer=0;
                    plant->stage++;
                }
            }
            if(++plant->timer>=SUNSHROOM_PRODUCE_TICKS){
                plant->timer=0;
                uint8_t v= plant->stage>=2 ? 25 : 15;
                spawn_sun((int16_t)(cell_x(col)+CELL_W/2),(int16_t)(cell_y(row)+14),(int16_t)(cell_y(row)+14),v);
            }
        } else if(plant->type==PLANT_SPIKE){
            if(++plant->timer>=SPIKE_DAMAGE_TICKS){
                plant->timer=0;
                int16_t left=cell_x(col);
                int16_t right=left+CELL_W;
                for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row==row && zombies[i].x+24>left && zombies[i].x<right){
                    if(zombies[i].health>1){ --zombies[i].health; play_sfx(PXA_GAME_SFX_HIT);} else defeat_zombie(&zombies[i]);
                }
            }
        } else if(plant->type==PLANT_BOMB && ++plant->timer>=BOMB_FUSE_TICKS){
            explode_bomb(row,col);
        } else if(plant->type==PLANT_DOOM && ++plant->timer>=DOOM_FUSE_TICKS){
            // 毁灭菇：3x3 大爆炸并留下弹坑
            int first_row=row>0?row-1:0;
            int last_row=row+1<BOARD_ROWS?row+1:BOARD_ROWS-1;
            int first_col=col>0?col-1:0;
            int last_col=col+1<BOARD_COLS?col+1:BOARD_COLS-1;
            int16_t left=cell_x(first_col);
            int16_t right=(int16_t)(cell_x(last_col)+CELL_W);
            start_blast(row,col);
            // 额外扩大一圈用于特效（与普通樱桃一致，但伤害范围相同；毁灭菇伤害更高，秒杀）
            for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row>=first_row && zombies[i].row<=last_row && zombies[i].x+24>left && zombies[i].x<right) {
                // 毁灭菇秒杀，巨人需 2 次
                if(zombies[i].type==ZOMBIE_GARG && zombies[i].health>8) { zombies[i].health-=8; play_sfx(PXA_GAME_SFX_HIT);} else defeat_zombie(&zombies[i]);
            }
            plants[row][col].type=PLANT_EMPTY;
            plants[row][col].health=0;
            plants[row][col].timer=0;
            plants[row][col].stage=0;
            plants[row][col].grow_timer=0;
            crater_timer[row][col]=DOOM_CRATER_TICKS;
            play_sfx(PXA_GAME_SFX_EXPLODE);
        } else if(plant->type==PLANT_MAGNET && ++plant->timer>=MAGNET_PULL_TICKS){
            plant->timer=0;
            // 磁力菇：吸取同行前方 3 格内一个铁桶/路障
            int16_t my_x=cell_x(col);
            for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row==row){
                int16_t dx=zombies[i].x - my_x;
                if(dx>0 && dx < MAGNET_RANGE_PX + CELL_W && (zombies[i].type==ZOMBIE_BUCKET || zombies[i].type==ZOMBIE_CONE)){
                    // 降级为普通，血量重算为普通剩余
                    uint8_t old_health=zombies[i].health;
                    zombies[i].type=ZOMBIE_REGULAR;
                    // 保留部分血量：按比例或固定 4
                    zombies[i].health = old_health>6 ? 4 : old_health;
                    if(zombies[i].health==0) zombies[i].health=4;
                    zombies[i].slow_timer=ICE_SLOW_TICKS/2;
                    play_sfx(PXA_GAME_SFX_ACTION);
                    break;
                }
            }
        } else if(plant->type==PLANT_GLOOM && ++plant->timer>=GLOOM_DAMAGE_TICKS){
            plant->timer=0;
            int first_row=row>0?row-1:0;
            int last_row=row+1<BOARD_ROWS?row+1:BOARD_ROWS-1;
            int16_t left=cell_x(col)-CELL_W;
            int16_t right=cell_x(col)+CELL_W*2;
            if(left<BOARD_X) left=BOARD_X;
            for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row>=first_row && zombies[i].row<=last_row && zombies[i].x+24>left && zombies[i].x<right){
                if(zombies[i].health>1){ --zombies[i].health; play_sfx(PXA_GAME_SFX_HIT);} else defeat_zombie(&zombies[i]);
                // 同时减速
                zombies[i].slow_timer=30;
            }
        }
    }
}
static void update_craters(void){
    for(int r=0;r<BOARD_ROWS;++r) for(int c=0;c<BOARD_COLS;++c) if(crater_timer[r][c]>0) --crater_timer[r][c];
}
static void update_blasts(void){
    for(int i=0;i<MAX_BLASTS;++i) if(blasts[i].active && --blasts[i].timer==0) blasts[i].active=0;
}
static void update_suns(void){
    for(int i=0;i<MAX_SUNS;++i){
        sun_t* sun=&suns[i];
        if(!sun->active) continue;
        if(sun->y < sun->target_y){
            sun->y=(int16_t)(sun->y+3);
            if(sun->y>sun->target_y) sun->y=sun->target_y;
        } else if(sun->lifetime--==0) sun->active=0;
    }
}
static void update_projectiles(void){
    for(int projectile_index=0;projectile_index<MAX_PROJECTILES;++projectile_index){
        projectile_t* projectile=&projectiles[projectile_index];
        if(!projectile->active) continue;
        projectile->x=(int16_t)(projectile->x+4);
        if(projectile->x>298){ projectile->active=0; continue; }
        for(int zombie_index=0;zombie_index<MAX_ZOMBIES;++zombie_index){
            zombie_t* zombie=&zombies[zombie_index];
            if(!zombie->active || zombie->row!=projectile->row) continue;
            if(projectile->x+4>=zombie->x && projectile->x<=zombie->x+24){
                projectile->active=0;
                if(projectile->kind==PROJECTILE_ICE_PEA) zombie->slow_timer=ICE_SLOW_TICKS;
                if(--zombie->health==0) defeat_zombie(zombie);
                else play_sfx(PXA_GAME_SFX_HIT);
                break;
            }
        }
    }
}
static int blocking_plant(const zombie_t* zombie){
    for(int col=BOARD_COLS-1;col>=0;--col){
        const plant_t* plant=&plants[zombie->row][col];
        if(plant->type==PLANT_EMPTY) continue;
        if(plant->type==PLANT_SPIKE) continue;
        int16_t x=cell_x(col);
        if(zombie->x <= x+CELL_W-3 && zombie->x+24 >= x+3) return col;
    }
    return -1;
}
static void update_mowers(void){
    for(int row=0;row<BOARD_ROWS;++row){
        if(mower_state[row]!=MOWER_ACTIVE) continue;
        mower_x[row]=(int16_t)(mower_x[row]+7);
        for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active && zombies[i].row==row && zombies[i].x < mower_x[row]+25 && zombies[i].x+24 > mower_x[row]) defeat_zombie(&zombies[i]);
        if(mower_x[row]>304) mower_state[row]=MOWER_SPENT;
    }
}
static void update_zombies(void){
    update_mowers();
    for(int i=0;i<MAX_ZOMBIES;++i){
        zombie_t* zombie=&zombies[i];
        if(!zombie->active) continue;
        if(zombie->slow_timer!=0) --zombie->slow_timer;
        int col=blocking_plant(zombie);
        // 撑杆跳：首次遇到植物时直接跳过
        if(col>=0 && zombie->type==ZOMBIE_POLE && zombie->has_pole){
            zombie->has_pole=0;
            zombie->x = (int16_t)(cell_x(col) + CELL_W + 4);
            zombie->walk_timer=0;
            zombie->bite_timer=0;
            play_sfx(PXA_GAME_SFX_ACTION);
            col=-1;
        }
        if(col>=0){
            zombie->walk_timer=0;
            uint8_t bite_ticks = (zombie->type==ZOMBIE_GARG) ? (ZOMBIE_BITE_TICKS/2) : ZOMBIE_BITE_TICKS;
            if(++zombie->bite_timer>=bite_ticks){
                plant_t* plant=&plants[zombie->row][col];
                zombie->bite_timer=0;
                uint8_t dmg = (zombie->type==ZOMBIE_GARG) ? 2 : 1;
                if(plant->health> dmg) plant->health-=dmg; else plant->health=0;
                play_sfx(PXA_GAME_SFX_BITE);
                if(plant->health==0){
                    plant->type=PLANT_EMPTY;
                    plant->health=0;
                    plant->timer=0;
                    plant->stage=0;
                    plant->grow_timer=0;
                }
            }
        } else {
            zombie->bite_timer=0;
            uint8_t base_ticks = ZOMBIE_MOVE_TICKS;
            if(zombie->type==ZOMBIE_GARG) base_ticks = ZOMBIE_MOVE_TICKS*2;
            else if(zombie->type==ZOMBIE_POLE && zombie->has_pole) base_ticks = ZOMBIE_MOVE_TICKS/2 ? ZOMBIE_MOVE_TICKS/2 : 2;
            uint8_t move_ticks=zombie->slow_timer!=0 ? (uint8_t)(base_ticks*2u): base_ticks;
            if(move_ticks<2) move_ticks=2;
            if(++zombie->walk_timer>=move_ticks){
                zombie->walk_timer=0;
                --zombie->x;
            }
        }
        if(zombie->x <= BOARD_X-5){
            if(mower_state[zombie->row]==MOWER_READY){
                mower_state[zombie->row]=MOWER_ACTIVE;
                mower_x[zombie->row]=MOWER_HOME_X;
                play_sfx(PXA_GAME_SFX_ACTION);
            } else if(mower_state[zombie->row]==MOWER_SPENT){
                state=STATE_LOSS;
                play_sfx(PXA_GAME_SFX_LOSE);
            }
        }
    }
}
static void spawn_zombie(void){
    for(int i=0;i<MAX_ZOMBIES;++i) if(!zombies[i].active){
        uint32_t roll=random_next();
        const level_def_t* lvl=current_level_def();
        uint8_t wave = zombies_spawned;
        uint8_t type;
        // 后期加入撑杆与巨人，增加夜间难度
        if(wave>=25 && roll%5u==0) type=ZOMBIE_GARG;
        else if(wave>=18 && roll%4u==0) type=ZOMBIE_POLE;
        else if(wave>=20 && roll%4u==0) type=ZOMBIE_BUCKET;
        else if(wave>=12 && roll%3u==0) type=ZOMBIE_BUCKET;
        else if(wave>=6 && roll%3u==0) type=ZOMBIE_CONE;
        else if(wave>=3 && roll%4u==1) type=ZOMBIE_CONE;
        else type=ZOMBIE_REGULAR;
        if(lvl->scene==SCENE_NIGHT && wave>=10 && roll%5u==0) type=ZOMBIE_BUCKET;
        if(lvl->scene==SCENE_NIGHT && wave>=15 && roll%6u==0) type=ZOMBIE_POLE;
        if(wave>=22 && roll%7u==0) type=ZOMBIE_GARG;
        zombies[i].active=1;
        zombies[i].row=(uint8_t)(random_next()%BOARD_ROWS);
        zombies[i].x=GARDEN_W;
        zombies[i].type=type;
        zombies[i].health=zombie_health(type);
        if(current_level>=6) {
            uint8_t extra = (uint8_t)(current_level/3);
            zombies[i].health = (uint8_t)(zombies[i].health + extra);
            if(zombies[i].health>14) zombies[i].health=14;
        }
        zombies[i].bite_timer=0;
        zombies[i].walk_timer=0;
        zombies[i].slow_timer=0;
        zombies[i].has_pole = (type==ZOMBIE_POLE)?1:0;
        // 巨人额外血量
        if(type==ZOMBIE_GARG){
            if(zombies[i].health<18) zombies[i].health=18;
            if(zombies[i].health>22) zombies[i].health=22;
        }
        ++zombies_spawned;
        return;
    }
}
static int any_zombies(void){
    for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active) return 1;
    return 0;
}
static int any_blasts(void){
    for(int i=0;i<MAX_BLASTS;++i) if(blasts[i].active) return 1;
    return 0;
}
static void update_wave(void){
    const level_def_t* lvl=current_level_def();
    if(zombies_spawned < lvl->wave_goal){
        uint16_t spawn_period = zombies_spawned==0 ? lvl->first_spawn : (uint16_t)(lvl->spawn_period - (zombies_spawned/4u)*10u);
        if(spawn_period<80) spawn_period=80;
        if(++spawn_timer>=spawn_period){
            spawn_timer=0;
            spawn_zombie();
        }
    } else if(defeated==lvl->wave_goal && !any_zombies() && !any_blasts()){
        state=STATE_WIN;
        play_sfx(PXA_GAME_SFX_WIN);
        uint8_t stars=3;
        uint8_t mowers_lost=0;
        for(int r=0;r<BOARD_ROWS;++r) if(mower_state[r]==MOWER_SPENT) mowers_lost++;
        if(mowers_lost>=2) stars=1;
        else if(mowers_lost==1) stars=2;
        else if(sun_count<50) stars=2;
        if(level_stars[current_level]<stars) level_stars[current_level]=stars;
        win_reward_plant=0;
        if(current_level+1==max_unlocked && max_unlocked<LEVELS_COUNT){
            /* 首次通关：解锁下一关 + 发放本关植物奖励 */
            max_unlocked++;
            apply_level_unlock(current_level);
            win_reward_plant=current_level_def()->unlock_plant;
            rebuild_unlocks_from_progress();
        } else {
            apply_level_unlock(current_level);
        }
        garden_save();
    }
}
