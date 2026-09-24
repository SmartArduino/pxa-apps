static int hit_sun(int16_t x,int16_t y){
    for(int i=0;i<MAX_SUNS;++i){
        int16_t dx=(int16_t)(x - suns[i].x);
        int16_t dy=(int16_t)(y - suns[i].y);
        if(suns[i].active && dx>=-12 && dx<=12 && dy>=-12 && dy<=12){
            suns[i].active=0;
            sun_count+=suns[i].value;
            last_empty_tap_us=0;
            play_sfx(PXA_GAME_SFX_COLLECT);
            return 1;
        }
    }
    return 0;
}
static int hit_pause_button(int16_t x,int16_t y){
    return x>=PAUSE_X && x<PAUSE_X+PAUSE_W && y>=PAUSE_Y && y<PAUSE_Y+PAUSE_H;
}
static int hit_shovel_button(int16_t x,int16_t y){
    return x>=SHOVEL_X && x<SHOVEL_X+SHOVEL_W && y>=SHOVEL_Y && y<SHOVEL_Y+SHOVEL_H;
}
static uint8_t seed_type_at_chosen(int16_t x,int16_t y){
    int32_t relative=(int32_t)x - SEED_VIEW_X + seed_scroll;
    if(y<SEED_Y || y>=SEED_Y+32 || x<SEED_VIEW_X || x>=SEED_VIEW_X+SEED_VIEW_W || relative<0) return PLANT_EMPTY;
    int32_t idx=relative / SEED_STRIDE;
    if(idx<0 || idx>=chosen_count) return PLANT_EMPTY;
    if(relative % SEED_STRIDE >= SEED_CARD_W) return PLANT_EMPTY;
    return chosen_plants[idx];
}
static void select_seed(uint8_t type){
    shovel_mode=0;
    last_empty_tap_us=0;
    if(type==selected_plant){
        selected_plant=PLANT_EMPTY;
        play_sfx(PXA_GAME_SFX_TAP);
    } else if(plant_cooldowns[type]==0 && sun_count>=plant_cost(type)){
        selected_plant=type;
        play_sfx(PXA_GAME_SFX_TAP);
    }
}
static int begin_seed_gesture(int16_t x,int16_t y){
    uint8_t type=seed_type_at_chosen(x,y);
    if(type==PLANT_EMPTY && (y<SEED_Y || y>=SEED_Y+32 || x<SEED_VIEW_X || x>=SEED_VIEW_X+SEED_VIEW_W)) return 0;
    last_empty_tap_us=0;
    seed_drag_active=1;
    seed_dragged=0;
    seed_drag_x=x;
    seed_drag_scroll=seed_scroll;
    if(type!=PLANT_EMPTY) select_seed(type);
    return 1;
}
static int update_seed_gesture(int16_t x){
    int16_t delta=(int16_t)(seed_drag_x - x);
    if(!seed_drag_active) return 0;
    if(!seed_dragged && delta>-SEED_DRAG_THRESHOLD && delta<SEED_DRAG_THRESHOLD) return 1;
    seed_dragged=1;
    selected_plant=PLANT_EMPTY;
    seed_scroll=seed_drag_scroll;
    scroll_seed_bar(delta);
    return 1;
}
static int board_cell_at(int16_t x,int16_t y,int* row,int* col){
    int found_col=(x-BOARD_X)/CELL_W;
    int found_row=(y-BOARD_Y)/CELL_H;
    if(x<BOARD_X||y<BOARD_Y||found_col<0||found_col>=BOARD_COLS||found_row<0||found_row>=BOARD_ROWS) return 0;
    *row=found_row;*col=found_col;return 1;
}
static int collect_all_suns(void){
    uint8_t collected=0;
    uint32_t total=0;
    for(int i=0;i<MAX_SUNS;++i) if(suns[i].active){ total+=suns[i].value; suns[i].active=0; ++collected; }
    if(collected==0) return 0;
    sun_count+=total;
    play_sfx(PXA_GAME_SFX_COLLECT);
    return 1;
}
static int collect_suns_on_double_tap(int16_t x,int16_t y){
    int row,col;
    if(!board_cell_at(x,y,&row,&col) || plants[row][col].type!=PLANT_EMPTY){ last_empty_tap_us=0; return 0; }
    if(last_empty_tap_us!=0 && last_tick_us>=last_empty_tap_us && last_tick_us-last_empty_tap_us<=DOUBLE_TAP_WINDOW_US){
        last_empty_tap_us=0;
        return collect_all_suns();
    }
    last_empty_tap_us=last_tick_us;
    return 0;
}
static int remove_plant(int16_t x,int16_t y){
    int row,col;
    if(!board_cell_at(x,y,&row,&col)||plants[row][col].type==PLANT_EMPTY) return 0;
    plants[row][col].type=PLANT_EMPTY;
    plants[row][col].health=0;
    plants[row][col].timer=0;
    plants[row][col].stage=0;
    plants[row][col].grow_timer=0;
    play_sfx(PXA_GAME_SFX_ACTION);
    return 1;
}
static int place_plant(int16_t x,int16_t y){
    int row,col;
    if(!board_cell_at(x,y,&row,&col)) return 0;
    if(selected_plant==PLANT_EMPTY) return 0;
    if(crater_timer[row][col]>0) { play_sfx(PXA_GAME_SFX_ALERT); return 0; }
    plant_t* plant=&plants[row][col];
    uint8_t type=selected_plant;
    if(type>=PLANT_TYPE_COUNT) return 0;
    uint8_t cost=plant_cost(type);
    if(plant->type==PLANT_EMPTY && plant_cooldowns[type]==0 && sun_count>=cost){
        plant->type=type;
        plant->health=plant_health(type);
        if(type==PLANT_SUNFLOWER) plant->timer=SUNFLOWER_PRODUCE_TICKS - SUNFLOWER_FIRST_TICKS;
        else if(type==PLANT_SUNSHROOM){ plant->timer=SUNSHROOM_PRODUCE_TICKS - SUNSHROOM_FIRST_TICKS; plant->stage=0; plant->grow_timer=0; }
        else plant->timer=0;
        plant_cooldowns[type]=plant_recharge_ticks(type);
        sun_count-=cost;
        selected_plant=PLANT_EMPTY;
        play_sfx(PXA_GAME_SFX_PLACE);
        return 1;
    }
    return 0;
}
static void update_cooldowns(void){
    for(uint8_t type=1;type<PLANT_TYPE_COUNT;++type) if(plant_cooldowns[type]!=0) --plant_cooldowns[type];
}
static int tick_step(void){
    if(state!=STATE_PLAYING && state!=STATE_READY) return 1;
    if(state==STATE_READY) return 1;
    update_cooldowns();
    update_craters();
    update_blasts();
    update_suns();
    const level_def_t* lvl=current_level_def();
    if(!is_night()){
        if(++sky_sun_timer>= (lvl->sky_ticks?lvl->sky_ticks:SKY_SUN_TICKS)){
            sky_sun_timer=0;
            spawn_sun((int16_t)(18+random_next()%260u),-9,(int16_t)(62+random_next()%136u),25);
        }
    }
    update_plants();
    update_projectiles();
    update_zombies();
    if(state==STATE_PLAYING) update_wave();
    return 1;
}
static uint8_t game_clock_steps(const pxa_canvas_event_t* event){
    uint64_t timestamp_us;
    uint64_t elapsed_us;
    uint8_t steps;
    if(!pxa_clock_tick_timestamp_us(event,&timestamp_us)) return 0;
    if(last_tick_us==0){ last_tick_us=timestamp_us; return 1; }
    if(timestamp_us<=last_tick_us) return 0;
    elapsed_us=timestamp_us - last_tick_us;
    last_tick_us=timestamp_us;
    if(elapsed_us> GAME_TICK_MS*1000u*GAME_MAX_CATCHUP_STEPS) elapsed_us= GAME_TICK_MS*1000u*GAME_MAX_CATCHUP_STEPS;
    game_remainder_us+=(uint32_t)elapsed_us;
    steps=(uint8_t)(game_remainder_us/(GAME_TICK_MS*1000u));
    if(steps> GAME_MAX_CATCHUP_STEPS) steps=GAME_MAX_CATCHUP_STEPS;
    game_remainder_us-=(uint32_t)steps*GAME_TICK_MS*1000u;
    return steps;
}
static int tick(uint8_t steps){
    if(steps==0 || (state!=STATE_PLAYING && state!=STATE_READY)) return 1;
    if(state==STATE_READY) return 1;
    int needs_render=0;
    while(steps--!=0){
        uint8_t previous_state=state;
        if(!tick_step()) return 0;
        if(++render_ticks>=RENDER_EVERY_TICKS || state!=previous_state){ render_ticks=0; needs_render=1; }
        if(state!=STATE_PLAYING) break;
    }
    return !needs_render || render();
}
static int hit_home_start(int16_t x,int16_t y){ return x>=HOME_X && x<HOME_X+220 && y>=122 && y<156; }
static int hit_home_levels(int16_t x,int16_t y){ return x>=HOME_X && x<HOME_X+106 && y>=166 && y<196; }
static int hit_home_almanac(int16_t x,int16_t y){ return x>=HOME_X+114 && x<HOME_X+220 && y>=166 && y<196; }
static int hit_back(int16_t x,int16_t y){ return x>=16 && x<56 && y>=4 && y<38; }
static int hit_level_button(int16_t x,int16_t y,int* idx){
    for(uint8_t i=0;i<LEVELS_COUNT;++i){
        int16_t bx=(int16_t)(LEVEL_CARD_X+(i%2)*LEVEL_CARD_STRIDE_X);
        int16_t by=(int16_t)(LEVEL_GRID_Y+(i/2)*LEVEL_CARD_STRIDE_Y-level_scroll);
        if(x>=bx && x<bx+LEVEL_CARD_W && y>=by && y<by+LEVEL_CARD_H){ *idx=i; return 1; }
    }
    return 0;
}
static int hit_seed_chosen(int16_t x,int16_t y,int* slot){
    const level_def_t* lvl=current_level_def();
    for(uint8_t i=0;i<lvl->slots;++i){
        int16_t bx=(int16_t)(SEED_OX+16+i*38);
        int16_t by=48;
        if(x>=bx && x<bx+34 && y>=by && y<by+34){ *slot=i; return 1; }
    }
    return 0;
}
static int hit_seed_avail(int16_t x,int16_t y,uint8_t* plant_out){
    uint8_t avail[PLANT_TYPE_COUNT]; uint8_t cnt=0;
    for(uint8_t t=1;t<PLANT_TYPE_COUNT;++t) if(is_plant_unlocked(t)){
        int already=0;
        for(uint8_t c=0;c<chosen_count;++c) if(chosen_plants[c]==t){already=1;break;}
        if(!already) avail[cnt++]=t;
    }
    for(uint8_t i=0;i<cnt;++i){
        uint8_t col=i%7; uint8_t row=i/7;
        int16_t bx=(int16_t)(SEED_OX+12+col*40);
        int16_t by=(int16_t)(102+row*46);
        if(x>=bx && x<bx+36 && y>=by && y<by+40){ *plant_out=avail[i]; return 1; }
    }
    return 0;
}
static int hit_seed_select_button(int16_t x,int16_t y){
    return plant_is_selectable(focused_plant) && x>=SEED_OX+196 && x<SEED_OX+276 && y>=112 && y<148;
}
static int hit_seed_cancel_button(int16_t x,int16_t y){
    return plant_is_selectable(focused_plant) && x>=SEED_OX+196 && x<SEED_OX+276 && y>=160 && y<190;
}
static int hit_seed_start(int16_t x,int16_t y){ return x>=SEED_OX+58 && x<SEED_OX+238 && y>=202 && y<232; }

static int begin_list_gesture(int16_t y){
    uint16_t* scroll=list_scroll_value();
    if(scroll==NULL || y<LIST_VIEW_Y) return 0;
    list_drag_active=1;
    list_dragged=0;
    list_drag_y=y;
    list_drag_scroll=*scroll;
    return 1;
}

static int update_list_gesture(int16_t y){
    int16_t delta;
    if(!list_drag_active) return 0;
    delta=(int16_t)(list_drag_y-y);
    if(!list_dragged && delta>-LIST_DRAG_THRESHOLD && delta<LIST_DRAG_THRESHOLD) return 1;
    list_dragged=1;
    scroll_list_to((int32_t)list_drag_scroll+delta);
    return 1;
}

static void open_level_for_selection(uint8_t level_index){
    current_level=level_index;
    choose_default_plants();
    focused_plant=PLANT_EMPTY;
    state=STATE_SEED_SELECT;
    play_sfx(PXA_GAME_SFX_TAP);
}

static int navigate_back(void){
    list_drag_active=0;
    seed_drag_active=0;
    if(state==STATE_LEVELS || state==STATE_ALMANAC){
        state=STATE_HOME;
    } else if(state==STATE_SEED_SELECT){
        state=STATE_LEVELS;
    } else if(state==STATE_PAUSED){
        state=STATE_PLAYING;
    } else if(state==STATE_PLAYING || state==STATE_READY || state==STATE_WIN || state==STATE_LOSS){
        state=STATE_HOME;
    } else {
        return 0;
    }
    play_sfx(PXA_GAME_SFX_TAP);
    return 1;
}
