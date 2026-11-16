// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/surface.hpp>
#include <pxa/audio.hpp>
#include <pxa/permission.hpp>
#include <vector>
#include <cstring>
#include "selection.hpp"
#include "ui.hpp"
#include "hud_layout.hpp"
#include "save.hpp"
#ifdef IRON_PROFILE
#include "profile.hpp"
#endif
#include "../common/motion-games/sound.hpp"
namespace iron {
enum class Screen {home,campaign,briefing,battle,pause,result,help};
enum Action {start=1,resume,missions,help,home,exit_game,begin,pausing,restart,next,select_army,base,units,buildings,repair,strike,zooming,cancel,save_return,sound_toggle,difficulty,box_select,stop_army,mission0=50,produce0=100};
constexpr const char* chapter_names[]={"灰烬里的电台","穿越静默之河","黎明之前"};
constexpr const char* chapter_places[]={"01 / 北境 · 赤砂前哨","02 / 河谷 · 第七中继站","03 / 灰塔 · 核心禁区"};
constexpr const char* objective[]={"摧毁东南方的灰塔指挥中心","占领中继站，保护撤离 90 秒","击毁敌方电站，摧毁灰塔指挥中心"};
struct Button {Rect r;int action;};
struct App {
 pxa::Context* ctx=nullptr;pxa::ui::DisplayMetrics display;std::optional<pxa::Surface> surface;std::vector<uint16_t> pixels;std::vector<uint16_t> column_map;NativeText native_text;
 motion::Sounds sounds;SelectionTool selection;uint8_t pointer_owner=0;World world;Career career;Battlefield field;Screen screen=Screen::home;std::array<Button,32> buttons{};int button_count=0;
 int w=480,h=320,tab=0,placing=-1,focus_action=start;bool exit_pending=false;int last_selected_id=-1;float last_tap=-10;bool ready=false,has_run=false,skirmish=false,pressing=false,dragging=false,exiting=false,saving=false,pending_save=false;uint64_t last_tick=0,frame_id=0,now=0;float clock=0,save_flash=0,sound_cd=0;int downx=0,downy=0,lastx=0,lasty=0;Vec pointer_world{};Save save_buffer{};uint32_t save_bank=0;
 void log(std::string_view text){(void)ctx->log().write(pxa::LogLevel::info,text);}
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){ctx=&c;if(auto d=pxa::ui::decode_start_display(config))display=*d;(void)c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);return c.tasks().start(initialize());}
 pxa::Task<void> initialize(){
  for(int bank=0;bank<2;++bank){const char* path=bank?"iron-dawn-1.save":"iron-dawn-0.save";auto opened=co_await ctx->fs().open(path);if(!opened)continue;auto bytes=std::as_writable_bytes(std::span{&save_buffer,1});size_t at=0;while(at<bytes.size()){auto got=opened->read(bytes.subspan(at,std::min(size_t(4096),bytes.size()-at)));if(!got||!*got)break;at+=*got;}(void)opened->close();
   if(at==bytes.size()&&((save_buffer.magic==0x49445734&&save_buffer.version==4)||(save_buffer.magic==0x49445735&&save_buffer.version==5))&&save_buffer.checksum==checksum(save_buffer)&&valid_save(save_buffer)&&save_buffer.generation>=save_bank){career=save_buffer.career;has_run=save_buffer.has_run;if(has_run)world=save_buffer.world;save_bank=save_buffer.generation;}
  }
  if(save_bank)log("Iron Dawn restored");
  auto s=co_await sounds.open(*ctx);if(!s)log("Iron Dawn: sound unavailable");
  w=480;h=std::clamp(int(uint32_t(display.height)*w/std::max(1u,uint32_t(display.width))),320,1060);if(display.width!=unsigned(w)||display.height!=unsigned(h))pixels.resize(w*h);column_map.resize(display.width);for(unsigned x=0;x<display.width;++x)column_map[x]=x*w/display.width;
  auto created=co_await ctx->surface().create_mapped({uint16_t(display.width),uint16_t(display.height),2,true,2097152});if(!created){char err[80];std::snprintf(err,sizeof err,"Iron Dawn surface create error=%d",int(created.error()));log(err);co_return std::unexpected(created.error());}surface.emplace(std::move(*created));
  auto configured=co_await surface->configure({0,0,uint16_t(display.width),uint16_t(display.height),0,true});if(!configured)co_return configured;
  field.width=w;field.height=h;field.top=50;field.bottom=HudLayout(h).top;field.zoom=1;
  if(!has_run)world.init(0);field.focus({8,10});ready=true;(void)ctx->clock().set_period(100);log("Iron Dawn ready");render();co_return pxa::Result<void>{};
 }
 void save(){pending_save=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(pending_save){pending_save=false;save_buffer={};save_buffer.career=career;save_buffer.has_run=has_run;save_buffer.world=world;save_buffer.generation=save_bank+1;save_buffer.checksum=checksum(save_buffer);auto bytes=std::as_bytes(std::span{&save_buffer,1});
   auto opened=co_await ctx->fs().open("iron-dawn.tmp",pxa::OpenMode::write|pxa::OpenMode::create|pxa::OpenMode::truncate);if(!opened){log("Iron Dawn save open failed");break;}size_t at=0;
   while(at<bytes.size()){auto wrote=opened->write(bytes.subspan(at,std::min(size_t(4096),bytes.size()-at)));if(!wrote||!*wrote)break;at+=*wrote;}auto closed=opened->close();
   if(at==bytes.size()&&closed){auto committed=co_await ctx->fs().replace("iron-dawn.tmp",save_buffer.generation%2?"iron-dawn-1.save":"iron-dawn-0.save");if(committed){save_bank=save_buffer.generation;save_flash=1.5f;log("Iron Dawn saved");}else log("Iron Dawn save publish failed");}else log("Iron Dawn save write failed");
  }saving=false;co_return pxa::Result<void>{};}

 void new_mission(int ch){world.init(ch);if(career.easy){world.credits+=600;for(auto& a:world.e)if(a.hp>0&&a.team==0)a.hp*=1.35f;}field.focus({8,10});placing=-1;tab=0;screen=Screen::briefing;has_run=true;skirmish=false;(void)ctx->clock().set_period(100);save();}
 void report(){char s[200];std::snprintf(s,sizeof s,"Iron Dawn state chapter=%d screen=%d time=%u credits=%d army=%d enemy=%d queue=%d power=%d selected=%d won=%d defeated=%d",world.chapter,int(screen),unsigned(world.time),world.credits,world.army(),world.army(1),world.job_count,world.power_net,world.selected(),world.won,world.defeated);log(s);}
 void finish(){selection.cancel();pressing=false;dragging=false;screen=Screen::result;has_run=false;if(world.won){career.unlocked=std::max(career.unlocked,unsigned(std::min(2,world.chapter+1)));career.wins|=1u<<world.chapter;auto& best=career.best[world.chapter];uint32_t seconds=std::max(1u,unsigned(world.time));if(!best||seconds<best)best=seconds;sounds.play(motion::Cue::win);}else sounds.play(motion::Cue::impact);placing=-1;(void)ctx->clock().set_period(100);save();report();}
 void action(int a){if(a!=box_select)selection.cancel();sounds.play(motion::Cue::click);if(a>=mission0&&a<mission0+3){if(unsigned(a-mission0)<=career.unlocked)new_mission(a-mission0);return;}
  if(a>=produce0&&a<produce0+12){int k=a-produce0;if(k>=hq){if(auto b=world.blocked(k)){world.say(b);return;}placing=k;world.say("点击基地附近空地部署；点取消退出");}else world.queue(k);report();return;}
  switch(a){case start:new_mission(std::min(2u,career.unlocked));break;case resume:screen=Screen::battle;placing=-1;last_tick=0;break;case missions:screen=Screen::campaign;break;case help:screen=Screen::help;break;case home:screen=Screen::home;placing=-1;break;
   case begin:screen=Screen::battle;world.say("先造兵，再点全军；点击战场下达移动或攻击。",9);world.select_all();save();break;
   case pausing:screen=Screen::pause;placing=-1;save();report();break;case restart:new_mission(world.chapter);break;case next:if(world.chapter<2)new_mission(world.chapter+1);else screen=Screen::campaign;break;
   case box_select:if(selection.armed){selection.cancel();world.say("框选已取消");}else {selection.armed=true;placing=-1;last_selected_id=-1;world.say("拖动框选作战单位，松手后点地面移动。",6);}break;
   case stop_army:world.stop_selected();report();break;
   case select_army:world.select_all();placing=-1;break;case base:field.focus({6,9});break;case units:tab=0;placing=-1;break;case buildings:tab=1;placing=-1;break;case repair:world.repair();break;case strike:placing=99;world.say("点亮区内目标：空袭 180 / 冷却 45 秒");break;
   case zooming:{Vec p=field.unproject(w/2,(field.top+field.bottom)/2);field.zoom=field.zoom>.95f?.72f:field.zoom<.8f?1.3f:1.f;field.focus(p);}break;case cancel:if(placing>=0)placing=-1;else world.cancel_job();break;
   case save_return:save();screen=Screen::home;break;case sound_toggle:sounds.toggle();break;case difficulty:career.easy=!career.easy;save();break;case exit_game:save();exit_pending=true;break;
  }
  (void)ctx->clock().set_period(screen==Screen::battle?33:100);
 }
 Rect selection_button()const{return {w-90,field.top+96,80,28};}
 Rect battlefield()const{return {0,field.top,w,field.bottom-field.top};}
 Rect minimap()const{return {w-90,field.top+8,80,80};}
 bool map_click(int x,int y){auto m=minimap();if(!m.has(x,y))return false;field.focus({std::clamp((x-m.x)*N/float(m.w),1.f,26.f),std::clamp((y-m.y)*N/float(m.h),1.f,26.f)});return true;}
 int hit_button(int x,int y)const{for(int i=button_count-1;i>=0;--i)if(buttons[i].r.has(x,y))return buttons[i].action;return -1;}
 void on_pointer(pxa::Context&,const pxa::input::Pointer& p){
  if(!ready)return;
  int x=pxa::ui::canvas_to_surface_coordinate(p.x,display)*w/std::max(1,int(display.width));
  int y=pxa::ui::canvas_to_surface_coordinate(p.y,display)*h/std::max(1,int(display.height));
  using P=pxa::input::PointerPhase;
  if(p.action()==P::down){
   if(pressing)return; // A second finger cannot steal an in-progress selection.
   pointer_owner=p.pointer_id;pressing=true;dragging=false;downx=lastx=x;downy=lasty=y;pointer_world=field.unproject(x,y);
   if(screen==Screen::battle)selection.begin(x,y,battlefield(),minimap(),selection_button(),hit_button(x,y)>=0,placing>=0);
   return;
  }
  if(!pressing||p.pointer_id!=pointer_owner)return;
  if(p.action()==P::cancel){pressing=false;dragging=false;selection.cancel();return;}
  if(p.action()==P::move){
   if(std::abs(x-downx)+std::abs(y-downy)>7)dragging=true;
   if(selection.tracking)selection.move(x,y);
   else if(dragging&&screen==Screen::battle&&battlefield().has(downx,downy)&&hit_button(downx,downy)<0&&!minimap().has(downx,downy)){
    field.camx-=x-lastx;field.camy-=y-lasty;clamp_camera();
   }
   lastx=x;lasty=y;pointer_world=field.unproject(x,y);return;
  }
  if(p.action()!=P::up)return;
  pressing=false;
  if(selection.tracking){
   selection.move(x,y);int n=selection.commit(world,field);dragging=false;last_selected_id=-1;
   char message[80];if(n)std::snprintf(message,sizeof message,"已框选 %d 个单位 · 点击地面移动 / 敌军集火",n);else std::snprintf(message,sizeof message,"选区内没有作战单位");world.say(message,5);
   report_selection();report();render();return;
  }
  if(dragging){last_selected_id=-1;return;}
  int a=hit_button(x,y);if(a>=0&&a==hit_button(downx,downy)){action(a);render();return;}
  if(screen!=Screen::battle||y<field.top||y>=field.bottom)return;
  if(map_click(x,y))return;
  Vec pos=field.unproject(x,y);pos.x=std::clamp(pos.x,1.f,26.8f);pos.y=std::clamp(pos.y,1.f,26.8f);
  if(placing==99){if(world.strike(pos))placing=-1;}
  else if(placing>=0){if(world.place(placing,pos))placing=-1;}
  else {
   int id=world.hit(pos,.95f);
   if(id>=0&&world.e[id].team==0){
    if(id==last_selected_id&&clock-last_tap<.4f){int kind=world.e[id].kind;for(auto& unit:world.e)unit.selected=unit.hp>0&&unit.team==0&&unit.kind==kind;world.say("已编组全部同型部队");}
    else {world.select_one(id);world.say(world.e[id].kind==factory||world.e[id].kind==barracks?"点击空地设置新部队集结点":specs[world.e[id].kind].name);}
    last_selected_id=id;last_tap=clock;
   }else if(world.selected())world.order(pos,id>=0&&world.e[id].team==1?id:-1);
   else world.say("点选己方部队，或使用框选和全军");
  }
  report();
 }
 void report_selection(){char line[512];int used=std::snprintf(line,sizeof line,"Iron Dawn selection ids=");for(int i=0;i<MAX;++i)if(world.e[i].hp>0&&world.e[i].selected&&used<int(sizeof line)-6)used+=std::snprintf(line+used,sizeof line-used,"%d,",i);log(line);}
 void clamp_camera(){field.camx=std::clamp(field.camx,-420.f*field.zoom,420.f*field.zoom);field.camy=std::clamp(field.camy,40.f*field.zoom,520.f*field.zoom);}
 pxa::BackAction on_back(){if(exiting)return pxa::BackAction::close;if(screen==Screen::battle){if(selection.armed||selection.tracking){selection.cancel();pressing=false;dragging=false;world.say("框选已取消");return pxa::BackAction::stay;}action(pausing);return pxa::BackAction::stay;}if(screen==Screen::pause){action(resume);return pxa::BackAction::stay;}if(screen==Screen::home){save();exit_pending=true;return pxa::BackAction::stay;}screen=Screen::home;return pxa::BackAction::stay;}
 void on_controller(pxa::Context&,const pxa::input::ControllerState& s){using namespace pxa::input;static uint32_t prior=0;uint32_t pressed=s.buttons&~prior;prior=s.buttons;if(!ready)return;if(pressed&controller_b){(void)on_back();return;}if(screen==Screen::battle){if(selection.tracking&&!(pressed&controller_start))return;if(pressed&controller_a)action(select_army);if(pressed&controller_x)action(stop_army);if(pressed&controller_start)action(pausing);if(s.buttons&controller_left)field.camx-=16;if(s.buttons&controller_right)field.camx+=16;if(s.buttons&controller_up)field.camy-=16;if(s.buttons&controller_down)field.camy+=16;clamp_camera();}else {if(pressed&controller_up)focus_action=button_count?buttons[std::max(0,button_index()-1)].action:start;if(pressed&controller_down)focus_action=button_count?buttons[std::min(button_count-1,button_index()+1)].action:start;if(pressed&controller_a)action(focus_action);}}
 int button_index()const{for(int i=0;i<button_count;++i)if(buttons[i].action==focus_action)return i;return 0;}
 pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e){sounds.event(e);auto tick=pxa::decode_clock_tick(e);if(!tick)return false;now=tick->timestamp_us;float dt=last_tick?std::clamp(float(now-last_tick)*1e-6f,0.f,.25f):.05f;last_tick=now;if(!ready||!ctx->foreground())return true;clock+=dt;field.clock=clock;save_flash=std::max(0.f,save_flash-dt);sound_cd-=dt;
  if(screen==Screen::battle){for(float left=dt;left>.001f;left-=.05f)world.tick(std::min(left,.05f));if(world.sound&&sound_cd<=0){sounds.play(world.sound==5?motion::Cue::settle:world.sound==4?motion::Cue::star:world.sound==3?motion::Cue::impact:world.sound==2?motion::Cue::dice:motion::Cue::click);world.sound=0;sound_cd=.3f;}if(world.won||world.defeated)finish();if(world.autosave>=12){world.autosave=0;save();report();}}
  render();if(exit_pending&&!saving){exiting=true;(void)ctx->window().request_back();}return true;}
 void on_background(pxa::Context&){selection.cancel();pressing=false;dragging=false;if(screen==Screen::battle)screen=Screen::pause;sounds.suspend();save();(void)ctx->clock().set_period(0);last_tick=0;}
 void on_foreground(pxa::Context&){sounds.resume();(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);last_tick=0;if(ready)(void)ctx->clock().set_period(screen==Screen::battle?33:100);}
 void button(Raster& r,Rect area,int a,std::string_view label,bool primary=false,bool enabled=true){
  if(button_count<int(buttons.size())&&enabled)buttons[button_count++]={area,a};bool held=pressing&&!dragging&&area.has(downx,downy);Skin::button(r,area,primary,held,enabled);
  UiIcon icon=UiIcon::none;if(display.width>=480)switch(a){case pausing:icon=UiIcon::pause;break;case stop_army:icon=UiIcon::stop;break;case repair:icon=UiIcon::repair;break;case strike:icon=UiIcon::air;break;case zooming:icon=UiIcon::zoom;break;case cancel:icon=UiIcon::cancel;break;case units:icon=UiIcon::units;break;case buildings:icon=UiIcon::buildings;break;case select_army:icon=UiIcon::all;break;case base:icon=UiIcon::base;break;case box_select:icon=UiIcon::box;break;default:break;}
  auto color=enabled?(primary?cyan:cream):muted;if(icon!=UiIcon::none){Skin::icon(r,icon,area.x+5,area.y+(area.h-12)/2,!enabled?muted:icon==UiIcon::stop?red:icon==UiIcon::air?gold:cyan);area.x+=17;area.w-=17;}if(held)++area.y;r.label(area,label,color,15);
 }
 void paragraph(Raster& r,int x,int y,std::initializer_list<const char*> lines,int size=17,uint16_t color=cream,int step=26){for(auto s:lines){r.text(x,y,s,color,size);y+=step;}}
 void insignia(Raster& r,int x,int y,int size,uint16_t c){r.poly({Pt{float(x),float(y-size)},Pt{float(x+size),float(y)},Pt{float(x),float(y+size)},Pt{float(x-size),float(y)}},c);r.line(x-size/2,y,x,y-size/2,navy,3);r.line(x,y-size/2,x+size/2,y,navy,3);r.rect(x-2,y,4,size/2,navy);}
 void portrait(Raster& r,int x,int y,int size,bool lin=true){r.box({x,y,size,size},rgb(43,63,62));for(int i=0;i<size;i+=6)r.line(x+i,y,x,y+i,edge);float f=size/64.f;auto b=[&](int xx,int yy,int ww,int hh,uint16_t c){r.rect(x+xx*f,y+yy*f,std::ceil(ww*f),std::ceil(hh*f),c);};b(7,49,50,15,rgb(65,90,78));b(16,37,31,20,rgb(62,81,70));b(26,37,12,13,rgb(177,142,104));b(18,13,29,27,rgb(184,155,119));b(15,9,35,12,rgb(39,49,44));b(14,17,7,23,rgb(40,47,42));b(43,18,7,21,rgb(40,47,42));b(19,9,29,6,lin?rgb(102,120,92):rgb(70,91,83));b(26,12,13,3,gold);b(24,25,5,2,navy);b(37,25,5,2,navy);b(30,32,4,2,rgb(128,99,78));b(28,37,10,2,rgb(121,85,66));b(16,42,6,12,cyan);b(45,28,7,5,navy);b(45,33,3,11,navy);b(36,54,6,3,gold);r.rect(x,y,size,1,cyan);}
 void topbar(Raster& r){
  r.rect(0,0,w,50,navy);r.rect(0,48,w,2,edge);r.rect(0,48,138,1,cyan);Sprites::fit(r,art::badge_first,{6,7,27,30});r.text(36,5,"钢铁黎明",cream,18);char b[80];
  Skin::panel_frame(r,{152,3,83,42},rgb(22,35,38));Skin::icon(r,UiIcon::ore,158,13,gold);if(world.credits>=10000)std::snprintf(b,sizeof b,"%dk",world.credits/1000);else std::snprintf(b,sizeof b,"%04d",world.credits);r.text(174,5,b,gold,20);r.text(174,28,"矿石",muted,10);
  Skin::panel_frame(r,{239,3,68,42},rgb(22,35,38));Skin::icon(r,UiIcon::power,244,14,world.power_net<0?red:cyan);std::snprintf(b,sizeof b,"%+d",world.power_net);r.text(257,6,b,world.power_net<0?red:cyan,18);r.text(257,28,world.power_net<0?(display.width<400?"缺电":"低电力"):"电力",muted,10);
  std::snprintf(b,sizeof b,"%02u:%02u",unsigned(world.time)/60,unsigned(world.time)%60);r.label({310,8,94,26},b,cream,16);button(r,{w-67,8,57,27},pausing,"暂停");
 }
 void draw_minimap(Raster& r){auto m=minimap();Skin::panel_frame(r,{m.x-3,m.y-3,m.w+6,m.h+6},navy,edge);r.rect(m.x-3,m.y-3,14,2,cyan);r.rect(m.x+m.w-11,m.y+m.h+1,14,2,cyan);for(int y=0;y<N;++y)for(int x=0;x<N;++x){int c=y*N+x;auto col=!world.seen[c]?navy:world.terrain[c]==2?rgb(38,72,80):world.ore[c]?gold:world.fog[c]?rgb(81,106,82):rgb(45,62,56);r.rect(m.x+x*m.w/N,m.y+y*m.h/N,(x+1)*m.w/N-x*m.w/N,(y+1)*m.h/N-y*m.h/N,col);}for(auto& a:world.e)if(a.hp>0&&(a.team==0||world.fog[cell(a.p)]))r.rect(m.x+a.p.x*m.w/N-1,m.y+a.p.y*m.h/N-1,a.kind==supply?4:3,a.kind==supply?4:3,a.team==0?cyan:a.team==1?red:a.kind==supply?cyan:gold);Vec c=field.unproject(w/2,(field.top+field.bottom)/2);int mx=m.x+c.x*m.w/N,my=m.y+c.y*m.h/N;r.ring(mx,my,13,8,cream);}
 void hud(Raster& r){
  topbar(r);draw_minimap(r);button(r,selection_button(),box_select,selection.armed?"框选中":"框选",selection.armed);HudLayout layout(h);int bottom=layout.top;bool compact=display.width<400;char b[160];
  const char* unit_short[]={"游骑","破甲","坦克","火炮","矿车"};const char* build_short[]={"电站","矿厂","兵营","工厂","炮塔"};const char* build_names[]={"聚变电站","精炼厂","步兵营","装甲工厂","哨戒炮塔"};
  r.rect(0,bottom,w,h-bottom,navy);r.rect(0,bottom,w,2,edge);r.rect(8,bottom,56,2,cyan);r.rect(72,bottom,w-152,1,rgb(104,124,113));
  button(r,{8,bottom+8,56,25},units,"部队",tab==0);button(r,{8,bottom+38,56,25},buildings,"基地",tab==1);
  const int unit_k[]={rifle,rocket,tank,artillery,harvester};const int build_k[]={power,refinery,barracks,factory,turret};
  for(int j=0;j<5;++j){int k=tab?build_k[j]:unit_k[j];bool unlocked=world.unlocked(k),afford=world.credits>=specs[k].cost;auto a=layout.card(j);if(button_count<int(buttons.size()))buttons[button_count++]={a,produce0+k};
   bool active=placing==k||(world.job_count&&world.jobs[0].kind==k);Skin::panel_frame(r,a,active?rgb(35,61,57):panel,active?cyan:rgb(79,121,112));auto picture=layout.image(j);r.rect(picture.x,picture.y,picture.w,picture.h,rgb(27,43,45));r.rect(a.x+3,a.y+3,13,1,tab?gold:Skin::class_color(k));
   unsigned id=tab?art::buildings_first+k-hq:Sprites::unit_frame(k,1.5707963f,k==harvester);Sprites::fit(r,id,{picture.x+2,picture.y+2,picture.w-4,picture.h-4},unlocked?0:4);
   r.label(layout.name(j),compact?(tab?build_short[j]:unit_short[j]):tab?build_names[j]:specs[k].name,unlocked?cream:muted,11);
   if(!unlocked)Skin::icon(r,UiIcon::lock,a.x+a.w-16,a.y+5,muted);
   int queued=0;for(int qi=0;qi<world.job_count;++qi)queued+=world.jobs[qi].kind==k;
   if(queued){std::snprintf(b,sizeof b,"%d",queued);r.box({a.x+a.w-17,a.y+5,14,15},navy,cyan);r.label({a.x+a.w-17,a.y+5,14,15},b,cyan,10);if(world.jobs[0].kind==k)Skin::progress(r,layout.progress(j),1-world.jobs[0].left/world.jobs[0].total,cyan);}
   std::snprintf(b,sizeof b,"%d",specs[k].cost);r.label(layout.price(j),unlocked?b:compact?"--":"未解锁",afford?gold:red,10);
  }
  button(r,{w-76,bottom+8,68,25},select_army,"全军",true);button(r,{w-76,bottom+38,68,25},placing>=0?cancel:base,placing>=0?"取消":compact?"归位":"回基地");
  if(layout.card_height>90){unsigned portrait=art::buildings_first;for(auto& a:world.e)if(a.hp>0&&a.selected){portrait=a.kind<hq?Sprites::unit_frame(a.kind,1.5707963f,a.kind==harvester):art::buildings_first+a.kind-hq;break;}Skin::panel_frame(r,{8,bottom+70,56,44});Sprites::fit(r,portrait,{11,bottom+73,50,38});Sprites::fit(r,art::badge_first,{w-67,bottom+73,50,40});}
  int command_y=layout.commands;
  if(world.job_count){auto& j=world.jobs[0];std::snprintf(b,sizeof b,"生产 %s",compact&&j.kind<=harvester?unit_short[j.kind]:specs[j.kind].name);r.text(9,command_y+2,b,cream,11);Skin::progress(r,{158,command_y+11,38,4},1-j.left/j.total,cyan);}else {std::snprintf(b,sizeof b,compact?"选%d / 军%d":"已选 %d / 部队 %d",world.selected(),world.army());if(world.selected()==1)for(auto& a:world.e)if(a.hp>0&&a.selected){if(a.kind==factory||a.kind==barracks)std::snprintf(b,sizeof b,compact?"点地面设集结":"点空地设置集结点");else if(a.kind<harvester){if(compact)std::snprintf(b,sizeof b,"%s %u星",unit_short[a.kind],veteran_rank(a));else std::snprintf(b,sizeof b,"%s · %u星 · %u击毁",specs[a.kind].name,veteran_rank(a),unsigned(a.kills));}}r.text(9,command_y+3,b,muted,11);}
  button(r,{202,command_y,52,24},stop_army,"停止");button(r,{264,command_y,48,24},cancel,"撤单");button(r,{317,command_y,48,24},repair,"维修");if(world.strike_cd>0){std::snprintf(b,sizeof b,"%02u秒",unsigned(std::ceil(world.strike_cd)));button(r,{370,command_y,50,24},strike,b,false,false);}else button(r,{370,command_y,50,24},strike,"空袭");button(r,{425,command_y,47,24},zooming,"缩放");
  r.shade({7,field.top+5,w-104,23},navy,13);r.rect(7,field.top+5,2,23,gold);std::snprintf(b,sizeof b,"%s",objective[world.chapter]);if(world.chapter==1)std::snprintf(b,sizeof b,"中继站 %s · 撤离 %02u / 90",world.count(relay,0)?"已占领":"待占领",std::min(90u,unsigned(world.radio_time)));r.text(13,field.top+7,b,cream,12);
  if(world.count(hq,1)){int left=std::max(0,int(std::ceil(world.next_wave-world.time)));std::snprintf(b,sizeof b,"敌援 %02d:%02d",left/60,left%60);r.shade({10,field.top+31,111,18},navy,12);r.text(15,field.top+32,b,left<=12?red:gold,11);}
  if(selection.armed){int y=bottom-33;r.shade({8,y,w-16,27},navy,14);r.label({14,y+2,w-28,23},"拖动框选 · 松手后下令 · 再点框选取消",cyan,13,0);}else if(world.notice_time>0){int y=bottom-33;r.shade({8,y,w-16,27},navy,14);r.rect(8,y,2,27,cyan);r.label({14,y+2,w-28,23},world.notice,cream,13,0);}else r.label({10,bottom-26,w-20,23},compact?"拖动平移 · 点地面移动 / 点敌人攻击":"拖动战场 · 点选部队 · 点地面移动 / 点敌人攻击",cream,11,0);
  if(placing>=0&&placing!=99){auto p=field.project(pointer_world);field.quad(r,pointer_world,.95f,.95f,0,world.can_place(placing,pointer_world)?cyan:red);r.text(p.x,p.y-25,"部署预览",cream,12,1);}
  r.rect(8,h-20,w-16,1,edge);int selected=world.selected();const Entity* single=nullptr;if(selected==1)for(auto& a:world.e)if(a.hp>0&&a.selected)single=&a;
  if(single){if(single->kind<hq){const char* roles[]={"步兵压制","重装破甲","前线装甲","远程溅射","自动采矿"};if(single->kind==harvester)std::snprintf(b,sizeof b,"矿车 · 载矿 %u",unsigned(single->cargo));else std::snprintf(b,sizeof b,"%s · %s · 耐久 %u",unit_short[single->kind],roles[single->kind],unsigned(std::ceil(single->hp)));}else std::snprintf(b,sizeof b,"%s · 耐久 %u",specs[single->kind].name,unsigned(std::ceil(single->hp)));}else if(selected>1){int soldiers=0,tanks=0,guns=0;for(auto& a:world.e)if(a.hp>0&&a.selected){soldiers+=a.kind<=rocket;tanks+=a.kind==tank;guns+=a.kind==artillery;}std::snprintf(b,sizeof b,"%d 步兵   %d 坦克   %d 火炮",soldiers,tanks,guns);}else std::snprintf(b,sizeof b,"守住蓝色补给站五秒可获得矿石与维修");r.label({10,h-18,w-20,18},b,muted,10,0);
  if(save_flash>0)r.text(36,display.width<400?28:32,"已保存",cyan,9);
 }
 void title_background(Raster& r){int oldtop=field.top,oldbottom=field.bottom;float cx=field.camx,cy=field.camy;field.top=0;field.bottom=h;field.focus({7,9});field.camx+=30;field.draw(r,world);field.top=oldtop;field.bottom=oldbottom;field.camx=cx;field.camy=cy;
  for(int y=0;y<h;++y){int opacity=y<h/2?12-y*5/(h/2):7+(y-h/2)*8/(h/2);r.shade({0,y,w,1},navy,std::clamp(opacity,0,16));}for(int y=0;y<h;y+=4)r.shade({0,y,w,1},navy,1);
 }
 void page_header(Raster& r,const char* title,const char* subtitle){r.rect(0,0,w,h,navy);r.rect(15,13,3,30,cyan);r.text(28,7,title,cream,25);r.text(29,41,subtitle,muted,12);}
 void home_screen(Raster& r){title_background(r);Sprites::fit(r,art::wordmark_first,{24,10,w-48,128});r.native_text=&native_text;r.text(27,140,"北境联合 / NORTH UNION",cyan,12);r.text(27,166,"当最后一座电台沉默，",cream,16);r.text(27,191,"你就是黎明的信号。",cream,16);
  int y=std::max(214,h-128);button(r,{24,y,246,39},start,career.unlocked?"继续战役  >":"开启战役  >",true);button(r,{280,y,176,39},has_run?resume:missions,has_run?"返回战场":"战役档案");button(r,{24,y+49,132,31},missions,"战役档案");button(r,{166,y+49,140,31},help,"指挥手册");button(r,{316,y+49,140,31},difficulty,career.easy?"难度：休闲":"难度：标准");r.text(24,h-24,"原创战役 · 基地建设 · 即时战略",muted,11);r.text(w-24,h-24,"PXA / 1.5.1",muted,11,2);
 }
 void campaign_screen(Raster& r){page_header(r,"战役档案","曙光协议 / 2089 年北境战事");int top=76,step=std::min(118,(h-126)/3);char b[80];for(int j=0;j<3;++j){int y=top+j*step;bool locked=unsigned(j)>career.unlocked;r.box({20,y,w-40,step-10},panel,locked?edge:cyan);r.text(34,y+8,chapter_places[j],locked?muted:gold,12);r.text(34,y+(step<90?24:28),chapter_names[j],locked?muted:cream,step<90?18:21);if(step>=93)r.text(34,y+58,objective[j],muted,12);std::snprintf(b,sizeof b,"%s",locked?"待解锁":career.wins&(1u<<j)?"重温":"出发");button(r,{w-97,y+20,61,32},mission0+j,b,!locked,!locked);}button(r,{20,h-43,w-40,30},home,"返回指挥部");}
 void briefing_screen(Raster& r){page_header(r,chapter_names[world.chapter],chapter_places[world.chapter]);int y=78;bool compact=display.width<400;portrait(r,24,y,compact?44:60);r.text(compact?80:96,y-1,world.chapter==1?"苏弥 / 通信官":"林岚 / 北境指挥官",cyan,16);int tx=compact?24:96,ty=compact?y+57:y+30;
  if(world.chapter==0)paragraph(r,tx,ty,{"三天前，北境的灯全部熄灭。","灰塔军团封锁矿区，切断了我们的电台。","夺回赤砂前哨。我们需要一条通往黎明的路。"},15,cream,24);
  else if(world.chapter==1)paragraph(r,tx,ty,{"截获的信号来自河对岸的第七中继站。","那里还有平民。把部队带过去，占领电台。","守住信号，直到最后一辆撤离车离开。"},15,cream,24);
  else paragraph(r,tx,ty,{"灰塔并非无人机器。它劫持了民用能源网。","击毁两座聚变电站，就能让炮塔失去火力。","天亮之前，结束这场战争。让城市重新亮起。"},15,cream,24);
  int yy=std::max(191,h-143);r.box({24,yy,w-48,65},panel);r.text(38,yy+9,"作战目标",gold,13);r.label({38,yy+30,w-76,25},objective[world.chapter],cream,16,0);button(r,{24,h-64,w-121,40},begin,"部署部队 / 开始行动",true);button(r,{w-85,h-64,61,40},home,"返回");}
 void pause_screen(Raster& r){field.draw(r,world);r.shade({0,0,w,h},navy,12);int yy=(h-248)/2;r.box({70,yy,w-140,248},navy,edge);r.native_text=&native_text;r.text(w/2,yy+15,"战术暂停",cream,26,1);r.text(w/2,yy+51,"战局已保存 · 时间已停止",muted,13,1);button(r,{92,yy+80,w-184,34},resume,"继续行动",true);button(r,{92,yy+123,w-184,30},help,"指挥手册");button(r,{92,yy+161,(w-194)/2,30},restart,"重新部署");button(r,{102+(w-194)/2,yy+161,(w-194)/2,30},sound_toggle,sounds.enabled?"音效：开":"音效：关");button(r,{92,yy+201,w-184,30},save_return,"保存并返回指挥部");}
 void help_screen(Raster& r){page_header(r,"指挥手册","每一条指令，都通向黎明");bool compact=h<390;int top=64,step=std::min(55,(h-118)/5);
  const char* titles[]={"01 / 编队指挥","02 / 经济与建设","03 / 侦察与补给","04 / 保护老兵","05 / 火力与支援"};
  const char* body[]={"框选后下令；选兵营点地面设集结。","矿车自动采集；先电站，再兵营工厂。","守住蓝色补给五秒，获矿石与维修。","击毁二、五、九名敌军晋星增强火力。","破甲反装甲，火炮溅射；空袭破防。"};
  for(int j=0;j<5;++j){r.text(24,top+j*step,titles[j],cyan,compact?13:15);r.label({24,top+j*step+21,w-48,20},body[j],cream,compact?12:14,0);}button(r,{24,h-43,w-48,30},has_run?resume:home,has_run?"返回战场":"返回指挥部");}
 void result_screen(Raster& r){title_background(r);r.native_text=&native_text;bool won=world.won;r.text(w/2,29,won?"作战成功":"战线失守",won?cyan:red,34,1);r.text(w/2,82,chapter_names[world.chapter],cream,20,1);int stars=won?(world.lost<4?3:world.lost<10?2:1):0;for(int j=0;j<3;++j)insignia(r,w/2+(j-1)*40,130,12,j<stars?gold:edge);
  if(won&&world.chapter==2)r.text(w/2,156,"苏弥：城市的灯亮了。指挥官，天亮了。",cream,16,1);else if(won)r.text(w/2,156,world.chapter==0?"电台恢复了信号。河对岸传来了求救声。":"最后一辆撤离车驶过大桥。向灰塔进发。",cream,15,1);else r.text(w/2,156,"守住矿线，补足电力。北境仍在等待你。",cream,15,1);
  char b[120];std::snprintf(b,sizeof b,"作战 %02u:%02u    击毁 %d    损失 %d    采矿 %d",unsigned(world.time)/60,unsigned(world.time)%60,world.killed,world.lost,world.earned);r.label({24,194,w-48,27},b,gold,14);button(r,{24,h-87,w-48,36},won?next:restart,won?(world.chapter==2?"战役档案":"下一章 / 继续前进"):"重新部署",true);button(r,{24,h-41,w-48,28},home,"返回指挥部");}
 void render(){if(!ready||!surface||!ctx->foreground())return;
#ifdef IRON_PROFILE
  auto perf_begin=RenderProfile::now();
#endif
auto lease=surface->acquire();if(!lease)return;auto dest=lease->pixels();int stride=lease->stride_bytes()/2;auto output=reinterpret_cast<uint16_t*>(dest.data());bool direct=display.width==unsigned(w)&&display.height==unsigned(h);
field.clock=(screen==Screen::battle||screen==Screen::pause)?world.time:clock;native_text.reset(display.width,display.height,w,h);Raster r(direct?output:pixels.data(),w,h,direct?stride:w);if(screen==Screen::battle||screen==Screen::campaign||screen==Screen::briefing||screen==Screen::help)r.native_text=&native_text;if(screen!=Screen::battle)r.rect(0,0,w,h,navy);button_count=0;switch(screen){case Screen::home:home_screen(r);break;case Screen::campaign:campaign_screen(r);break;case Screen::briefing:briefing_screen(r);break;case Screen::battle:field.draw(r,world);selection.draw(r,world,field);hud(r);break;case Screen::pause:pause_screen(r);break;case Screen::help:help_screen(r);break;case Screen::result:result_screen(r);break;}
#ifdef IRON_PROFILE
  auto perf_drawn=RenderProfile::now();
#endif
  if(!direct)for(unsigned yy=0;yy<display.height;++yy){auto row=output+yy*stride;auto source=pixels.data()+(yy*h/display.height)*w;if(display.width==unsigned(w))std::copy_n(source,w,row);else for(unsigned xx=0;xx<display.width;++xx)row[xx]=source[column_map[xx]];}
  Raster lettering(output,display.width,display.height,stride);draw_native_text(native_text,lettering);
  auto shown=lease->present(++frame_id);if(!shown&&shown.error()!=pxa::Error::would_block)log("Iron Dawn present failed");
#ifdef IRON_PROFILE
  if(shown){static RenderProfile profile;profile.present(*ctx,perf_begin,perf_drawn,RenderProfile::now());}
#endif
 }
};
}
PXA_APPLICATION(iron::App)
