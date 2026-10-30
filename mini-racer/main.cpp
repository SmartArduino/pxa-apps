// SPDX-License-Identifier: MIT
#include "model.hpp"
#include "../common/motion-games/runtime.hpp"
#include "../common/motion-games/sound.hpp"
#include "render.hpp"
namespace racer {
using namespace motion;
enum class Screen {home,tracks,play,pause,settings,result};
enum Action {start_action=1,tracks_action,settings_action,home_action,resume_action,calibrate_action,control_action,restart_action,sensitivity_action,orientation_action,color_action,exit_action,sound_action,track_base=100};
struct App:Runtime<App> {
 Sounds sounds;
 Profile profile;Race race;RoadPainter road;Screen screen=Screen::home;
 bool saving=false,save_pending=false,touching=false,new_record=false;unsigned override_control=0,last_countdown=4,hud_speed=0;uint32_t hud_elapsed=0;uint64_t next_hud=0;float countdown=3,touch_steer=0,autosave=0,next_report=0;
 std::string pause_reason;std::array<Rect,3> track_tiles{};
 static const char* id(){return "pxa-mini-racer";}static const char* ready_message(){return "Racer ready";}
 static const char* track_name(unsigned i){const char* names[]={"晴空海岸","云上山路","星光夜城"};return names[std::min(i,2u)];}
 pxa::Task<void> load(){std::array<std::byte,Profile::size> data{};auto n=co_await ctx->storage().get("racer-v1",data);if(n&&*n==data.size())profile.decode(data);co_return pxa::Result<void>{};}
 bool initial_motion()const{return profile.control==0;}bool initial_gyro()const{return false;}
 void started(){race.begin(profile.track);if(profile.run)profile.restore(race,countdown);controls.focus=start_action;request_motion(false);}
 void save(){save_pending=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(save_pending){save_pending=false;auto data=profile.encode();auto status=co_await ctx->storage().set("racer-v1",data);if(!status){log("Racer save failed");break;}}saving=false;co_return pxa::Result<void>{};}
 bool use_motion()const{return profile.control==0&&override_control==0;}
 void connect_motion(){motion.rotation=profile.rotation;motion.flip_x=profile.mirror;if(use_motion()){motion.calibrate();request_motion(true);}else request_motion(false);}
 void begin(bool restore=true){race.begin(profile.track);countdown=3;if(restore)profile.restore(race,countdown);last_countdown=countdown>0?4:0;autosave=0;next_report=race.elapsed;screen=Screen::play;controls.focus=resume_action;override_control=profile.control;touching=false;touch_steer=0;pause_reason.clear();connect_motion();dirty=true;
   char text[100];std::snprintf(text,sizeof text,"Racer begin track=%u control=%u",profile.track+1,profile.control);log(text);}
 void snapshot(){if(screen==Screen::play||screen==Screen::pause){profile.snapshot(race,countdown);save();}}
 void home(){sounds.stop();snapshot();request_motion(false);screen=Screen::home;controls.focus=start_action;touching=false;touch_steer=0;race.begin(profile.track);if(profile.run)profile.restore(race,countdown);dirty=true;}
 void pause(const char* reason=""){if(screen!=Screen::play)return;sounds.stop();snapshot();screen=Screen::pause;controls.focus=resume_action;request_motion(false);touching=false;touch_steer=0;pause_reason=reason;dirty=true;log("Racer paused");}
 void resume(){screen=Screen::play;controls.focus=resume_action;touching=false;touch_steer=0;last_tick=0;connect_motion();dirty=true;}
 void activate(int action){if(action!=sound_action)sounds.play(Cue::click);if(action>=track_base&&action<track_base+3){if(profile.track!=unsigned(action-track_base))profile.run=false;profile.track=unsigned(action-track_base);save();begin();return;}
   switch(action){
    case start_action:begin();break;
    case restart_action:profile.run=false;save();begin(false);break;
    case home_action:home();break;
    case tracks_action:request_motion(false);screen=Screen::tracks;controls.focus=track_base+int(profile.track);dirty=true;break;
    case settings_action:request_motion(false);screen=Screen::settings;controls.focus=home_action;dirty=true;break;
    case resume_action:if(screen==Screen::play)pause();else resume();break;
    case calibrate_action:profile.control=0;override_control=0;save();resume();break;
    case control_action:profile.control=(profile.control+1)%3;override_control=profile.control;save();if(screen==Screen::pause)resume();else dirty=true;break;
    case sensitivity_action:profile.sensitivity=(profile.sensitivity+1)%3;save();dirty=true;break;
    case orientation_action:profile.rotation=(profile.rotation+1)%4;if(!profile.rotation)profile.mirror=!profile.mirror;save();dirty=true;break;
    case color_action:profile.color=(profile.color+1)%3;save();dirty=true;break;
    case sound_action:sounds.toggle();dirty=true;break;
    case exit_action:request_exit();break;
   }
 }
 void key(uint32_t keys){using namespace pxa::input;if(keys&controller_b){if(back()==pxa::BackAction::close)request_exit();return;}
   if(screen==Screen::play){if(keys&(controller_up|controller_down|controller_left|controller_right)){override_control=1;if(profile.control!=1){profile.control=1;save();}request_motion(false);dirty=true;}if(keys&controller_a)pause();return;}
   if(keys&controller_up){controls.move(-1);dirty=true;}if(keys&controller_down){controls.move(1);dirty=true;}if(keys&controller_a)activate(controls.focus);
 }
 Rect footer()const{return {8,h-(h<220?29:37),w-16,h<220?23:30};}
 void pointer(pxa::input::PointerPhase phase,int x,int y){
   if(screen==Screen::play){auto f=footer();bool footer_pause=f.contains(x,y)&&x>w/3&&x<w*2/3;
     if(phase==pxa::input::PointerPhase::down){controls.down(x,y);if(controls.hit(x,y)>=0||footer_pause){touching=false;return;}touching=true;override_control=2;if(profile.control!=2){profile.control=2;save();}request_motion(false);touch_steer=x<w/2?-1.f:1.f;dirty=true;}
     else if(phase==pxa::input::PointerPhase::move&&touching){touch_steer=std::clamp((x-w*.5f)/(w*.28f),-1.f,1.f);dirty=true;}
     else if(phase==pxa::input::PointerPhase::up||phase==pxa::input::PointerPhase::cancel){bool was=touching;touching=false;touch_steer=0;dirty=true;int action=controls.up(x,y,phase==pxa::input::PointerPhase::cancel);
       if(!was&&phase==pxa::input::PointerPhase::up){if(action>=0)activate(action);else if(footer_pause&&std::abs(x-controls.down_x)<12&&std::abs(y-controls.down_y)<12)pause();}}return;}
   if(phase==pxa::input::PointerPhase::down)controls.down(x,y);
   else if(phase==pxa::input::PointerPhase::up||phase==pxa::input::PointerPhase::cancel){int action=controls.up(x,y,phase==pxa::input::PointerPhase::cancel);if(action>=0)activate(action);}
 }
 pxa::BackAction back(){if(screen==Screen::home)return pxa::BackAction::close;if(screen==Screen::play)pause();else if(screen==Screen::pause)resume();else home();return pxa::BackAction::stay;}
 void motion_changed(bool,bool status){dirty|=status;if(screen==Screen::play&&motion.calibrating)dirty=true;if(status&&!motion.calibrating&&motion.calibrated)log("Racer calibrated");}
 void tick(float dt){if(screen!=Screen::play)return;dirty=true;
   if((controller.buttons()&(pxa::input::controller_up|pxa::input::controller_down|pxa::input::controller_left|pxa::input::controller_right))&&override_control!=1){override_control=1;if(profile.control!=1){profile.control=1;save();}request_motion(false);}
   if(use_motion()){
     if(!acceleration){if(connecting)return;override_control=1;request_motion(false);log("Racer fallback keys");}
     else{if(motion.calibrating)return;if(!motion.fresh(now)){pause("体感读数中断，已暂停");return;}}
   }
   if(countdown>0){unsigned number=unsigned(std::ceil(countdown));if(number!=last_countdown){sounds.play(Cue::countdown,number);last_countdown=number;}float before=countdown;countdown=std::max(0.f,countdown-dt);if(before>0&&!countdown){sounds.play(Cue::go);last_countdown=0;log("Racer go");}return;}
   float steer=0;using namespace pxa::input;uint32_t buttons=controller.buttons();if(touching)steer=touch_steer;
   else if(override_control==1||profile.control==1){steer=((buttons&(controller_down|controller_right))?1.f:0.f)-((buttons&(controller_up|controller_left))?1.f:0.f);}
   else if(use_motion())steer=motion.tilt().x;
   unsigned pads=race.pads,hits=race.collisions,lap=race.lap();race.tick(dt,steer,profile.sensitivity==0?.70f:profile.sensitivity==1?1.f:1.35f);
   if(now>=next_hud){hud_speed=unsigned(race.speed*3.6f);hud_elapsed=uint32_t(race.elapsed*1000);next_hud=now+100000;}
   autosave+=dt;if(autosave>=15){autosave=0;snapshot();}
   if(race.elapsed>=next_report){next_report=race.elapsed+1;char text[140];std::snprintf(text,sizeof text,"Racer state distance_mm=%u x_milli=%d speed_mms=%u elapsed_ms=%u",unsigned(race.distance*1000),int(race.x*1000),unsigned(race.speed*1000),unsigned(race.elapsed*1000));log(text);}
   sounds.engine(unsigned(race.speed),now);
   if(race.pads!=pads){sounds.play(Cue::boost);log("Racer boost");}if(race.collisions!=hits){sounds.play(Cue::impact);log("Racer collision");}if(race.lap()!=lap){sounds.play(Cue::lap);log("Racer lap 2");}
   if(race.finished){sounds.stop_engine();sounds.play(Cue::win);new_record=!profile.best[profile.track]||race.elapsed*1000<profile.best[profile.track];profile.finish(race);save();screen=Screen::result;controls.focus=restart_action;request_motion(false);
     char text[100];std::snprintf(text,sizeof text,"Racer finished track=%u rank=%u ms=%u",profile.track+1,race.rank(),unsigned(race.elapsed*1000));log(text);}
 }
 void background(){if(screen==Screen::play)pause();else if(screen==Screen::pause)snapshot();touching=false;touch_steer=0;}
 void foreground(){if(screen==Screen::play){screen=Screen::pause;controls.focus=resume_action;}request_motion(false);}
 void clock_text(char* out,size_t n,uint32_t ms)const{unsigned tenths=ms/100;std::snprintf(out,n,"%02u:%02u.%u",tenths/600,(tenths/10)%60,tenths%10);}
 void toolbar(std::initializer_list<std::pair<int,std::string_view>> actions){auto f=footer();int x=f.x,width=(f.w-6*(int(actions.size())-1))/int(actions.size());
   for(auto a:actions){controls.add({x,f.y,width,f.h},a.first,a.second,a.first==start_action?pink:mint);x+=width+6;}}
 uint16_t background_color()const{return screen==Screen::home||screen==Screen::play?palettes[race.track.id].grass:paper;}
 void draw(Paint& p){bool small=h<220;int heading=small?25:32;char text[128];
   if(screen==Screen::home||screen==Screen::play){bool preview=screen==Screen::home;int top=preview?(small?48:65):(small?45:54),bottom=footer().y-(preview?(small?25:30):4);p.rect(0,0,w,top,paper);p.rect(0,bottom,w,h-bottom,paper);}
   if(screen==Screen::home){p.label({12,3,w-24,heading},"3D小赛车",ink,0,small?12:16);p.label({w-63,4,51,heading},profile.control==0?"体感":profile.control==1?"按键":"触屏",teal,2,11);
     p.label({12,heading,w-24,small?20:25},"把快乐开向远方",muted,0,small?10:13);int top=small?48:65,bottom=footer().y-(small?25:30);road.draw(p,race,{0,top,w,std::max(25,bottom-top)},profile.color,true);
     p.label({10,bottom,w-20,small?23:28},track_name(profile.track),teal,1,small?12:16);toolbar({{start_action,profile.run&&profile.run_track==profile.track?"继续比赛":"开始比赛"},{tracks_action,"赛道"},{settings_action,"设置"}});
   }else if(screen==Screen::play){int road_top=small?45:54;auto f=footer();road.draw(p,race,{0,road_top,w,f.y-road_top-4},profile.color,false,race.elapsed+3-countdown);
     std::snprintf(text,sizeof text,"%u",hud_speed);p.label({10,1,small?34:52,heading},text,ink,0,small?17:24);p.label({small?45:64,small?5:9,small?25:35,18},"km/h",muted,0,small?8:10);
     Rect rank_chip{w/2-5,5,w/2-55,heading-8};p.round(rank_chip,race.rank()==1?rgb(250,227,165):mint,7);std::snprintf(text,sizeof text,"第%u名",race.rank());p.label(rank_chip,text,teal,1,small?8:12);controls.add({w-49,5,41,small?22:27},resume_action,"暂停",mint);
     std::snprintf(text,sizeof text,"第 %u / 2 圈",race.lap());p.label({10,heading-1,w/2,20},text,muted,0,small?10:12);
     char clock[30];clock_text(clock,sizeof clock,hud_elapsed);p.label({w/2,heading-1,w/2-10,20},clock,muted,2,small?10:12);p.rect(0,road_top-3,w,2,rgb(207,225,218));p.rect(0,road_top-3,int(w*race.progress()),2,teal);
     int third=(f.w-12)/3;bool left=touching?touch_steer<-.1f:controller.buttons()&(pxa::input::controller_up|pxa::input::controller_left),right=touching?touch_steer>.1f:controller.buttons()&(pxa::input::controller_down|pxa::input::controller_right);
     p.round({f.x,f.y,third,f.h},left?rgb(134,214,188):mint,6);p.label({f.x,f.y,third,f.h},"左转",ink);p.round({f.x+third+6,f.y,third,f.h},pink,6);p.label({f.x+third+6,f.y,third,f.h},"暂停",ink);p.round({f.x+2*(third+6),f.y,third,f.h},right?rgb(134,214,188):mint,6);p.label({f.x+2*(third+6),f.y,third,f.h},"右转",ink);
     if(use_motion()&&(!acceleration||motion.calibrating)){int yy=road_top+(f.y-road_top)/2-21;p.round({12,yy,w-24,44},paper,8);p.label({18,yy+1,w-36,23},"拿稳设备，准备出发",teal,1,small?11:14);p.label({18,yy+22,w-36,20},"按方向键也能开始",muted,1,small?10:12);}
     else if(countdown>0){int size=small?40:58,yy=road_top+(f.y-road_top-size)/2;p.round({w/2-size/2,yy,size,size},paper,9);std::snprintf(text,sizeof text,"%u",unsigned(std::ceil(countdown)));p.label({w/2-size/2,yy,size,size},text,teal,1,small?25:36);}
     else if(race.boost>0)p.label({10,road_top+4,w-20,22},"加速带，冲呀！",gold,1,small?12:16);
   }else if(screen==Screen::tracks){p.label({12,3,w-24,heading},"选一条风景赛道",ink,0,small?12:16);int top=small?33:48,step=(footer().y-top-5)/3;
     for(unsigned i=0;i<3;++i){track_tiles[i]={12,top+int(i)*step,w-24,step-7};controls.add(track_tiles[i],track_base+int(i),track_name(i),i==0?mint:i==1?pink:lilac,true,step>=48);}
     toolbar({{home_action,"返回首页"}});
   }else if(screen==Screen::pause||screen==Screen::settings){bool paused=screen==Screen::pause;p.label({12,3,w-88,heading},paused?"小赛车歇一会儿":"车库设置",ink,0,small?12:16);
     int top=small?32:46;if(paused&&!pause_reason.empty()){p.label({12,heading,w-24,20},pause_reason,coral,0,small?9:11);top+=17;}
     int step=(h-top-8)/6;int settings[]={home_action,control_action,sensitivity_action,orientation_action,color_action,exit_action};int pause_ids[]={resume_action,calibrate_action,control_action,restart_action,home_action,exit_action};
     const char* modes[]={"体感优先","按键转向","触屏转向"};const char* speeds[]={"轻柔","标准","灵敏"};const char* colors[]={"珊瑚红","晴空蓝","星光紫"};
     for(int i=0;i<6;++i){int action=paused?pause_ids[i]:settings[i];switch(action){
       case home_action:std::snprintf(text,sizeof text,"返回首页");break;case resume_action:std::snprintf(text,sizeof text,"继续比赛");break;case calibrate_action:std::snprintf(text,sizeof text,"重新校准体感");break;
       case control_action:std::snprintf(text,sizeof text,"控制：%s",modes[profile.control]);break;case sensitivity_action:std::snprintf(text,sizeof text,"灵敏度：%s",speeds[profile.sensitivity]);break;
       case orientation_action:std::snprintf(text,sizeof text,"方向：%u度%s",profile.rotation*90,profile.mirror?"·镜像":"");break;case color_action:std::snprintf(text,sizeof text,"车身：%s",colors[profile.color]);break;
       case restart_action:std::snprintf(text,sizeof text,"重新比赛");break;default:std::snprintf(text,sizeof text,"退出游戏");break;}
       controls.add({12,top+i*step,w-24,step-5},action,text,i==0?pink:i%2?mint:lilac);}
     controls.add({w-66,5,54,small?20:26},sound_action,sounds.label(true),mint);
   }else if(screen==Screen::result){p.label({12,3,w-24,heading},new_record?"冲线啦，新纪录！":"完成一场好风景",teal,1,small?12:16);int sy=small?35:62;unsigned stars=race.rank()==1?3:race.rank()==2?2:1;
     p.round({12,sy+10,w-24,small?40:46},rgb(233,241,226),8);for(unsigned i=0;i<3;++i)p.star(w/2+(int(i)-1)*(small?25:34),sy,small?9:13,i<stars?gold:rgb(219,221,205));
     char current[30],best[30];clock_text(current,sizeof current,uint32_t(race.elapsed*1000));clock_text(best,sizeof best,profile.best[profile.track]);std::snprintf(text,sizeof text,"第%u名 · %s",race.rank(),current);p.label({10,sy+13,w-20,23},text,ink,1,small?11:16);
     std::snprintf(text,sizeof text,"最好 %s",best);p.label({10,sy+(small?31:35),w-20,small?17:20},text,muted,1,small?10:13);int top=sy+(small?51:59),step=(h-top-9)/3;
     controls.add({12,top,w-24,step-5},restart_action,"再跑一次",pink);controls.add({12,top+step,w-24,step-5},tracks_action,"换条赛道",lilac);controls.add({12,top+2*step,w-24,step-5},home_action,"返回首页",mint);
   }
 }
 void overlay(Paint& p){if(screen!=Screen::tracks)return;for(unsigned i=0;i<3;++i){auto r=track_tiles[i];int size=std::min(26,r.h-8),x=r.x+6,y=r.y+4;auto theme=palettes[i];p.round({x,y,size,size},theme.sky,4);p.quad({Point{float(x),float(y+size)},Point{float(x+size/2),float(y+size/3)},Point{float(x+size),float(y+size)},Point{float(x+size),float(y+size)}},theme.mountain);p.quad({Point{float(x+size/4),float(y+size)},Point{float(x+size/2-1),float(y+size/2)},Point{float(x+size/2+1),float(y+size/2)},Point{float(x+size*3/4),float(y+size)}},theme.road);p.circle(x+size*3/4,y+size/4,2,theme.accent);if(r.h<44)continue;char text[64],clock[30];if(profile.best[i]){clock_text(clock,sizeof clock,profile.best[i]);std::snprintf(text,sizeof text,"最好 %s · 第%u名",clock,unsigned(profile.best_rank[i]));}else std::snprintf(text,sizeof text,"等你创造第一份纪录");p.label({r.x+6,r.y+28,r.w-12,r.h-30},text,muted,1,11);}}
};
}
PXA_APPLICATION(racer::App)
