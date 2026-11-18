// SPDX-License-Identifier: MIT
void word(Paint& p,unsigned id,Rect rect){p.image(id,rect);}
void board(Paint& p){draw_board(p,game,layout,using_keys,row,col,false);}
void transient_notice(Paint& p){using namespace garden_art;
 if(notice_time>0){unsigned label=notice==std::string_view("露珠不足")?text_funds:notice==std::string_view("已有植物")?text_occupied:text_empty;
  p.image(ui_notice,{layout.x+layout.w/2-61,layout.by+layout.cell*2-14,122,30});word(p,label,{layout.x+layout.w/2-53,layout.by+layout.cell*2-10,106,23});}
}
void game_button(Paint& p,Rect r,unsigned label,bool focused=false){using namespace garden_art;p.image(ui_button,r);word(p,label,{r.x+10,r.y+3,r.w-20,r.h-6});if(focused){p.image(ui_focus_leaf,{r.x-11,r.y+r.h/2-6,14,12});p.image(ui_focus_leaf,{r.x+r.w-3,r.y+r.h/2-6,14,12});}}
void scenery(Paint& p){using namespace garden_art;
 for(int y=0;y<2;++y)for(int x=0;x<2;++x)p.image(garden_0+y*2+x,{x*w/2,y*h/2,(x+1)*w/2-x*w/2,(y+1)*h/2-y*h/2},false);
 if(!p.prepare_only)shade_lawn(p.pixels,p.width,p.height,p.stride,layout.field(),layout.cell,p.zoom);
}
void draw(Paint& p){using namespace garden_art;int cx=layout.x+layout.w/2;
 if(screen==Screen::play||screen==Screen::pause){board(p);if(screen==Screen::play){transient_notice(p);return;}}
 if(screen==Screen::home){
  p.image(ui_title_board,{cx-108,layout.y+15,216,65});word(p,text_title,{cx-92,layout.y+25,184,43});
  for(int i=0;i<stage_count;++i){auto r=menu_rect(i);p.image(i==menu?ui_medal:ui_stone,r);p.number({r.x+8,r.y+7,r.w-16,r.h-14},i+1);if(i>=unlocked)p.image(ui_leaf_shot,{r.x+r.w-12,r.y+2,12,9});}

  p.image(ui_start_button,layout.start());word(p,text_start,{layout.start().x+15,layout.start().y+8,layout.start().w-30,layout.start().h-16});p.image(ui_help,menu_rect(stage_count));return;
 }
 if(screen==Screen::help){
  p.image(ui_title_board,{cx-109,layout.y+8,218,42});word(p,text_help,{cx-71,layout.y+13,142,31});
  int y=layout.y+60,step=std::max(36,(layout.h-104)/3);
  if(help_page==0){
  p.image(ui_seed_selected,{cx-86,y,28,34},false);p.actor(coralflower_idle,0,cx-72,y+26,19);p.image(ui_leaf_shot,{cx-25,y+12,28,13});p.actor(coralflower_idle,0,cx+48,y+34,30);p.image(glyph_a,{cx+77,y+6,17,23});
  y+=step;p.actor(dewflower_action,float(now%600000)/600000.f,cx-67,y+34,30);p.image(ui_dew,{cx-10,y+9,22,26});p.number({cx+35,y+11,45,23},10);
  y+=step;p.actor(leafshield_action,0,cx-65,y+34,30);p.actor(dustimp_action,.3f,cx-18,y+34,29);p.number({cx+26,y+10,40,22},100);p.image(glyph_seconds,{cx+68,y+10,19,22});
  }else if(help_page==1){
   constexpr Defender kinds[]={Defender::frost,Defender::ember,Defender::clover};
   for(int i=0;i<3;++i){int yy=y+i*step;p.actor(plant_clip(kinds[i],1),float(now%600000)/600000.f,cx-72,yy+33,27);p.number({cx-48,yy+9,28,19},cost[int(kinds[i])]);p.image(i==0?frost_shot:i==1?ember_shot:ui_dew,{cx-12,yy+8,22,20});
    if(i<2)p.actor(enemy_clip(i==0?EnemyKind::moth:EnemyKind::snail),0,cx+37,yy+33,25);else {p.actor(plant_clip(Defender::stone),0,cx+37,yy+33,25);p.image(glyph_plus,{cx+58,yy+10,12,16});}
    p.number({cx+70,yy+10,30,19},i==0?45:i==1?38:14);if(i==0)p.image(glyph_percent,{cx+99,yy+10,12,17});}
  }else{
   for(int i=0;i<4;++i){int xx=cx-75+(i%2)*105,yy=y+(i/2)*(step+12);p.actor(enemy_clip(EnemyKind(i),1),float(now%600000)/600000.f,xx,yy+34,28);p.number({xx+19,yy+10,42,19},i==2?160:i==3?38:55);}
   p.number({cx-15,layout.y+layout.h-57,30,17},stage_count);p.image(glyph_stage,{cx+17,layout.y+layout.h-57,16,18});
  }
  p.arrow({layout.x+10,layout.y+layout.h-31,27,27},false,true);p.arrow({layout.x+layout.w-37,layout.y+layout.h-31,27,27},true,true);
  game_button(p,{cx-47,layout.y+layout.h-31,94,27},text_back,true);return;
 }
 p.image(ui_pause_panel,{cx-112,layout.y+(layout.h-224)/2,224,224});
 unsigned heading=screen==Screen::pause?text_paused:screen==Screen::won?text_won:text_lost;
 word(p,heading,{cx-83,modal_y()+15,166,33});
 if(screen==Screen::won)p.image(ui_medal,{cx-19,modal_y()+47,38,38});
 for(int i=0;i<menu_count();++i){unsigned label;
  if(screen==Screen::pause){constexpr unsigned labels[]={text_resume,text_help,text_restart,text_home,text_exit};label=labels[i];}
  else label=i==1?text_home:screen==Screen::won?(game.level+1<stage_count?text_next:text_again):text_restart;
  auto r=menu_rect(i);
  game_button(p,r,label,i==menu);
 }
 if(screen==Screen::pause){auto r=sound_rect();word(p,text_sound,{r.x,r.y,38,r.h});p.number({r.x+39,r.y+2,12,r.h-4},sounds.enabled?1:0);}
}
