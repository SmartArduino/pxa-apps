static void draw_actor(pxa_canvas_frame_t* frame,int16_t x,int16_t y,uint16_t width,uint16_t height,int16_t source_x,int16_t source_y,uint8_t opacity){
    if(!pxa_canvas_clip_push(frame,x,y,width,height)) return;
    pxa_canvas_image(frame,(int16_t)(x-source_x),(int16_t)(y-source_y),ACTOR_ATLAS_W,ACTOR_ATLAS_H,opacity,0,actor_asset,sizeof(actor_asset)-1);
    pxa_canvas_clip_pop(frame);
}

static void draw_body_button_text(pxa_canvas_frame_t* frame, int16_t x, int16_t y,
                                  uint16_t width, uint16_t height, uint32_t color,
                                  const char* text, size_t text_length) {
    pxa_canvas_text_box_role(frame, x, y, width, height,
                             pxa_canvas_rgba(color), PXA_CANVAS_FONT_BODY,
                             PXA_CANVAS_ALIGN_CENTER,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, text, text_length);
}

static void draw_title_button_text(pxa_canvas_frame_t* frame, int16_t x, int16_t y,
                                   uint16_t width, uint16_t height, uint32_t color,
                                   const char* text, size_t text_length) {
    pxa_canvas_text_box_role(frame, x, y, width, height,
                             pxa_canvas_rgba(color), PXA_CANVAS_FONT_TITLE,
                             PXA_CANVAS_ALIGN_CENTER,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, text, text_length);
}

static int16_t plant_actor_x(uint8_t type){
    static const int16_t source_x[]={0,ACTOR_SHOOTER_X,ACTOR_SUNFLOWER_X,ACTOR_WALL_X,ACTOR_BOMB_X,ACTOR_REPEATER_X,ACTOR_ICE_X,ACTOR_PUFF_X,ACTOR_SUNSHROOM_X,ACTOR_SPIKE_X,ACTOR_FUME_X,ACTOR_DOOM_X,ACTOR_MAGNET_X,ACTOR_GLOOM_X};
    if(type>=1 && type<PLANT_TYPE_COUNT) return source_x[type];
    return 0;
}
static int16_t plant_actor_y(uint8_t type){
    return type>PLANT_ICE?ACTOR_EXTRA_Y:ACTOR_PLANT_Y;
}
static void draw_cooldown_cover(pxa_canvas_frame_t* frame,int16_t x,uint16_t height){
    const uint16_t radius=height>4u?4u:height>1u?height-1u:1u;
    const uint32_t color=0x000000A0;
    const int16_t right_corner=(int16_t)(x+SEED_CARD_W-radius);
    pxa_canvas_rect_rgba(frame,(int16_t)(x+radius),SEED_Y,SEED_CARD_W-radius*2u,height,color,0,0,0);
    if(height>radius){
        pxa_canvas_rect_rgba(frame,x,(int16_t)(SEED_Y+radius),radius,height-radius,color,0,0,0);
        pxa_canvas_rect_rgba(frame,right_corner,(int16_t)(SEED_Y+radius),radius,height-radius,color,0,0,0);
    }
    if(pxa_canvas_clip_push(frame,x,SEED_Y,radius,radius)){
        pxa_canvas_ellipse_rgba(frame,x,SEED_Y,radius*2u,radius*2u,color,0,0);
        pxa_canvas_clip_pop(frame);
    }
    if(pxa_canvas_clip_push(frame,right_corner,SEED_Y,radius,radius)){
        pxa_canvas_ellipse_rgba(frame,(int16_t)(x+SEED_CARD_W-radius*2u),SEED_Y,radius*2u,radius*2u,color,0,0);
        pxa_canvas_clip_pop(frame);
    }
}
static void draw_seed(pxa_canvas_frame_t* frame,int16_t x,uint8_t type,uint16_t cooldown){
    const uint32_t available_color=0xCFEFBB;
    const uint32_t unavailable_color=0x465549;
    const uint32_t selected_color=0xA6D66D;
    const uint16_t recharge_ticks=plant_recharge_ticks(type);
    const int can_afford=sun_count>=plant_cost(type);
    const int cooling=cooldown!=0;
    const int available=!cooling && can_afford;
    const int selected=selected_plant==type && available;
    const uint32_t color=selected?selected_color:available?available_color:unavailable_color;
    if(selected) pxa_canvas_rect(frame,(int16_t)(x-1),(int16_t)(SEED_Y-1),46,34,0xF6BE32,5);
    pxa_canvas_rect(frame,x,SEED_Y,SEED_CARD_W,32,color,4);
    if(type>=1 && type<PLANT_TYPE_COUNT){
        draw_actor(frame,(int16_t)(x+7),(int16_t)(SEED_Y+1),30,30,plant_actor_x(type),plant_actor_y(type),can_afford?255:125);
        if(!can_afford) pxa_canvas_rect_rgba(frame,x,SEED_Y,SEED_CARD_W,32,0x00000060,4,0,0);
    }
    // 已移除植物卡片右下角花费数字，避免重叠遮挡
    if(cooling && recharge_ticks!=0){
        uint16_t cover=(uint16_t)(cooldown*32u/recharge_ticks);
        if(cover!=0) draw_cooldown_cover(frame,x,cover);
    }
}
static void draw_seed_bar(pxa_canvas_frame_t* frame){
    if(!pxa_canvas_clip_push(frame,SEED_VIEW_X-3,SEED_Y-3,SEED_VIEW_W+6,38)) return;
    for(uint8_t i=0;i<chosen_count;++i){
        uint8_t type=chosen_plants[i];
        int16_t x=(int16_t)(SEED_X + i*SEED_STRIDE - seed_scroll);
        draw_seed(frame,x,type,plant_cooldowns[type]);
    }
    pxa_canvas_clip_pop(frame);
}
static void draw_plant(pxa_canvas_frame_t* frame,int row,int col){
    const plant_t* plant=&plants[row][col];
    int16_t x=(int16_t)(cell_x(col)+CELL_W/2);
    int16_t y=(int16_t)(cell_y(row)+CELL_H/2);
    draw_actor(frame,(int16_t)(x-15),(int16_t)(y-15),30,30,plant_actor_x(plant->type),plant_actor_y(plant->type),255);
    if(plant->type==PLANT_SUNSHROOM && plant->stage>=2) pxa_canvas_circle(frame,x,(int16_t)(y+2),3,0xFFE27A);
    if((plant->type==PLANT_BOMB || plant->type==PLANT_DOOM) && (plant->timer/2u)%2u==0) pxa_canvas_circle(frame,(int16_t)(x+9),(int16_t)(y-10),3,0xFFF1A1);
    if(plant->type==PLANT_PUFF && plant->stage>0) pxa_canvas_circle(frame,(int16_t)(x+8),(int16_t)(y-2),2,0xD1B3FF);
    if(plant->type==PLANT_FUME && plant->timer<7 && has_zombie_ahead(row,col,PLANT_FUME)){
        int16_t spray_left=fume_start_x(col);
        int16_t spray_y=(int16_t)(cell_y(row)+SPORE_Y_OFFSET);
        if(pxa_canvas_clip_push(frame,spray_left,(int16_t)(spray_y-7),FUME_RANGE_PX,14)){
            pxa_canvas_line_rgba(frame,spray_left,spray_y,(int16_t)(fume_end_x(col)-1),spray_y,0x8D65D060,5);
            for(int16_t offset=4;offset<FUME_RANGE_PX;offset+=16){
                int16_t wobble=(int16_t)(((offset/16+plant->timer)%3)-1);
                pxa_canvas_ellipse_rgba(frame,(int16_t)(spray_left+offset-5),(int16_t)(spray_y-4+wobble),14,8,0xA774E890,0,0);
                pxa_canvas_circle(frame,(int16_t)(spray_left+offset+5),(int16_t)(spray_y-1-wobble),1,0xE2D0FF);
            }
            pxa_canvas_clip_pop(frame);
        }
    }
    if(plant->type==PLANT_MAGNET && plant->timer>MAGNET_PULL_TICKS-6) pxa_canvas_ellipse_rgba(frame,x-8,y-8,16,12,0xFFD70040,0,0);
    if(plant->type==PLANT_GLOOM && plant->timer>GLOOM_DAMAGE_TICKS-8) pxa_canvas_ellipse_rgba(frame,x-10,y-10,20,14,0x9B6BFF30,0,0);
}
static void draw_zombie(pxa_canvas_frame_t* frame,const zombie_t* zombie){
    int16_t y=(int16_t)(cell_y(zombie->row)+CELL_H/2);
    int alpha=zombie->slow_timer?180:255;
    if(zombie->type==ZOMBIE_GARG){
        draw_actor(frame,(int16_t)(zombie->x-10),(int16_t)(y-30),48,48,ACTOR_GARG_X,ACTOR_TALL_ZOMBIE_Y,(uint8_t)alpha);
        if(zombie->slow_timer) pxa_canvas_ellipse_rgba(frame,zombie->x-9,(int16_t)(y-29),46,46,0x7EDBFF30,0,0);
        pxa_canvas_rect(frame,(int16_t)(zombie->x-1),(int16_t)(y-32),30,3,0x3A1E1E,1);
        pxa_canvas_rect(frame,(int16_t)(zombie->x-1),(int16_t)(y-32),(uint16_t)(zombie->health*30/18),3,0xFF4A4A,1);
        return;
    }
    if(zombie->type==ZOMBIE_POLE){
        draw_actor(frame,(int16_t)(zombie->x-7),(int16_t)(y-30),40,48,ACTOR_POLE_ZOMBIE_X,ACTOR_TALL_ZOMBIE_Y,(uint8_t)alpha);
        if(zombie->slow_timer) pxa_canvas_ellipse_rgba(frame,zombie->x-5,(int16_t)(y-28),36,44,0x7EDBFF30,0,0);
        return;
    }
    draw_actor(frame,(int16_t)(zombie->x-2),(int16_t)(y-15),30,30,zombie_actor_x(zombie->type),ACTOR_PLANT_Y,(uint8_t)alpha);
    if(zombie->slow_timer) pxa_canvas_ellipse_rgba(frame,zombie->x, (int16_t)(y-12), 30,30,0x7EDBFF30,0,0);
}
static void draw_sun(pxa_canvas_frame_t* frame,int16_t center_x,int16_t center_y,uint16_t size){
    pxa_canvas_image(frame,(int16_t)(center_x-size/2),(int16_t)(center_y-size/2),size,size,255,0,sun_asset,sizeof(sun_asset)-1);
}
static void draw_projectile(pxa_canvas_frame_t* frame,const projectile_t* projectile){
    int16_t y=(int16_t)(cell_y(projectile->row)+
                        (projectile->kind==PROJECTILE_SPORE?SPORE_Y_OFFSET:PROJECTILE_Y_OFFSET));
    if(projectile->kind==PROJECTILE_SPORE){
        pxa_canvas_circle(frame,(int16_t)(projectile->x-5),(int16_t)(y+1),1,0x8060B8);
        pxa_canvas_ellipse_rgba(frame,(int16_t)(projectile->x-4),(int16_t)(y-3),9,6,0xA774E8E0,1,0xD8C0FFFF);
        pxa_canvas_circle(frame,(int16_t)(projectile->x+1),(int16_t)(y-1),1,0xF1E5FF);
        return;
    }
    pxa_canvas_circle(frame,projectile->x,y,3,projectile->kind==PROJECTILE_ICE_PEA?0x7EDBFF:0xA6DE58);
}
static void draw_shovel_button(pxa_canvas_frame_t* frame){
    if(shovel_mode) pxa_canvas_rect(frame,SHOVEL_X-1,SHOVEL_Y-1,SHOVEL_W+2,SHOVEL_H+2,0xF6BE32,5);
    pxa_canvas_rect(frame,SHOVEL_X,SHOVEL_Y,SHOVEL_W,SHOVEL_H,shovel_mode?0xA6D66D:0x456B4B,4);
    draw_actor(frame,SHOVEL_X+3,SHOVEL_Y+1,SHOVEL_IMAGE_SIZE,SHOVEL_IMAGE_SIZE,ACTOR_SHOVEL_X,ACTOR_SHOVEL_Y,255);
}
static void draw_blast(pxa_canvas_frame_t* frame,const blast_t* blast){
    int first_row=blast->row>0?blast->row-1:0;
    int last_row=blast->row+1<BOARD_ROWS?blast->row+1:BOARD_ROWS-1;
    int first_col=blast->col>0?blast->col-1:0;
    int last_col=blast->col+1<BOARD_COLS?blast->col+1:BOARD_COLS-1;
    int16_t left=cell_x(first_col);
    int16_t top=cell_y(first_row);
    uint16_t width=(uint16_t)((last_col-first_col+1)*CELL_W);
    uint16_t height=(uint16_t)((last_row-first_row+1)*CELL_H);
    int16_t center_x=(int16_t)(cell_x(blast->col)+CELL_W/2);
    int16_t center_y=(int16_t)(cell_y(blast->row)+CELL_H/2);
    uint16_t elapsed=(uint16_t)(BOMB_BLAST_TICKS-blast->timer);
    uint16_t outer_radius=(uint16_t)(18+elapsed*3u);
    uint16_t inner_radius=(uint16_t)(10+elapsed*2u);
    if(!pxa_canvas_clip_push(frame,left,top,width,height)) return;
    pxa_canvas_rect_rgba(frame,left,top,width,height,0xFF592850,7,1,0xFFC463B0);
    pxa_canvas_ellipse_rgba(frame,(int16_t)(center_x-outer_radius),(int16_t)(center_y-outer_radius),outer_radius*2u,outer_radius*2u,0xFF8D1A90,0,0);
    pxa_canvas_ellipse_rgba(frame,(int16_t)(center_x-inner_radius),(int16_t)(center_y-inner_radius),inner_radius*2u,inner_radius*2u,0xFFDB4AC0,0,0);
    pxa_canvas_circle(frame,center_x,center_y,9,0xFFE982);
    pxa_canvas_line(frame,center_x,(int16_t)(center_y-outer_radius),center_x,(int16_t)(center_y-12),0xFFF5B1,2);
    pxa_canvas_line(frame,(int16_t)(center_x+outer_radius),center_y,(int16_t)(center_x+12),center_y,0xFFF5B1,2);
    pxa_canvas_line(frame,center_x,(int16_t)(center_y+outer_radius),center_x,(int16_t)(center_y+12),0xFFF5B1,2);
    pxa_canvas_line(frame,(int16_t)(center_x-outer_radius),center_y,(int16_t)(center_x-12),center_y,0xFFF5B1,2);
    pxa_canvas_clip_pop(frame);
}
static void draw_scene_background(pxa_canvas_frame_t* frame){
    if(is_night()){
        pxa_canvas_rect(frame,0,0,GARDEN_W,GARDEN_H,0x0B1A2E,12);
        pxa_canvas_circle(frame,GARDEN_W-48,30,18,0xFFF2A8);
        pxa_canvas_circle(frame,GARDEN_W-54,26,4,0xE8E0B0);
        pxa_canvas_circle(frame,GARDEN_W-44,34,3,0xE8E0B0);
        for(int i=0;i<20;++i){
            int16_t sx=(int16_t)( (i*73 + 17)%(GARDEN_W-16) + 8 );
            int16_t sy=(int16_t)( (i*53)%32 + 8 );
            uint8_t r=(uint8_t)(1+(i%2));
            pxa_canvas_circle(frame,sx,sy,r, i%3==0?0xFFFFFF:0xB9D8FF);
        }
        pxa_canvas_rect(frame,BOARD_X-28,BOARD_Y-4,BOARD_COLS*CELL_W+56,BOARD_ROWS*CELL_H+8,0x1A2F4A,12);
        pxa_canvas_rect(frame,BOARD_X-25,BOARD_Y-2,BOARD_COLS*CELL_W+50,BOARD_ROWS*CELL_H+4,0x243A5A,9);
        pxa_canvas_rect(frame,MOWER_HOME_X,BOARD_Y,BOARD_X-MOWER_HOME_X,BOARD_ROWS*CELL_H,0x2A3F5A,5);
        pxa_canvas_rect(frame,BOARD_X,BOARD_Y,BOARD_COLS*CELL_W,BOARD_ROWS*CELL_H,0x2E4A6A,5);
        for(int row=1;row<BOARD_ROWS;row+=2) pxa_canvas_rect(frame,BOARD_X,cell_y(row),BOARD_COLS*CELL_W,CELL_H,0x2A4260,0);
    } else {
        pxa_canvas_rect(frame,0,0,GARDEN_W,GARDEN_H,0x87CEEB,12);
        pxa_canvas_circle(frame,GARDEN_W-44,30,20,0xFFD76A);
        pxa_canvas_circle(frame,40,24,16,0xFFFFFF);
        pxa_canvas_circle(frame,62,20,20,0xFFFFFF);
        pxa_canvas_circle(frame,86,26,14,0xFFFFFF);
        pxa_canvas_rect(frame,0,BOARD_Y-6,GARDEN_W,GARDEN_H-(BOARD_Y-6),0x283E30,0);
        pxa_canvas_rect(frame,BOARD_X-28,BOARD_Y-4,BOARD_COLS*CELL_W+56,BOARD_ROWS*CELL_H+8,0x315D3C,12);
        pxa_canvas_rect(frame,BOARD_X-25,BOARD_Y-2,BOARD_COLS*CELL_W+50,BOARD_ROWS*CELL_H+4,0x4A8348,9);
        pxa_canvas_rect(frame,MOWER_HOME_X,BOARD_Y,BOARD_X-MOWER_HOME_X,BOARD_ROWS*CELL_H,0x4F843F,5);
        pxa_canvas_rect(frame,BOARD_X,BOARD_Y,BOARD_COLS*CELL_W,BOARD_ROWS*CELL_H,0x6AAA4D,5);
        for(int row=1;row<BOARD_ROWS;row+=2) pxa_canvas_rect(frame,BOARD_X,cell_y(row),BOARD_COLS*CELL_W,CELL_H,0x62A047,0);
    }
    for(int col=0;col<=BOARD_COLS;++col) pxa_canvas_line(frame,cell_x(col),BOARD_Y,cell_x(col),BOARD_Y+BOARD_ROWS*CELL_H, is_night()?0x3A5A7A:0xA7D07B,1);
    for(int row=1;row<BOARD_ROWS;++row) pxa_canvas_line(frame,BOARD_X,cell_y(row),BOARD_X+BOARD_COLS*CELL_W,cell_y(row), is_night()?0x3A5A7A:0xA7D07B,1);
}

static void draw_pause_menu(pxa_canvas_frame_t* frame){
    // 半透明遮罩
    pxa_canvas_rect_rgba(frame,0,0,GARDEN_W,GARDEN_H,0x00000080,12,0,0);
    pxa_canvas_rect(frame,PAUSE_MENU_X,PAUSE_MENU_Y,200,176,0x1E3A4A,12);
    pxa_canvas_rect_rgba(frame,PAUSE_MENU_X,PAUSE_MENU_Y,200,176,0x1E3A4A,12,2,0x4A6A8A);
    pxa_canvas_rect(frame,PAUSE_MENU_X,PAUSE_MENU_Y,200,26,0x2A4A6A,12);
    const char* pause_title=garden_message(PXA_MSG_PAUSE_TITLE);
    draw_body_button_text(frame,PAUSE_MENU_X,PAUSE_MENU_Y,200,26,0xFFFFFF,pause_title,garden_text_len(pause_title));
    // 标题、关卡名和昼夜标识分别占行，避免使用同一基线。
    {
        const level_def_t* lvl = current_level_def();
        const char* scene = garden_message(is_night()?PXA_MSG_SCENE_NIGHT:PXA_MSG_SCENE_DAY);
        const char* level_name=garden_message(lvl->name);
        pxa_canvas_text(frame,PAUSE_MENU_X+14,PAUSE_MENU_Y+32,128,0xB0D0E8,PXA_CANVAS_ALIGN_CENTER,level_name,garden_text_len(level_name));
        pxa_canvas_rect(frame,PAUSE_MENU_X+148,PAUSE_MENU_Y+28,40,26,is_night()?0x5A4FC0:0xE6BA49,5);
        draw_body_button_text(frame,PAUSE_MENU_X+148,PAUSE_MENU_Y+28,40,26,0xFFFFFF,scene,garden_text_len(scene));
    }
    // 继续
    pxa_canvas_rect(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+58,160,24,0xA6D66D,7);
    const char* resume_txt=garden_message(PXA_MSG_ACTION_RESUME);
    draw_body_button_text(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+58,160,24,0x1D3528,resume_txt,garden_text_len(resume_txt));
    // 重新开始
    pxa_canvas_rect(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+88,160,24,0x4A8AC0,7);
    const char* restart_txt=garden_message(PXA_MSG_ACTION_RESTART);
    draw_body_button_text(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+88,160,24,0xFFFFFF,restart_txt,garden_text_len(restart_txt));
    // 返回主菜单
    pxa_canvas_rect(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+118,160,24,0x5A3FA0,7);
    const char* home_txt=garden_message(PXA_MSG_ACTION_MAIN_MENU);
    draw_body_button_text(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+118,160,24,0xFFFFFF,home_txt,garden_text_len(home_txt));
    // 声音
    uint32_t sound_bg=sound_enabled?0xE6BA49:0x4A5A5A;
    uint32_t sound_fg=sound_enabled?0x1D3528:0xB0B0B0;
    pxa_canvas_rect(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+148,160,24,sound_bg,6);
    const char* sound_txt=garden_message(sound_enabled?PXA_MSG_SOUND_ON:PXA_MSG_SOUND_OFF);
    draw_body_button_text(frame,PAUSE_MENU_X+20,PAUSE_MENU_Y+148,160,24,sound_fg,sound_txt,garden_text_len(sound_txt));
}
static int render_game(pxa_canvas_frame_t* frame){
    char sun_text[10]; size_t sun_length=pxa_canvas_u32_text(sun_text,sun_count);
    const level_def_t* lvl=current_level_def();
    draw_scene_background(frame);
    // 顶栏: 仅保留暂停按钮（关卡信息已移至暂停菜单，避免左上角圆角遮挡与卡片重叠）
    pxa_canvas_rect(frame,PAUSE_X,PAUSE_Y,PAUSE_W,PAUSE_H, state==STATE_PAUSED?0xE6BA49:0x456B4B,6);
    if(state==STATE_PAUSED){
        static const char play_icon[]=PXA_CANVAS_ICON_PLAY;
        pxa_canvas_text_role(frame,PAUSE_X+4,PAUSE_Y+1,PAUSE_W-8,pxa_canvas_rgba(0xFFFFFF),PXA_CANVAS_FONT_ICON,PXA_CANVAS_ALIGN_CENTER,play_icon,sizeof(play_icon)-1);
    } else {
        static const char pause_icon[]=PXA_CANVAS_ICON_PAUSE;
        pxa_canvas_text_role(frame,PAUSE_X+4,PAUSE_Y+1,PAUSE_W-8,pxa_canvas_rgba(0xFFFFFF),PXA_CANVAS_FONT_ICON,PXA_CANVAS_ALIGN_CENTER,pause_icon,sizeof(pause_icon)-1);
    }
    // 卡片槽支持手指横向滑动，阳光已移到底部空白区以避开圆角。
    draw_seed_bar(frame);
    draw_shovel_button(frame);
    for(int row=0;row<BOARD_ROWS;++row){
        if(mower_state[row]!=MOWER_SPENT){
            int16_t y=(int16_t)(cell_y(row)+CELL_H/2);
            draw_actor(frame,mower_x[row],(int16_t)(y-14),30,28,ACTOR_MOWER_X,ACTOR_MOWER_Y,255);
        }
        for(int col=0;col<BOARD_COLS;++col) if(plants[row][col].type!=PLANT_EMPTY) draw_plant(frame,row,col);
        for(int col=0;col<BOARD_COLS;++col) if(crater_timer[row][col]>0){
            int16_t cx=cell_x(col)+CELL_W/2;
            int16_t cy=cell_y(row)+CELL_H/2+6;
            uint8_t alpha = crater_timer[row][col] > 100 ? 160 : 80;
            pxa_canvas_ellipse_rgba(frame,cx-12,cy-4,24,10,0x3A2A1A | alpha,0,0);
            pxa_canvas_ellipse_rgba(frame,cx-8,cy-2,16,6,0x5A3A2A | alpha,0,0);
        }
    }
    for(int i=0;i<MAX_PROJECTILES;++i) if(projectiles[i].active) draw_projectile(frame,&projectiles[i]);
    for(int i=0;i<MAX_ZOMBIES;++i) if(zombies[i].active) draw_zombie(frame,&zombies[i]);
    for(int i=0;i<MAX_BLASTS;++i) if(blasts[i].active) draw_blast(frame,&blasts[i]);
    for(int i=0;i<MAX_SUNS;++i) if(suns[i].active){
        uint16_t size=suns[i].value<25?16:SUN_IMAGE_SIZE;
        draw_sun(frame,suns[i].x,suns[i].y,size);
    }
    // 底部状态栏：阳光居中（适应长数字，避开圆角），进度细条置于屏幕最底下
    {
        // 阳光居中，宽度 88 可容纳 5 位数字，位于 216 高度避开圆角
        pxa_canvas_rect(frame,(GARDEN_W-88)/2,SUN_STATUS_Y,88,22,0x1D3528,7);
        draw_sun(frame,(GARDEN_W-88)/2+12,SUN_STATUS_Y+11,SUN_IMAGE_SIZE);
        pxa_canvas_text_role(frame,(GARDEN_W-88)/2+38,SUN_STATUS_Y-2,44,pxa_canvas_rgba(0xFFFFFF),PXA_CANVAS_FONT_CAPTION,PXA_CANVAS_ALIGN_CENTER,sun_text,sun_length);
        // 进度细条无文字，置于屏幕最底下居中，避开铲子与阳光
        pxa_canvas_rect(frame,(GARDEN_W-200)/2,GARDEN_BOTTOM-4,200,3,0x1A2F1A,1);
        if(defeated!=0) pxa_canvas_rect(frame,(GARDEN_W-200)/2,GARDEN_BOTTOM-4,(uint16_t)(defeated*200u/lvl->wave_goal),3,0xE6B43A,1);
    }
    if(state==STATE_READY){
        const char* ready=garden_message(PXA_MSG_GAME_READY);
        pxa_canvas_rect(frame,(GARDEN_W-140)/2,(GARDEN_H-72)/2,140,72,0x12281C,8);
        pxa_canvas_rect(frame,(GARDEN_W-136)/2,(GARDEN_H-70)/2,136,70,0x2F5A3A,8);
        pxa_canvas_rect(frame,(GARDEN_W-124)/2,(GARDEN_H-70)/2+4,124,2,0xA9D271,1);
        pxa_canvas_text(frame,(GARDEN_W-136)/2,(GARDEN_H-70)/2+12,136,0xFFFFFF,PXA_CANVAS_ALIGN_CENTER,ready,garden_text_len(ready));
        const char* hint=garden_message(PXA_MSG_GAME_PLANT_HINT);
        pxa_canvas_text(frame,(GARDEN_W-136)/2,(GARDEN_H-70)/2+42,136,0xC8E6A0,PXA_CANVAS_ALIGN_CENTER,hint,garden_text_len(hint));
    } else if(state==STATE_WIN){
        const char* won=garden_message(PXA_MSG_GAME_WON);
        pxa_canvas_rect(frame,(GARDEN_W-164)/2,(GARDEN_H-104)/2,164,104,0x16352A,10);
        pxa_canvas_rect(frame,(GARDEN_W-156)/2,(GARDEN_H-100)/2,156,100,0x286A4B,10);
        pxa_canvas_text(frame,(GARDEN_W-156)/2,(GARDEN_H-100)/2+10,156,0xFFFFFF,PXA_CANVAS_ALIGN_CENTER,won,garden_text_len(won));
        char star_txt[10]; size_t sl=pxa_canvas_u32_text(star_txt,level_stars[current_level]);
        static const char star_lbl[]="*";
        pxa_canvas_rect(frame,(GARDEN_W-120)/2,(GARDEN_H-100)/2+34,120,20,0x1D4A33,5);
        pxa_canvas_text(frame,(GARDEN_W-120)/2+20,(GARDEN_H-100)/2+37,40,0xFFE27A,PXA_CANVAS_ALIGN_CENTER,star_txt,sl);
        pxa_canvas_text(frame,(GARDEN_W-120)/2+60,(GARDEN_H-100)/2+37,30,0xFFE27A,PXA_CANVAS_ALIGN_LEFT,star_lbl,sizeof(star_lbl)-1);
        if(win_reward_plant!=0){
            const char* pn=plant_name(win_reward_plant);
            pxa_i18n_argument_t argument={"plant",5u,PXA_I18N_ARGUMENT_STRING,
                {.string={pn,plant_name_len(win_reward_plant)}}};
            char reward[48];
            size_t rl=pxa_i18n_format(&i18n,PXA_MSG_GAME_REWARD,&argument,1u,reward,sizeof(reward));
            pxa_canvas_text_role(frame,(GARDEN_W-156)/2,(GARDEN_H-100)/2+60,156,pxa_canvas_rgba(0xFFE27A),PXA_CANVAS_FONT_CAPTION,PXA_CANVAS_ALIGN_CENTER,reward,rl);
        }
        const char* cont=garden_message(PXA_MSG_ACTION_CONTINUE);
        pxa_canvas_rect(frame,(GARDEN_W-110)/2,(GARDEN_H-100)/2+76,110,20,0xF6BE32,6);
        draw_body_button_text(frame,(GARDEN_W-110)/2,(GARDEN_H-100)/2+76,110,20,0x1D3528,cont,garden_text_len(cont));
    } else if(state==STATE_LOSS){
        const char* lost=garden_message(PXA_MSG_GAME_LOST);
        pxa_canvas_rect(frame,(GARDEN_W-160)/2,(GARDEN_H-92)/2,160,92,0x3A1E20,10);
        pxa_canvas_rect(frame,(GARDEN_W-156)/2,(GARDEN_H-88)/2,156,88,0x70393C,10);
        pxa_canvas_text(frame,(GARDEN_W-156)/2,(GARDEN_H-88)/2+10,156,0xFFFFFF,PXA_CANVAS_ALIGN_CENTER,lost,garden_text_len(lost));
        const char* retry=garden_message(PXA_MSG_ACTION_RETRY);
        pxa_canvas_rect(frame,(GARDEN_W-110)/2,(GARDEN_H-88)/2+64,110,20,0xE65C51,6);
        draw_body_button_text(frame,(GARDEN_W-110)/2,(GARDEN_H-88)/2+64,110,20,0xFFFFFF,retry,garden_text_len(retry));
    }
    if(state==STATE_PAUSED){
        draw_pause_menu(frame);
    }
    return 1;
}
static void draw_home(pxa_canvas_frame_t* frame){
    pxa_canvas_rect(frame,0,0,GARDEN_W,GARDEN_H,0x1B4D3E,12);
    pxa_canvas_rect(frame,0,0,GARDEN_W,70,0x2E7D5A,12);
    pxa_canvas_circle(frame,250,22,20,0xFFD93D);
    pxa_canvas_circle(frame,30,18,12,0xFFFFFF);
    pxa_canvas_circle(frame,52,14,16,0xFFFFFF);
    pxa_canvas_circle(frame,74,20,10,0xFFFFFF);
    const char* title=garden_message(PXA_MSG_SCREEN_TITLE);
    pxa_canvas_text_role(frame,0,14,GARDEN_W,pxa_canvas_rgba(0xFFFFFF),PXA_CANVAS_FONT_TITLE,PXA_CANVAS_ALIGN_CENTER,title,garden_text_len(title));
    const char* subtitle=garden_message(PXA_MSG_SCREEN_SUBTITLE);
    pxa_canvas_text_role(frame,0,36,GARDEN_W,pxa_canvas_rgba(0xE8F5D0),PXA_CANVAS_FONT_CAPTION,PXA_CANVAS_ALIGN_CENTER,subtitle,garden_text_len(subtitle));
    // 已移除“白天收集阳光 夜晚蘑菇觉醒”提示行，进度与按钮上移并垂直居中
    pxa_canvas_rect(frame,HOME_PANEL_X,84,268,32,0x4A8348,8);
    char prog[48];
    uint8_t total_stars=0; for(uint8_t i=0;i<LEVELS_COUNT;++i) total_stars+=level_stars[i];
    pxa_i18n_argument_t progress_arguments[]={
        {"current",7u,PXA_I18N_ARGUMENT_U32,{.u32=max_unlocked}},
        {"total",5u,PXA_I18N_ARGUMENT_U32,{.u32=LEVELS_COUNT}},
        {"stars",5u,PXA_I18N_ARGUMENT_U32,{.u32=total_stars}}};
    size_t pl=pxa_i18n_format(&i18n,PXA_MSG_HOME_PROGRESS,
                              progress_arguments,3u,prog,sizeof(prog));
    pxa_canvas_text_box_role(frame,HOME_PANEL_X,84,268,32,pxa_canvas_rgba(0xFFE27A),
                             PXA_CANVAS_FONT_TITLE, PXA_CANVAS_ALIGN_CENTER,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, prog, pl);
    pxa_canvas_rect(frame,HOME_X,122,220,34,0xF6BE32,8);
    const char* btn1=garden_message(PXA_MSG_ACTION_START_ADVENTURE);
    draw_title_button_text(frame,HOME_X,122,220,34,0x1D3528,btn1,garden_text_len(btn1));
    pxa_canvas_rect(frame,HOME_X,166,106,30,0x456B4B,7);
    const char* btn2=garden_message(PXA_MSG_ACTION_SELECT_LEVEL);
    draw_body_button_text(frame,HOME_X,166,106,30,0xFFFFFF,btn2,garden_text_len(btn2));
    pxa_canvas_rect(frame,HOME_X+114,166,106,30,0x5A3FA0,7);
    const char* btn3=garden_message(PXA_MSG_ACTION_OPEN_ALMANAC);
    draw_body_button_text(frame,HOME_X+114,166,106,30,0xFFFFFF,btn3,garden_text_len(btn3));
    const char* ver=garden_message(PXA_MSG_SCREEN_VERSION);
    pxa_canvas_text_role(frame,0,GARDEN_H-23,GARDEN_W,pxa_canvas_rgba(0x9FBFA4),PXA_CANVAS_FONT_CAPTION,PXA_CANVAS_ALIGN_CENTER,ver,garden_text_len(ver));
}

static void draw_back_button(pxa_canvas_frame_t* frame){
    pxa_canvas_rect_rgba(frame,20,8,32,26,0x173550,8,1,0x6F9FC2);
    pxa_canvas_line(frame,37,14,29,21,0xFFFFFF,2);
    pxa_canvas_line(frame,29,21,37,28,0xFFFFFF,2);
    pxa_canvas_line(frame,29,21,44,21,0xFFFFFF,2);
}

static uint16_t list_scroll_limit(void){
    uint16_t content_end;
    if(state==STATE_LEVELS){
        content_end=(uint16_t)(LEVEL_GRID_Y + ((LEVELS_COUNT-1u)/2u)*LEVEL_CARD_STRIDE_Y + LEVEL_CARD_H);
    } else if(state==STATE_ALMANAC){
        content_end=(uint16_t)(ALMANAC_GRID_Y + (PLANT_TYPE_COUNT-2u)*ALMANAC_CARD_STRIDE_Y + ALMANAC_CARD_H);
    } else {
        return 0;
    }
    return content_end>240u ? (uint16_t)(content_end-240u) : 0;
}

static uint16_t* list_scroll_value(void){
    if(state==STATE_LEVELS) return &level_scroll;
    if(state==STATE_ALMANAC) return &almanac_scroll;
    return NULL;
}

static void scroll_list_to(int32_t value){
    uint16_t* scroll=list_scroll_value();
    uint16_t limit=list_scroll_limit();
    if(scroll==NULL) return;
    if(value<0) value=0;
    if(value>(int32_t)limit) value=limit;
    *scroll=(uint16_t)value;
}

static void draw_list_scrollbar(pxa_canvas_frame_t* frame,uint16_t scroll){
    uint16_t limit=list_scroll_limit();
    if(limit==0) return;
    uint16_t content_h=(uint16_t)(LIST_VIEW_H+limit);
    uint16_t thumb_h=(uint16_t)((LIST_VIEW_H*LIST_VIEW_H)/content_h);
    if(thumb_h<22) thumb_h=22;
    int16_t thumb_y=(int16_t)(LIST_VIEW_Y + ((LIST_VIEW_H-thumb_h)*scroll)/limit);
    pxa_canvas_rect(frame,289,LIST_VIEW_Y+5,3,LIST_VIEW_H-10,0x28465A,2);
    pxa_canvas_rect(frame,289,thumb_y,3,thumb_h,0xA6D66D,2);
}

static void draw_levels(pxa_canvas_frame_t* frame){
    pxa_canvas_rect(frame,0,0,GARDEN_W,GARDEN_H,0x162A40,12);
    pxa_canvas_rect(frame,0,0,GARDEN_W,LIST_HEADER_H,0x1C3A5A,12);
    const char* title=garden_message(PXA_MSG_LEVEL_SCREEN_TITLE);
    pxa_canvas_text(frame,0,9,GARDEN_W,0xFFFFFF,PXA_CANVAS_ALIGN_CENTER,title,garden_text_len(title));
    draw_back_button(frame);
    pxa_canvas_clip_push(frame,0,LIST_VIEW_Y,GARDEN_W,LIST_VIEW_H);
    for(uint8_t i=0;i<LEVELS_COUNT;++i){
        uint8_t col=i%2;
        uint8_t row=i/2;
        int16_t x=(int16_t)(LEVEL_CARD_X+col*LEVEL_CARD_STRIDE_X);
        int16_t y=(int16_t)(LEVEL_GRID_Y+row*LEVEL_CARD_STRIDE_Y-level_scroll);
        uint8_t locked=i>=max_unlocked;
        uint32_t bg=locked?0x2A3F5A: (levels[i].scene==SCENE_NIGHT?0x2E3A6E:0x3A6B4A);
        uint32_t border=locked?0x3A4F6A: (levels[i].scene==SCENE_NIGHT?0x7A6BFF:0xA6D66D);
        pxa_canvas_rect_rgba(frame,x,y,LEVEL_CARD_W,LEVEL_CARD_H,bg,7,2,border);
        if(locked){
            pxa_canvas_rect_rgba(frame,x,y,LEVEL_CARD_W,LEVEL_CARD_H,0x00000070,7,0,0);
            const char* lock=garden_message(PXA_MSG_LEVEL_LOCKED);
            const char* lock_tip=garden_message(PXA_MSG_LEVEL_NOT_UNLOCKED);
            pxa_canvas_text(frame,x,y+14,LEVEL_CARD_W,0x8CA0B8,PXA_CANVAS_ALIGN_CENTER,lock,garden_text_len(lock));
            pxa_canvas_text(frame,x,y+58,LEVEL_CARD_W,0x8CA0B8,PXA_CANVAS_ALIGN_CENTER,lock_tip,garden_text_len(lock_tip));
        } else {
            const char* level_code=garden_message(levels[i].name);
            const char* level_name=level_code+4;
            uint8_t level_name_len=(uint8_t)(garden_text_len(level_code)-4u);
            pxa_canvas_text(frame,x+10,y+8,62,0xFFFFFF,PXA_CANVAS_ALIGN_LEFT,level_code,3);
            const char* scene_icon=garden_message(levels[i].scene==SCENE_NIGHT?PXA_MSG_SCENE_NIGHT:PXA_MSG_SCENE_DAY);
            pxa_canvas_rect(frame,x+94,y+8,26,26, levels[i].scene==SCENE_NIGHT?0x5A4FC0:0xE6BA49,4);
            draw_body_button_text(frame,x+94,y+8,26,26,0xFFFFFF,scene_icon,garden_text_len(scene_icon));
            uint8_t stars=level_stars[i];
            if(stars>0){
                char st[6]; size_t sl=0;
                for(uint8_t s=0;s<stars;++s) st[sl++]='*';
                pxa_canvas_text(frame,x+76,y+39,46,0xFFE27A,PXA_CANVAS_ALIGN_CENTER,st,sl);
            } else {
                static const char nostar[]="---";
                pxa_canvas_text(frame,x+76,y+39,46,0x6A8A7A,PXA_CANVAS_ALIGN_CENTER,nostar,sizeof(nostar)-1);
            }
            pxa_canvas_text(frame,x+10,y+61,112,0xC8E6A0,PXA_CANVAS_ALIGN_CENTER,level_name,level_name_len);
        }
    }
    pxa_canvas_clip_pop(frame);
    draw_list_scrollbar(frame,level_scroll);
}
static void draw_seed_select(pxa_canvas_frame_t* frame){
    const level_def_t* lvl=current_level_def();
    pxa_canvas_rect(frame,0,0,GARDEN_W,GARDEN_H, is_night()?0x0B1A2E:0x1B4D3E,12);
    pxa_canvas_rect(frame,0,0,GARDEN_W,36, is_night()?0x1C2E4A:0x2E5A3A,12);
    draw_back_button(frame);
    const char* scene_txt=garden_message(is_night()?PXA_MSG_SCENE_NIGHT:PXA_MSG_SCENE_DAY);
    const char* level_name=garden_message(lvl->name);
    pxa_canvas_text(frame,SEED_OX+60,9,148,0xFFFFFF,PXA_CANVAS_ALIGN_CENTER,level_name,garden_text_len(level_name));
    pxa_canvas_rect(frame,SEED_OX+220,6,56,26, is_night()?0x5A4FC0:0xE6BA49,5);
    draw_body_button_text(frame,SEED_OX+220,6,56,26,0xFFFFFF,scene_txt,garden_text_len(scene_txt));
    pxa_canvas_rect(frame,SEED_OX+8,42,280,46,0x1D3528,6);
    pxa_canvas_rect(frame,SEED_OX+10,44,276,42,0x2A4A3A,5);
    for(uint8_t i=0;i<lvl->slots;++i){
        int16_t x=(int16_t)(SEED_OX+16+i*38);
        int16_t y=48;
        if(i<chosen_count){
            uint8_t pt=chosen_plants[i];
            pxa_canvas_rect(frame,x,y,34,34,0xCFEFBB,3);
            draw_actor(frame,x+2,y+2,30,30,plant_actor_x(pt),plant_actor_y(pt),255);
        } else {
            pxa_canvas_rect_rgba(frame,x,y,34,34,0x2A4A3A,3,1,0x4A6A4A);
            pxa_canvas_line(frame,x+10,y+17,x+24,y+17,0x4A6A4A,1);
            pxa_canvas_line(frame,x+17,y+10,x+17,y+24,0x4A6A4A,1);
        }
    }
    uint8_t avail_count=0;
    uint8_t avail_list[PLANT_TYPE_COUNT];
    for(uint8_t t=1;t<PLANT_TYPE_COUNT;++t) if(is_plant_unlocked(t)){
        if(plant_is_selectable(t)) avail_list[avail_count++]=t;
    }
    if(plant_is_selectable(focused_plant)){
        uint8_t detail_length=0;
        const char* detail=plant_description(focused_plant,&detail_length);
        const char* detail_name=plant_name(focused_plant);
        uint8_t detail_name_length=plant_name_len(focused_plant);
        pxa_canvas_rect(frame,SEED_OX+8,94,280,106,0x243A5A,6);
        pxa_canvas_rect(frame,SEED_OX+10,96,276,102,0x1E344E,5);
        pxa_canvas_rect(frame,SEED_OX+20,122,48,48,0xCFEFBB,4);
        draw_actor(frame,SEED_OX+29,131,30,30,plant_actor_x(focused_plant),plant_actor_y(focused_plant),255);
        pxa_canvas_text(frame,SEED_OX+80,104,110,0xFFE27A,PXA_CANVAS_ALIGN_LEFT,detail_name,detail_name_length);
        char cost_text[24];
        pxa_i18n_argument_t cost_argument={"cost",4u,PXA_I18N_ARGUMENT_U32,
                                           {.u32=plant_cost(focused_plant)}};
        size_t cost_length=pxa_i18n_format(&i18n,PXA_MSG_SEED_SUN_COST,
                                           &cost_argument,1u,cost_text,sizeof(cost_text));
        pxa_canvas_text(frame,SEED_OX+80,132,110,0xFFE27A,PXA_CANVAS_ALIGN_LEFT,cost_text,cost_length);
        pxa_canvas_text(frame,SEED_OX+20,174,164,0xC8E6A0,PXA_CANVAS_ALIGN_CENTER,detail,detail_length);
        pxa_canvas_rect(frame,SEED_OX+196,112,80,36,0xA6D66D,7);
        const char* choose=garden_message(PXA_MSG_ACTION_CHOOSE);
        draw_body_button_text(frame,SEED_OX+196,112,80,36,0x1D3528,choose,garden_text_len(choose));
        pxa_canvas_rect(frame,SEED_OX+196,160,80,30,0x4A6A8A,7);
        const char* cancel=garden_message(PXA_MSG_ACTION_CANCEL);
        draw_body_button_text(frame,SEED_OX+196,160,80,30,0xFFFFFF,cancel,garden_text_len(cancel));
    } else {
        pxa_canvas_rect(frame,SEED_OX+8,94,280,106,0x243A5A,6);
        pxa_canvas_rect(frame,SEED_OX+10,96,276,102,0x1E344E,5);
        for(uint8_t i=0;i<avail_count;++i){
            uint8_t pt=avail_list[i];
            uint8_t col=i%7;
            uint8_t row=i/7;
            int16_t x=(int16_t)(SEED_OX+12+col*40);
            int16_t y=(int16_t)(102+row*46);
            pxa_canvas_rect(frame,x,y,36,40,0xCFEFBB,4);
            draw_actor(frame,x+3,y+5,30,30,plant_actor_x(pt),plant_actor_y(pt),255);
        }
        if(avail_count==0){
            const char* empty=garden_message(PXA_MSG_SEED_EMPTY);
            pxa_canvas_text(frame,SEED_OX+14,140,268,0x8CA8C8,PXA_CANVAS_ALIGN_CENTER,empty,garden_text_len(empty));
        }
    }
    uint32_t start_bg=chosen_count>0?0xA6D66D:0x4A5A4A;
    uint32_t start_fg=chosen_count>0?0x1D3528:0x8CA0B8;
    pxa_canvas_rect(frame,SEED_OX+58,202,180,30,start_bg,7);
    const char* start_txt=garden_message(PXA_MSG_ACTION_START_BATTLE);
    draw_body_button_text(frame,SEED_OX+58,202,180,30,start_fg,start_txt,garden_text_len(start_txt));
}
static void draw_almanac(pxa_canvas_frame_t* frame){
    pxa_canvas_rect(frame,0,0,GARDEN_W,GARDEN_H,0x1C2E2E,12);
    pxa_canvas_rect(frame,0,0,GARDEN_W,LIST_HEADER_H,0x2A4A3A,12);
    draw_back_button(frame);
    const char* title=garden_message(PXA_MSG_ALMANAC_TITLE);
    pxa_canvas_text(frame,0,9,GARDEN_W,0xFFFFFF,PXA_CANVAS_ALIGN_CENTER,title,garden_text_len(title));
    pxa_canvas_clip_push(frame,0,LIST_VIEW_Y,GARDEN_W,LIST_VIEW_H);
    for(uint8_t pt=PLANT_SHOOTER;pt<PLANT_TYPE_COUNT;++pt){
        int16_t y=(int16_t)(ALMANAC_GRID_Y+(pt-PLANT_SHOOTER)*ALMANAC_CARD_STRIDE_Y-almanac_scroll);
        uint8_t unlocked=is_plant_unlocked(pt);
        uint32_t bg=unlocked?0x2A4A3A:0x1E2A2A;
        pxa_canvas_rect(frame,ALMANAC_CARD_X,y,ALMANAC_CARD_W,ALMANAC_CARD_H,bg,6);
        pxa_canvas_rect(frame,10,y+20,46,46, unlocked?0xCFEFBB:0x3A4A4A,4);
        if(unlocked){
            draw_actor(frame,18,y+28,30,30,plant_actor_x(pt),plant_actor_y(pt),255);
        } else {
            const char* lock=garden_message(PXA_MSG_LEVEL_LOCKED);
            pxa_canvas_text(frame,10,y+34,46,0x8CA0B8,PXA_CANVAS_ALIGN_CENTER,lock,garden_text_len(lock));
        }
        pxa_canvas_text(frame,64,y+7,132,unlocked?0xFFFFFF:0x6A7A6A,PXA_CANVAS_ALIGN_LEFT,plant_name(pt),plant_name_len(pt));
        char cost_text[24];
        pxa_i18n_argument_t cost_argument={"cost",4u,PXA_I18N_ARGUMENT_U32,
                                           {.u32=plant_cost(pt)}};
        size_t cost_length=pxa_i18n_format(&i18n,PXA_MSG_SEED_SUN_COST,
                                           &cost_argument,1u,cost_text,sizeof(cost_text));
        pxa_canvas_text(frame,64,y+32,126,unlocked?0xFFE27A:0x5A6A5A,PXA_CANVAS_ALIGN_LEFT,cost_text,cost_length);
        uint8_t description_length=0;
        const char* description=plant_description(pt,&description_length);
        pxa_canvas_text(frame,64,y+58,212,unlocked?0xB8D3BE:0x5A6A5A,PXA_CANVAS_ALIGN_LEFT,description,description_length);
        if(!unlocked){
            const char* need=garden_message(PXA_MSG_ALMANAC_UNLOCK_REQUIRED);
            pxa_canvas_rect(frame,194,y+8,82,26,0x4A3A2A,4);
            pxa_canvas_text(frame,194,y+11,82,0xFF8A8A,PXA_CANVAS_ALIGN_CENTER,need,garden_text_len(need));
        } else {
            const char* ok=garden_message(PXA_MSG_ALMANAC_UNLOCKED);
            pxa_canvas_text(frame,204,y+11,72,0xA6D66D,PXA_CANVAS_ALIGN_CENTER,ok,garden_text_len(ok));
        }
    }
    pxa_canvas_clip_pop(frame);
    draw_list_scrollbar(frame,almanac_scroll);
}
static int render(void){
    pxa_canvas_frame_t frame;
    pxa_canvas_begin(&frame,draw_data,sizeof(draw_data));
    if(state==STATE_HOME) draw_home(&frame);
    else if(state==STATE_LEVELS) draw_levels(&frame);
    else if(state==STATE_SEED_SELECT) draw_seed_select(&frame);
    else if(state==STATE_ALMANAC) draw_almanac(&frame);
    else {
        render_game(&frame);
    }
    return pxa_canvas_present(GAME_NODE,&frame,&generation,&initialized,packet,sizeof(packet),packet,sizeof(packet));
}
