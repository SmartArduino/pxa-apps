// SPDX-License-Identifier: MIT
#include "model.hpp"
#include "../common/motion-games/runtime.hpp"
#include "../common/motion-games/sound.hpp"
namespace marble {
using namespace motion;
enum class Screen {home,play,pause,levels,settings,result};
enum Action {play_action=1,levels_action,settings_action,home_action,resume_action,calibrate_action,control_action,hints_action,restart_action,sensitivity_action,rotation_action,flip_action,exit_action,sound_action,next_action,previous_action,next_level_action,level_base=100};
struct App:Runtime<App> {
 static constexpr uint32_t frame_period_ms=16,sensor_period_ms=20;
 Sounds sounds;
 Profile profile;Maze board;Screen screen=Screen::home;
 bool saving=false,save_pending=false,dragging=false,was_active=false;Vec touch{};int origin_x=0,origin_y=0;
 float heading=0,celebrate=0,autosave=0,impact_cooldown=0;unsigned level_page=0;
 std::array<Rect,12> tiles{};std::array<bool,12> tile_visible{};
 static const char* id(){return "pxa-marble-maze";}static const char* ready_message(){return "Marble ready";}
 pxa::Task<void> load(){std::array<std::byte,Profile::size> data{};auto n=co_await ctx->storage().get("marble-v1",data);if(n&&*n==data.size())profile.decode(data);co_return pxa::Result<void>{};}
 void generate(unsigned level){Random map_random{0x72615631u+level*0x9e3779b9u};board.generate(level,w>h,map_random,h>w*3/2);}
 void prepare(){generate(profile.selected);if(profile.run&&!profile.restore(board)){profile.run=false;save();}}
 bool initial_motion()const{return profile.control==0;}bool initial_gyro()const{return false;}
 void started(){controls.focus=play_action;prepare();request_motion(false);}
 void save(){save_pending=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(save_pending){save_pending=false;auto data=profile.encode();auto result=co_await ctx->storage().set("marble-v1",data);if(!result){log("Marble save failed");break;}}saving=false;co_return pxa::Result<void>{};}
 void snapshot(){if(screen==Screen::play||screen==Screen::pause)profile.snapshot(board);save();}
 void show_home(){snapshot();request_motion(false);screen=Screen::home;controls.focus=play_action;dragging=false;touch={};prepare();dirty=true;}
 void start_motion(bool force=false){motion.rotation=profile.rotation;motion.flip_x=profile.flip;
   if(profile.control==0){if(force||!motion.calibrated){motion.calibrate();log("Marble calibration begin");}else log("Marble calibration reused");request_motion(true);}else request_motion(false);}
 void begin(bool restore=true){request_motion(false);generate(profile.selected);if(restore)profile.restore(board);
   screen=Screen::play;controls.focus=resume_action;heading=0;autosave=0;was_active=false;celebrate=0;dragging=false;touch={};
   start_motion();dirty=true;
   char text[100];std::snprintf(text,sizeof text,"Marble start level=%u restored=%u size=%dx%d",profile.selected+1,unsigned(restore&&profile.run),board.w,board.h);log(text);}
 void pause(){if(screen!=Screen::play)return;sounds.stop();profile.snapshot(board);save();request_motion(false);board.velocity={};touch={};dragging=false;screen=Screen::pause;controls.focus=resume_action;dirty=true;}
 void resume(bool recalibrate=false){screen=Screen::play;board.velocity={};was_active=false;last_tick=0;controls.focus=resume_action;
   start_motion(recalibrate);dirty=true;}
 void activate(int action){if(action!=sound_action)sounds.play(Cue::click);
   if(action>=level_base&&action<level_base+12){unsigned level=unsigned(action-level_base);if(level>profile.unlocked)return;
     if(level!=profile.selected)profile.run=false;profile.selected=level;save();begin(true);return;}
   switch(action){
   case play_action:begin(true);break;
   case home_action:show_home();break;
   case levels_action:request_motion(false);screen=Screen::levels;level_page=profile.selected/per_page();controls.focus=level_base+int(profile.selected);dirty=true;break;
   case settings_action:screen=Screen::settings;controls.focus=home_action;request_motion(false);dirty=true;break;
   case resume_action:if(screen==Screen::play)pause();else resume();break;
   case calibrate_action:profile.control=0;save();resume(true);break;
   case control_action:profile.control=(profile.control+1)%3;save();if(screen==Screen::pause)resume();else dirty=true;break;
   case hints_action:profile.hints=!profile.hints;save();if(screen==Screen::pause)resume();else dirty=true;break;
   case restart_action:profile.selected=board.level;profile.run=false;save();begin(false);break;
   case sensitivity_action:profile.sensitivity=(profile.sensitivity+1)%3;save();dirty=true;break;
   case rotation_action:profile.rotation=(profile.rotation+1)%4;motion.rotation=profile.rotation;save();dirty=true;break;
   case flip_action:profile.flip=!profile.flip;motion.flip_x=profile.flip;save();dirty=true;break;
   case sound_action:sounds.toggle();dirty=true;break;
    case exit_action:request_exit();break;
   case next_action:level_page=(level_page+1)%((12+per_page()-1)/per_page());controls.focus=level_base+int(level_page*per_page());dirty=true;break;
   case previous_action:if(level_page){--level_page;controls.focus=level_base+int(level_page*per_page());dirty=true;}break;
   case next_level_action:if(board.level<11){profile.selected=board.level+1;profile.run=false;save();begin(false);}else activate(levels_action);break;
   }
 }
 void key(uint32_t keys){using namespace pxa::input;if(keys&controller_b){(void)back();return;}
   if(screen==Screen::play){if(profile.control==1){if(keys&controller_up)heading-=.392699f;if(keys&controller_down)heading+=.392699f;dirty=true;}
     else if(keys&controller_up){board.velocity={};motion.calibrate();request_motion(profile.control==0);dirty=true;}
     if(keys&controller_a)pause();return;}
   if(keys&controller_up){controls.move(-1);dirty=true;}if(keys&controller_down){controls.move(1);dirty=true;}if(keys&controller_a)activate(controls.focus);
 }
 void pointer(pxa::input::PointerPhase phase,int x,int y){
   if(screen==Screen::play){if(phase==pxa::input::PointerPhase::down){controls.down(x,y);if(controls.hit(x,y)>=0)return;dragging=true;origin_x=x;origin_y=y;touch={};dirty=true;}
     else if(phase==pxa::input::PointerPhase::move&&dragging){touch={std::clamp((x-origin_x)/38.f,-1.f,1.f),std::clamp((y-origin_y)/38.f,-1.f,1.f)};dirty=true;}
     else if(phase==pxa::input::PointerPhase::up||phase==pxa::input::PointerPhase::cancel){bool was=dragging;dragging=false;touch={};dirty=true;int action=controls.up(x,y,phase==pxa::input::PointerPhase::cancel);if(!was&&action>=0)activate(action);}return;}
   if(phase==pxa::input::PointerPhase::down)controls.down(x,y);
   else if(phase==pxa::input::PointerPhase::up||phase==pxa::input::PointerPhase::cancel){int action=controls.up(x,y,phase==pxa::input::PointerPhase::cancel);if(action>=0)activate(action);
     else if(phase==pxa::input::PointerPhase::up&&screen==Screen::home&&home_area().contains(x,y)&&std::abs(x-controls.down_x)<12&&std::abs(y-controls.down_y)<12)begin(true);}
 }
 pxa::BackAction back(){if(screen==Screen::home)return pxa::BackAction::close;if(screen==Screen::play)pause();else if(screen==Screen::pause)resume();else show_home();return pxa::BackAction::stay;}
 void motion_changed(bool,bool status){dirty|=status;if(screen==Screen::play&&motion.calibrating)dirty=true;if(status&&!motion.calibrating&&motion.calibrated)log("Marble calibrated");}
 Rect board_area()const{int top=h<220?44:57,bottom=h-(h<220?22:29);return {10,top,w-20,std::max(35,bottom-top)};}
 Rect home_area()const{int top=h<220?43:65,bottom=h-(h<220?57:77);return {16,top,w-32,std::max(30,bottom-top)};}
 int cell_size(Rect area)const{return std::max(3,std::min((area.w-8)/board.w,(area.h-8)/board.h));}
 void tick(float dt){impact_cooldown=std::max(0.f,impact_cooldown-dt);if(celebrate>0){celebrate=std::max(0.f,celebrate-dt);dirty=true;}if(screen!=Screen::play)return;
   Vec tilt{};bool active=false;if(dragging){tilt=touch;active=true;}else if(profile.control==1){tilt={std::cos(heading)*.40f,std::sin(heading)*.40f};active=true;}
   else if(profile.control==0&&motion.calibrated&&motion.fresh(now)){tilt=motion.tilt();active=true;}
   dirty|=active!=was_active;was_active=active;if(!active)return;
   unsigned seconds=unsigned(board.elapsed),stars=board.collected;int cell=cell_size(board_area()),px=int(board.ball.x*cell),py=int(board.ball.y*cell);
   auto previous_velocity=board.velocity;board.tick(dt,tilt,profile.sensitivity==0?.65f:profile.sensitivity==1?1.f:1.4f);autosave+=dt;
   dirty|=seconds!=unsigned(board.elapsed)||stars!=board.collected||px!=int(board.ball.x*cell)||py!=int(board.ball.y*cell);
   if(!impact_cooldown&&((std::abs(previous_velocity.x)>.6f&&board.velocity.x==0)||(std::abs(previous_velocity.y)>.6f&&board.velocity.y==0))){sounds.play(Cue::impact);impact_cooldown=.3f;}
   if(stars!=board.collected){sounds.play(Cue::star,unsigned(std::popcount(board.collected)));celebrate=.45f;profile.snapshot(board);save();log("Marble star collected");}
   if(autosave>=15){autosave=0;profile.snapshot(board);save();}
   if(board.won){sounds.play(Cue::win);unsigned level=board.level;profile.finish(board);save();screen=Screen::result;controls.focus=next_level_action;request_motion(false);celebrate=.8f;dirty=true;char s[100];std::snprintf(s,sizeof s,"Marble won level=%u ms=%u",level+1,unsigned(board.elapsed*1000));log(s);}
 }
 void background(){if(screen==Screen::play)pause();else if(screen==Screen::pause)snapshot();dragging=false;touch={};}
 void foreground(){if(screen==Screen::play){screen=Screen::pause;controls.focus=resume_action;}request_motion(false);}
 unsigned per_page()const{return h<220?6:12;}
 static const char* theme(unsigned level){const char* names[]={"嫩叶小径","林间寻星","星光探险"};return names[std::min(2u,level/4)];}
 void time_text(char* out,size_t size,uint32_t ms)const{unsigned s=ms/1000;std::snprintf(out,size,"%02u:%02u",s/60,s%60);}
 void draw_board(Paint& p,Rect area,bool preview){int cell=cell_size(area),left=area.x+(area.w-cell*board.w)/2,top=area.y+(area.h-cell*board.h)/2;
   unsigned tier=std::min(2u,board.level/4);const uint16_t edge[]={rgb(194,215,199),rgb(191,213,223),rgb(208,198,223)},floor[]={rgb(245,241,218),rgb(235,244,241),rgb(245,235,246)},wall[]={rgb(112,157,140),rgb(115,154,179),rgb(151,130,178)},light[]={rgb(153,190,165),rgb(164,193,207),rgb(186,168,204)},shadow[]={rgb(76,128,114),rgb(82,119,145),rgb(114,92,144)};
   p.round({left-4,top-4,cell*board.w+8,cell*board.h+8},edge[tier],7);p.rect(left,top,cell*board.w,cell*board.h,floor[tier]);
   if(profile.hints&&!preview){for(unsigned i=1;i<board.route_size;++i){auto a=board.route[i-1],b=board.route[i];p.line({left+(a%board.w+.5f)*cell,top+(a/board.w+.5f)*cell},{left+(b%board.w+.5f)*cell,top+(b/board.w+.5f)*cell},std::max(1.f,cell*.10f),rgb(225,200,129));}}
   for(int y=0;y<board.h;++y)for(int x=0;x<board.w;){if(!board.wall(x,y)){++x;continue;}int first=x;while(x<board.w&&board.wall(x,y))++x;int xx=left+first*cell,yy=top+y*cell,ww=(x-first)*cell;
    p.rect(xx,yy,ww,cell,wall[tier]);p.rect(xx,yy,ww,std::max(1,cell/7),light[tier]);p.rect(xx,yy+cell-1,ww,1,shadow[tier]);}
   int gx=left+int((board.w-1.5f)*cell),gy=top+int((board.h-1.5f)*cell),gr=std::max(2,cell/3);
   p.circle(gx,gy,gr,board.collected==7?gold:rgb(197,197,174));p.circle(gx,gy,std::max(1,gr-2),board.collected==7?rgb(255,237,165):rgb(232,228,204));
   for(unsigned i=0;i<3;++i)if(!(board.collected&(1u<<i)))p.star(left+(board.stars[i]%board.w)*cell+cell/2,top+(board.stars[i]/board.w)*cell+cell/2,std::max(2,cell/3),gold);
   int bx=left+int(board.ball.x*cell),by=top+int(board.ball.y*cell),radius=std::max(2,int(Maze::radius*cell));p.marble(bx,by,radius);
   if(!preview&&profile.control==1)p.circle(bx+int(std::cos(heading)*cell*.7f),by+int(std::sin(heading)*cell*.7f),2,coral);
   if(!preview&&celebrate>0){for(unsigned i=0;i<7;++i){float angle=i*.897598f;int r=int((1-celebrate)*cell*.8f)+5;p.circle(bx+int(std::cos(angle)*r),by+int(std::sin(angle)*r),1,gold);}}
 }
 void add_toolbar(std::initializer_list<std::pair<int,std::string_view>> actions){int height=h<220?27:33,y=h-height-8,width=(w-20-6*(int(actions.size())-1))/int(actions.size()),x=10;
   for(auto a:actions){controls.add({x,y,width,height},a.first,a.second,a.first==play_action||a.first==next_level_action?pink:mint);x+=width+6;}}
 void draw(Paint& p){tile_visible.fill(false);const bool small=h<220;int header=small?25:32;char text[120];
   if(screen==Screen::home){p.label({12,3,w-24,header},"弹珠迷宫",ink,0,small?12:16);p.label({w-67,4,55,header},profile.control==0?"体感":profile.control==1?"按键":"触屏",teal,2,11);p.label({12,header,w-24,23},"滚向星星，发现小小世界",muted);
     draw_board(p,home_area(),true);std::snprintf(text,sizeof text,"第%02u关 · %s",profile.selected+1,theme(profile.selected));p.label({10,h-(small?59:74),w-20,24},text,teal);
     add_toolbar({{play_action,profile.run&&profile.run_level==profile.selected?"继续":"开始"},{levels_action,"选关"},{settings_action,"设置"}});
   }else if(screen==Screen::play){
     std::snprintf(text,sizeof text,"第%02u关",board.level+1);p.label({10,2,w-72,header},text,ink,0);controls.add({w-57,5,47,small?23:28},resume_action,"暂停",mint);
     unsigned n=unsigned(std::popcount(board.collected));std::snprintf(text,sizeof text,"星星 %u / 3",n);p.label({10,header-2,w/2,23},text,muted,0,small?11:13);
     char clock[24];time_text(clock,sizeof clock,uint32_t(board.elapsed*1000));p.label({w/2,header-2,w/2-12,23},clock,muted,2,small?11:13);
     draw_board(p,board_area(),false);
     std::string_view hint=profile.control==1?"上下转向，确认暂停":profile.control==2?"按住拖动，带弹珠前进":!acceleration?(connecting?"正在连接体感":"暂停可切换按键玩法"):motion.calibrating?"拿稳一会儿，正在校准":!motion.fresh(now)?"等待读数，已停住弹珠":board.collected==7?"金色出口打开啦":"收集三颗星，前往出口";
     p.label({8,h-(small?23:29),w-16,small?20:25},hint,teal,1,small?10:13);
     if(dragging){p.circle(origin_x,origin_y,3,rgb(147,188,162));p.circle(origin_x+int(touch.x*15),origin_y+int(touch.y*15),3,teal);}
   }else if(screen==Screen::levels){
     p.label({12,3,w-24,header},"选择关卡",ink,0,small?12:16);std::snprintf(text,sizeof text,"已点亮 %u / 12 关",unsigned(std::popcount(profile.completed)));p.label({12,header-1,w-24,22},text,muted);
     unsigned capacity=per_page(),cols=(w>h&&capacity==12)?4:3,rows=(capacity+cols-1)/cols;int top=small?46:57,cw=(w-20)/int(cols),ch=(h-top-48)/int(rows);
     for(unsigned i=0;i<capacity;++i){unsigned level=level_page*capacity+i;if(level>=12)break;Rect r{10+int(i%cols)*cw,top+int(i/cols)*ch,cw-6,ch-5};tiles[level]=r;tile_visible[level]=true;
       std::snprintf(text,sizeof text,"%02u",level+1);controls.add(r,level_base+int(level),text,profile.completed&(1u<<level)?mint:pink,level<=profile.unlocked,true);}
     if(capacity<12)add_toolbar({{next_action,level_page?"上一组":"下一组"},{home_action,"返回"}});else add_toolbar({{home_action,"返回首页"}});
   }else if(screen==Screen::settings||screen==Screen::pause){
     bool paused=screen==Screen::pause;p.label({12,3,w-88,header},paused?"弹珠歇一会儿":"冒险设置",ink,0,small?12:16);
     const char* control_names[]={"体感倾斜","按键转向","触屏拖动"};const char* speed_names[]={"轻柔","标准","灵敏"};
     int ids_pause[]={resume_action,calibrate_action,control_action,hints_action,restart_action,home_action};int ids_settings[]={home_action,control_action,sensitivity_action,rotation_action,flip_action,exit_action};
     int top=small?31:43,step=(h-top-9)/6;
     for(int i=0;i<6;++i){int action=paused?ids_pause[i]:ids_settings[i];switch(action){
       case resume_action:std::snprintf(text,sizeof text,"继续冒险");break;case calibrate_action:std::snprintf(text,sizeof text,"重新校准体感");break;
       case control_action:std::snprintf(text,sizeof text,"控制：%s",control_names[profile.control]);break;case hints_action:std::snprintf(text,sizeof text,"路线提示：%s",profile.hints?"开启":"关闭");break;
       case restart_action:std::snprintf(text,sizeof text,"重玩本关");break;case home_action:std::snprintf(text,sizeof text,"返回首页");break;
       case sensitivity_action:std::snprintf(text,sizeof text,"灵敏度：%s",speed_names[profile.sensitivity]);break;case rotation_action:std::snprintf(text,sizeof text,"方向旋转：%u度",profile.rotation*90);break;
       case flip_action:std::snprintf(text,sizeof text,"左右镜像：%s",profile.flip?"开启":"关闭");break;default:std::snprintf(text,sizeof text,"退出游戏");break;}
       controls.add({12,top+i*step,w-24,step-5},action,text,i==0?pink:i%2?mint:lilac);}
     controls.add({w-66,5,54,small?20:26},sound_action,sounds.label(true),mint);
   }else if(screen==Screen::result){
     p.label({10,3,w-20,header},board.level==11?"十二关，全点亮啦！":"星星收集成功！",teal,1,small?12:16);
     int sy=h<200?35:small?47:65;for(int i=0;i<3;++i)p.star(w/2+(i-1)*(small?27:38),sy,small?10:15,gold);
     char current[24],best[24];time_text(current,sizeof current,uint32_t(board.elapsed*1000));time_text(best,sizeof best,profile.best[board.level]);
     std::snprintf(text,sizeof text,"本次 %s · 最好 %s",current,best);p.label({10,sy+17,w-20,28},text,ink);
     int top=sy+53,step=std::max(22,(h-top-10)/3);controls.add({12,top,w-24,step-5},next_level_action,board.level<11?"下一关":"再选一关",pink);
     controls.add({12,top+step,w-24,step-5},restart_action,"再挑战一次",lilac);controls.add({12,top+2*step,w-24,step-5},home_action,"返回首页",mint);
   }
 }
 void overlay(Paint& p){if(screen!=Screen::levels)return;for(unsigned i=0;i<12;++i)if(tile_visible[i]){auto r=tiles[i];
   if(i>profile.unlocked){p.round({r.x+r.w-12,r.y+5,7,7},muted,2);p.round({r.x+r.w-11,r.y+2,5,6},muted,2);p.round({r.x+r.w-10,r.y+3,3,3},disabled,1);}
   if(profile.completed&(1u<<i)){if(r.h>=48&&profile.best[i]){char t[20];time_text(t,sizeof t,profile.best[i]);p.label({r.x+2,r.y+27,r.w-4,18},t,muted,1,10);}else for(int k=0;k<3;++k)p.star(r.x+r.w/2+(k-1)*9,r.y+r.h-8,3,gold);}
 }}
};
}
PXA_APPLICATION(marble::App)
