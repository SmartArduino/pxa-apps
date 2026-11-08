// SPDX-License-Identifier: MIT
// Original, resumable five-scene prologue. It runs without advancing farm time.
struct OpeningScene {const char *title_cn,*title_en,*cn,*en;};
constexpr OpeningScene Opening[]={
  {"一封迟到的信","A letter, years late",
   "末班车驶过雨夜，你在旧外套里摸到一封转寄的信。信封上是童年的字：长大后，记得回晴露谷，把春天种回来。",
   "After a late shift, you find a letter from your childhood self: Return to Dewfield. Plant a spring of our own."},
  {"驶向山谷","The valley-bound bus",
   "你请了一次很长的假。车窗外，高楼变成了山和田。林婆婆在回信里说：旧农场还在，门口那棵树也一直等着你。",
   "You finally take leave. Towers turn into fields. Lin writes: The old farm is here. The tree by its gate waits for you."},
  {"门还为你开着","A gate left open",
   "林婆婆把旧钥匙放进你手里：这不是一场考试。田荒了就慢慢开，桥坏了就一起修。先住下来，听听自己想过什么日子。",
   "Lin hands you the key. Take your time. Clear one patch; mend the bridge together. Find the days you want to live."},
  {"第一把种子","A handful of seeds",
   "小禾带来种子，澄在远处招手：等旧桥修好，我带你去湖边。你忽然明白，那封信里的春天，不只是一片长满作物的田。",
   "Xiaohe brings seeds. Cheng waves: Mend the bridge and we'll see the lake! Spring begins with more than a field."},
  {"清晨的新声音","A new morning chorus",
   "阿岚推开旧棚：林婆婆托你照顾这两只小鸡，草料放在门边。先添满食槽吧。明天，也许会有新的声音叫你起床。",
   "Alan opens the shed. Lin left two young chickens and hay for you. Fill the trough. New voices will greet the morning."}
};
void start_intro(bool replay) {
  open_page(Page::Intro);intro_replay=replay;intro_skip=false;
  intro_scene=replay?0:std::min(int(state.intro),IntroScenes-1);intro_elapsed=0;toast_ms=0;redraw();
}
void finish_intro() {
  if(!intro_replay) {state.intro=IntroScenes;begin_ranch(state);base_dirty=true;changed();}
  open_page(intro_replay?Page::Story:Page::Farm);intro_replay=false;intro_skip=false;
  if(page==Page::Farm)msg("欢迎回家。手册里的牧场入口可找到食槽","Welcome home. The journal leads to your trough");
}
void intro_next() {
  if(intro_elapsed<180)return;
  if(intro_skip){finish_intro();return;}
  if(++intro_scene>=IntroScenes){finish_intro();return;}
  intro_elapsed=0;
  if(!intro_replay){state.intro=uint8_t(intro_scene);changed();}
}
void draw_intro() {
  art::Raster r{framebuffer(),WIDTH,HEIGHT};
  int horizon=HEIGHT/2-16;
  clear(intro_scene?art::Sky:art::rgb(55,67,77));
  if(!intro_scene) {
    for(int i=0;i<WIDTH/24+1;++i) {
      int roof=24+(i*37)%55;
      rect(i*24,roof,21,horizon+22-roof,art::rgb(69,79,83));
      for(int y=roof+8;y<horizon+10;y+=14)for(int x=4;x<19;x+=8)rect(i*24+x,y,3,5,(i+y)%3?art::rgb(166,144,100):art::rgb(87,100,100));
    }
    for(int i=0;i<25;++i){int x=(i*41+intro_elapsed/45)%WIDTH,y=(i*13+intro_elapsed/25)%std::max(1,horizon+20);line(x,y,x-2,y+5,art::rgb(113,139,147));}
    rect(0,horizon+18,WIDTH,HEIGHT-horizon,art::rgb(106,87,66));
    art::gait::draw(r,WIDTH/2+28,horizon+25,3,0);
    rect(WIDTH/2-32,horizon+12,39,17,art::Ink);rect(WIDTH/2-30,horizon+12,35,3,art::Gold);
    rect(WIDTH/2-19,horizon+4,16,9,art::Cream);line(WIDTH/2-18,horizon+5,WIDTH/2-11,horizon+9,art::Earth);
  } else {
    for(int i=-1;i<WIDTH/48+2;++i) {
      int x=i*48-(intro_scene==1?int(intro_elapsed/100)%48:0);
      rect(x,horizon-35+(i&1)*8,48,60,art::rgb(131,157,103));
      rect(x+8,horizon-42+(i&1)*8,30,10,art::rgb(131,157,103));
    }
    rect(0,horizon+5,WIDTH,HEIGHT-horizon,art::Green);
    for(int i=0;i<70;++i){int x=(i*73+11)%WIDTH,y=horizon+8+(i*19)%std::max(1,HEIGHT-horizon-20);rect(x,y,2,1,art::Leaf);}
    rect(0,horizon+26,WIDTH,21,art::rgb(197,160,94));
    for(int x=0;x<WIDTH;x+=23)rect(x,horizon+28,6,1,art::rgb(223,185,112));
    if(intro_scene==1||intro_scene==2) {
      int bx=intro_scene==1?std::min(WIDTH/2-38,-65+int(intro_elapsed/24)):8;
      rect(bx,horizon-14,76,38,art::Ink);rect(bx+1,horizon-13,74,35,art::Cream);
      rect(bx+3,horizon-8,69,13,art::rgb(94,138,143));
      for(int x=0;x<4;++x)rect(bx+6+x*17,horizon-7,12,11,art::rgb(161,192,172));
      rect(bx+2,horizon+8,72,11,art::rgb(115,139,100));rect(bx+62,horizon-5,9,27,art::rgb(157,149,104));
      for(int x:{12,56}){rect(bx+x,horizon+20,12,7,art::Ink);rect(bx+x+3,horizon+22,6,3,art::Earth);}
    }
    if(intro_scene>=2) {
      int fx=WIDTH/2+(intro_scene==2?12:0),fy=horizon+24;
      if(intro_scene>=3)art::house(r,WIDTH/2-82,horizon-76,false,false);
      int dist=std::min(14,int(intro_elapsed/100));int frame=intro_elapsed<1400?walk_frame(dist):0;
      art::gait::draw(r,fx-14+dist,fy,1,frame);
      int npc=intro_scene==2?3:intro_scene==3?0:1;
      art::gait::draw(r,fx+34,fy,3,0,false,&art::villager_rig(npc,3));
      if(intro_scene==3){art::carry_item(r,fx+34,fy,8,0);art::gait::draw(r,fx+70,fy+3,3,0,false,&art::villager_rig(2,3));}
      if(intro_scene==4) {
        art::ranch_shed(r,WIDTH/2-76,horizon+11,4);
        farm::Animal chick{1,0,2,55};
        for(int i=0;i<2;++i)art::animal(r,fx+20+i*15,fy+6,chick,3,(intro_elapsed/220+i)%2,false);
      }
    }
  }
  begin_ui();
  panel(6,6,std::min(WIDTH-78,234),25);fit_text(13,12,english()?Opening[intro_scene].title_en:Opening[intro_scene].title_cn,std::min(WIDTH-92,220),Sage);
  button(WIDTH-66,6,60,24,intro_skip?tr("RESUME","继续看"):tr("SKIP","跳过"));
  int y=HEIGHT-105;panel(6,y,WIDTH-12,99);
  const auto &scene=Opening[intro_scene];
  paragraph(14,y+8,intro_skip?tr("Skip to the farm? Your starter chickens and hay will still be waiting.","跳过演出，直接开始农场生活吗？两只小鸡与草料仍会送到。可在故事手册重看序章。"):english()?scene.en:scene.cn,WIDTH-28,4,Text);
  char b[24];std::snprintf(b,sizeof(b),"%d / %d",intro_scene+1,IntroScenes);text(16,HEIGHT-26,b,Muted);
  button(WIDTH-116,HEIGHT-34,102,23,intro_skip?tr("CONFIRM SKIP","确认跳过"):intro_scene==IntroScenes-1?tr("WELCOME HOME","开始生活"):tr("NEXT / A","下一幕"));
}
