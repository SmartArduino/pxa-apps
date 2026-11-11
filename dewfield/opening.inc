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
  if(!intro_replay) {state.intro=IntroScenes;dewfield_actions::begin_ranch(state);base_dirty=true;changed();}
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
farm_ui::Rect intro_next_button(){int bottom=HEIGHT-std::max(4,layout().navigation_bottom+4);return {WIDTH-116,bottom-28,102,22};}
farm_ui::Rect intro_skip_button(){return {WIDTH>=400?6:WIDTH-66,6,60,24};}
void draw_intro() {
  int bottom=HEIGHT-std::max(4,layout().navigation_bottom+4);
  bool landscape=WIDTH>=400;
  int dialogue_width=landscape?std::min(188,WIDTH*2/5):WIDTH;
  int image_w=landscape?WIDTH-dialogue_width:WIDTH;
  int image_h=std::max(1,landscape?bottom+4:bottom-111);
  int source_w=opening_art::Width,source_h=opening_art::Height;
  bool wide=image_w*source_h>image_h*source_w;
  int crop_w=wide?source_w:source_h*image_w/image_h;
  int crop_h=wide?source_w*image_h/image_w:source_h;
  int pan=std::min(3,int(intro_elapsed/1600));
  int left=(source_w-crop_w)/2,top=std::min(source_h-crop_h,(source_h-crop_h)/2+pan);
  auto *target=framebuffer();auto *pixels=reinterpret_cast<uint8_t*>(unlit_base);int asset=opening_art::index(intro_scene,state.appearance);auto &palette=opening_art::palette[asset];
  bool ready=opening_image(asset,pixels,source_w*source_h);
  rect(0,0,WIDTH,HEIGHT,art::Ink);
  for(int y=0;y<image_h;++y)for(int x=0;x<image_w;++x)
    target[y*WIDTH+x]=ready?palette[pixels[(top+y*crop_h/image_h)*source_w+left+x*crop_w/image_w]]:art::Ink;
  begin_ui();
  if(!ready)fit_text(10,image_h/2,tr("Loading the story...","故事画面载入中…"),image_w-20,Paper);
  auto skip=intro_skip_button();button(skip.x,skip.y,skip.w,skip.h,intro_skip?tr("RESUME","继续看"):tr("SKIP","跳过"));
  const auto &scene=Opening[intro_scene];
  const char *body=intro_skip?tr("Skip to your farm? Replay this story from the journal.","跳过演出，直接开始农场生活吗？以后可以在故事手册重看。"):
    english()?scene.en:scene.cn;
  if(landscape) {
    panel(image_w+2,4,dialogue_width-6,bottom-4);
    paragraph(image_w+13,16,english()?scene.title_en:scene.title_cn,dialogue_width-26,2,Sage);
    line(image_w+13,46,WIDTH-14,46,Soft);
    paragraph(image_w+13,56,body,dialogue_width-26,std::max(1,(bottom-102)/14),Text);
  } else {
    panel(6,6,std::min(WIDTH-78,234),25);
    fit_text(13,12,english()?scene.title_en:scene.title_cn,std::min(WIDTH-92,220),Sage);
    panel(4,image_h,WIDTH-8,111);
    paragraph(13,image_h+8,body,WIDTH-26,5,Text);
  }
  int dots=landscape?image_w+14:14;
  for(int i=0;i<IntroScenes;++i)rounded_rect(dots+i*10,bottom-(landscape?43:16),5,5,i==intro_scene?Sage:Soft,1);
  auto next=intro_next_button();button(next.x,next.y,next.w,next.h,intro_skip?tr("CONFIRM SKIP","确认跳过"):intro_scene==IntroScenes-1?tr("WELCOME HOME","开始生活"):tr("NEXT / A","下一幕"));
}
