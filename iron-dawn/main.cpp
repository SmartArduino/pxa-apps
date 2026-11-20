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
#include "minimap.hpp"
#include "save.hpp"
#ifdef IRON_PROFILE
#include "profile.hpp"
#endif
#include "game_audio.hpp"
#include "frame_scaler.hpp"
#include "story.hpp"
#include "story_text.hpp"
namespace iron {
enum class Screen {home,campaign,briefing,battle,pause,result,help,research,radio};
enum Action {start=1,resume,missions,help,home,exit_game,begin,pausing,restart,next,select_army,base,units,buildings,repair,strike,zooming,cancel,save_return,sound_toggle,difficulty,box_select,stop_army,endless,research_open,research_ammo,research_armor,research_cancel,tactics_open,dock_close,campaign_prev,campaign_next,radio_continue,objective_focus,brief_next,brief_previous,help_next,help_previous,cancel_build,squads_open,tactic_escort,tactic_retreat,cancel_tactic,construction_page,hero_support,tactic_transport,mission0=50,squad_recall0=70,squad_assign0=80,produce0=100};
enum class TacticalMode : uint8_t {none,escort,retreat,board,land};
struct Button {Rect r;int action;};
struct App {
 pxa::Context* ctx=nullptr;pxa::ui::DisplayMetrics display,desired_display;bool surface_rebuild=false;std::optional<pxa::Surface> surface;std::vector<uint16_t> pixels;FrameScaler frame_scaler;NativeText native_text;unsigned clock_period=0;
 GameAudio sounds;SelectionTool selection;uint8_t pointer_owner=0;World world;Career career;Battlefield field;MinimapCache minimap_cache;DockState dock;Screen screen=Screen::home;TacticalMode tactical=TacticalMode::none;std::array<Button,32> buttons{};int button_count=0;
 DisplayPlan display_plan{480,320,1,0,0,false,1,1};int help_page=0,campaign_page=0,radio_id=0,brief_page=0,brief_pages=1,radio_page=0,radio_pages=1;int w=480,h=320,tab=0,placing=-1,focus_action=start;bool combined_page=false;bool exit_pending=false;int last_selected_id=-1;float last_tap=-10;bool ready=false,has_run=false,skirmish=false,pressing=false,dragging=false,exiting=false,saving=false,pending_save=false;uint64_t last_tick=0,frame_id=0,now=0;float clock=0,save_flash=0;int downx=0,downy=0,lastx=0,lasty=0;Vec pointer_world{};Save save_buffer{};uint32_t save_bank=0,defense_best=0;
 void log(std::string_view text){(void)ctx->log().write(pxa::LogLevel::info,text);}
 void set_clock(unsigned period){if(clock_period!=period&&ctx->clock().set_period(period))clock_period=period;}
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){ctx=&c;if(auto d=pxa::ui::decode_start_display(config))display=*d;desired_display=display;(void)c.window().request_orientation(pxa::OrientationPreference::landscape);(void)c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);return c.tasks().start(initialize());}
 pxa::Task<void> initialize(){
  for(int bank=0;bank<2;++bank){const char* path=bank?"iron-dawn-1.save":"iron-dawn-0.save";auto opened=co_await ctx->fs().open(path);if(!opened)continue;auto bytes=std::as_writable_bytes(std::span{&save_buffer,1});size_t at=0;while(at<bytes.size()){auto got=opened->read(bytes.subspan(at,std::min(size_t(4096),bytes.size()-at)));if(!got||!*got)break;at+=*got;}(void)opened->close();
   if(decode_save(std::as_bytes(std::span{&save_buffer,1}).first(at),save_buffer)&&save_buffer.generation>=save_bank){career=save_buffer.career;defense_best=save_buffer.defense_best;has_run=save_buffer.has_run;if(has_run)world=save_buffer.world;save_bank=save_buffer.generation;}
  }
  if(save_bank)log("Iron Dawn restored");
  auto s=co_await sounds.open(*ctx);if(!s)log("Iron Dawn: sound unavailable");
  if(!has_run)world.init(0);auto window=co_await ctx->window().snapshot(true);if(window){desired_display.width=window->pixel_width;desired_display.height=window->pixel_height;}surface_rebuild=true;auto configured=co_await rebuild_surface();if(!configured)co_return configured;log("Iron Dawn ready");co_return pxa::Result<void>{};
 }
 void update_display(pxa::ui::DisplayMetrics next){desired_display=next;if(!ready||surface_rebuild)return;if(next.width==display.width&&next.height==display.height&&next.density_q16==display.density_q16&&next.font_scale_q16==display.font_scale_q16){display=next;return;}surface_rebuild=true;ready=false;auto started=ctx->tasks().start(rebuild_surface());if(!started){surface_rebuild=false;log("Iron Dawn display resize failed");}}
 void on_window_changed(const pxa::WindowMetrics& metrics){auto next=desired_display;next.width=metrics.pixel_width;next.height=metrics.pixel_height;update_display(next);}
 pxa::Task<void> rebuild_surface(){Vec center=surface?field.unproject(field.width/2,(field.top+field.bottom)/2):world.deployment_view();selection.cancel();dock.close();pressing=dragging=false;last_tick=0;ready=false;
  do{surface.reset();display=desired_display;display_plan=DisplayPlan::fit(display.width,display.height,display.density_q16,display.font_scale_q16);auto viewport=wide_ui()?GameViewport{display_plan.width,display_plan.height}:GameViewport::fit(display_plan.width,display_plan.height);w=viewport.width;h=viewport.height;if(display_plan.width==w&&display_plan.height==h)std::vector<uint16_t>{}.swap(pixels);else{size_t needed=size_t(w)*h;if(needed>pixels.capacity())std::vector<uint16_t>{}.swap(pixels);pixels.resize(needed);}frame_scaler.reset(w,h,display_plan.width,display_plan.height);
   auto created=co_await ctx->surface().create_mapped({uint16_t(display_plan.width),uint16_t(display_plan.height),2,true,2097152});if(!created){surface_rebuild=false;log("Iron Dawn surface create error");co_return std::unexpected(created.error());}surface.emplace(std::move(*created));auto configured=co_await surface->configure({0,0,uint16_t(display_plan.width),uint16_t(display_plan.height),0,true});if(!configured){surface_rebuild=false;co_return configured;}
  }while(display.width!=desired_display.width||display.height!=desired_display.height||display.density_q16!=desired_display.density_q16||display.font_scale_q16!=desired_display.font_scale_q16);
  display=desired_display;field.width=wide_ui()?large_layout().world_width:w;field.height=h;field.top=wide_ui()?large_layout().top:readable_ui()?compact_layout().status_height:50;field.bottom=wide_ui()?large_layout().bottom:compact_ui()?compact_layout().top:HudLayout(h).top;field.ground.valid=false;field.focus(center);ready=true;surface_rebuild=false;set_clock(screen==Screen::battle||screen==Screen::research?33:100);char message[220];std::snprintf(message,sizeof message,"Iron Dawn display %ux%u canvas=%dx%d wide=%d world=%d top=%d bottom=%d scale=%d dpi=%u font=%u",display.width,display.height,w,h,wide_ui(),field.width,field.top,field.bottom,display_plan.scale,display.density_q16,display.font_scale_q16);log(message);render();co_return pxa::Result<void>{};
 }
 void save(){pending_save=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(pending_save){pending_save=false;new(&save_buffer) Save{};save_buffer.career=career;save_buffer.has_run=has_run;save_buffer.world=world;save_buffer.defense_best=defense_best;save_buffer.generation=save_bank+1;save_buffer.checksum=checksum(save_buffer);auto bytes=std::as_bytes(std::span{&save_buffer,1});
   auto opened=co_await ctx->fs().open("iron-dawn.tmp",pxa::OpenMode::write|pxa::OpenMode::create|pxa::OpenMode::truncate);if(!opened){log("Iron Dawn save open failed");break;}size_t at=0;
   while(at<bytes.size()){auto wrote=opened->write(bytes.subspan(at,std::min(size_t(4096),bytes.size()-at)));if(!wrote||!*wrote)break;at+=*wrote;}auto closed=opened->close();
   if(at==bytes.size()&&closed){auto committed=co_await ctx->fs().replace("iron-dawn.tmp",save_buffer.generation%2?"iron-dawn-1.save":"iron-dawn-0.save");if(committed){save_bank=save_buffer.generation;save_flash=1.5f;log("Iron Dawn saved");}else log("Iron Dawn save publish failed");}else log("Iron Dawn save write failed");
  }saving=false;co_return pxa::Result<void>{};}

 void new_mission(int ch){field.ground.release();brief_page=0;combined_page=false;dock.close();world.init(ch);if(career.easy){world.credits+=600;for(auto& a:world.e)if(a.hp>0&&a.team==0)a.hp*=1.35f;}field.focus(world.deployment_view());placing=-1;tab=0;screen=Screen::briefing;has_run=true;skirmish=false;set_clock(100);save();}
 void new_defense(uint32_t seed=0){field.ground.release();brief_page=0;combined_page=false;dock.close();world.init_defense(seed?seed:uint32_t(now)^0x51d39f17u);if(career.easy){world.credits+=600;for(auto& a:world.e)if(a.hp>0&&a.team==0)a.hp*=1.35f;}field.focus(world.deployment_view());placing=-1;tab=0;screen=Screen::briefing;has_run=true;skirmish=true;selection.cancel();save();}
 void report(){char s[200];std::snprintf(s,sizeof s,"Iron Dawn state chapter=%d screen=%d time=%u credits=%d army=%d enemy=%d queue=%d power=%d selected=%d won=%d defeated=%d",world.chapter,int(screen),unsigned(world.time),world.credits,world.army(),world.army(1),world.job_count,world.power_net,world.selected(),world.won,world.defeated);log(s);}
 void finish(){tactical=TacticalMode::none;selection.cancel();pressing=false;dragging=false;screen=Screen::result;has_run=false;if(world.extra.mode){defense_best=std::max(defense_best,world.extra.cleared);sounds.play(motion::Cue::impact);}else if(world.won){career.unlocked=std::max(career.unlocked,unsigned(std::min(CHAPTERS-1,world.chapter+1)));career.wins|=1u<<world.chapter;auto& best=career.best[world.chapter];uint32_t seconds=std::max(1u,unsigned(world.time));if(!best||seconds<best)best=seconds;sounds.play(motion::Cue::win);}else sounds.play(motion::Cue::impact);placing=-1;set_clock(100);save();report();}
 void action(int a){
  if(a!=tactic_escort&&a!=tactic_retreat&&a!=tactic_transport&&a!=objective_focus&&a!=zooming&&a!=base)tactical=TacticalMode::none;
  if(a!=box_select)selection.cancel();sounds.play(motion::Cue::click);if(a>=mission0&&a<mission0+CHAPTERS){if(unsigned(a-mission0)<=career.unlocked)new_mission(a-mission0);return;}
  if(a>=squad_recall0&&a<squad_recall0+squad_count){squad_action(a-squad_recall0+1,false);return;}
  if(a>=squad_assign0&&a<squad_assign0+squad_count){squad_action(a-squad_assign0+1,true);return;}
  if(a>=produce0&&a<produce0+int(kind_count)){production_action(a-produce0);return;}
  switch(a){case start:new_mission(std::min(unsigned(CHAPTERS-1),career.unlocked));break;case resume:dock.close();screen=Screen::battle;placing=-1;last_tick=0;break;case missions:campaign_page=int(career.unlocked)/3;screen=Screen::campaign;break;case campaign_prev:campaign_page=0;break;case campaign_next:campaign_page=1;break;case brief_next:++brief_page;break;case brief_previous:brief_page=std::max(0,brief_page-1);break;case radio_continue:if(radio_page+1<radio_pages){++radio_page;break;}world.mission.read_events|=1u<<radio_id;screen=Screen::battle;last_tick=0;save();break;case help:help_page=0;screen=Screen::help;break;case help_next:++help_page;break;case help_previous:help_page=std::max(0,help_page-1);break;case endless:new_defense();break;case research_open:screen=Screen::research;placing=-1;break;case research_ammo:case research_armor:if(world.research_start(a==research_ammo?1:2))save();break;case research_cancel:world.research_cancel();save();break;case home:screen=Screen::home;placing=-1;break;
   case begin:minimap_cache.release();dock.close();screen=Screen::battle;world.say("点兵营或工厂生产，点全军下达移动或攻击。",compact_ui()?5:9);world.select_all();sounds.respond(world,UnitCue::select);save();report();break;
   case pausing:screen=Screen::pause;placing=-1;save();report();break;case restart:if(world.extra.mode)new_defense(world.extra.seed);else new_mission(world.chapter);break;case next:if(world.chapter<CHAPTERS-1)new_mission(world.chapter+1);else screen=Screen::campaign;break;
   case box_select:if(selection.armed){selection.cancel();world.say("框选已取消");}else {selection.armed=true;placing=-1;last_selected_id=-1;world.say("拖动框选作战单位，松手后点地面移动。",6);}break;
   case stop_army:world.stop_selected();sounds.respond(world,UnitCue::stop);report();break;
   case objective_focus:{
    auto point=world.objective_position();field.focus(point);
    char focus[112];std::snprintf(focus,sizeof focus,"Iron Dawn objective focus x=%.1f y=%.1f side=%d",point.x,point.y,world.side);log(focus);
    world.say("任务目标已标记；点地面下达行军指令。",5);
   }break;case select_army:world.select_all();sounds.respond(world,UnitCue::select);placing=-1;break;case base:field.focus(world.base_position());break;case units:tab=0;placing=-1;if(compact_ui())dock.toggle(DockPanel::units);break;case buildings:tab=1;placing=-1;if(compact_ui())dock.toggle(DockPanel::buildings);break;case tactics_open:dock.toggle(DockPanel::tactics);break;case dock_close:dock.close();break;case squads_open:placing=-1;dock.toggle(DockPanel::squads);break;case repair:world.repair();break;case strike:placing=99;world.say("点亮区内目标：空袭 180 / 冷却 45 秒");break;
   case zooming:{Vec p=field.unproject(field.width/2,(field.top+field.bottom)/2);field.zoom=field.zoom>.95f?.72f:field.zoom<.8f?1.3f:1.f;field.focus(p);}break;case cancel:if(placing>=0){placing=-1;world.say("部署已取消",3);}else {world.cancel_job();save();}break;
   case tactic_escort:arm_tactical(TacticalMode::escort);break;
   case tactic_retreat:arm_tactical(TacticalMode::retreat);break;
   case tactic_transport:arm_transport();break;
   case construction_page:combined_page=!combined_page;break;
   case hero_support:if(world.commander_support())save();break;
   case cancel_tactic:world.say("已取消待发指令");break;
   case cancel_build:if(world.cancel_construction()){placing=-1;save();}break;
   case save_return:save();screen=Screen::home;break;case sound_toggle:sounds.toggle();break;case difficulty:career.easy=!career.easy;save();break;case exit_game:save();exit_pending=true;break;
  }
  if(a!=units&&a!=buildings&&a!=tactics_open&&a!=cancel_build&&a!=squads_open&&a!=construction_page)dock.close();
  set_clock(screen==Screen::battle||screen==Screen::research?33:100);
 }
 bool wide_ui()const{return display_plan.wide;}
 bool readable_ui()const{return !wide_ui()&&display.density_q16>=81920;}
 int page_top(const Raster& r)const{return readable_ui()?r.text_height("中",25)+r.text_height("中",12)+24:64;}
 int page_row(const Raster& r)const{return readable_ui()?std::max(compact_layout().row,r.text_height("中",15)+16):32;}
 LargeHudLayout large_layout()const{return LargeHudLayout(w,h,display_plan.units_per_dp,display_plan.font_scale);}
 bool compact_ui()const{return !wide_ui()&&CompactHudLayout::applies(display.width,display.height);}
 CompactHudLayout compact_layout()const{return CompactHudLayout(h,std::max(1,int(display.height)),w,display.density_q16/65536.f,display_plan.font_scale);}
 int visible_bottom()const{
  if(compact_ui())return compact_layout().overlay(dock.panel).y;
  return field.bottom-(!wide_ui()&&dock.panel==DockPanel::squads?104:0);
 }
 bool minimap_visible()const{auto m=minimap();return m.y+m.h+3<=visible_bottom();}
 bool selection_visible()const{if(wide_ui())return false;auto a=selection_button();return a.y+a.h<=visible_bottom();}
 Rect selection_button()const{return wide_ui()?large_layout().command(1):Rect{w-90,field.top+96,80,readable_ui()?compact_layout().row:28};}
 Rect battlefield()const{return {0,field.top,field.width,visible_bottom()-field.top};}
 Rect minimap()const{return wide_ui()?large_layout().map():Rect{w-90,field.top+8,80,80};}
 bool map_click(int x,int y){if(!minimap_visible())return false;auto m=minimap();if(!m.has(x,y))return false;field.focus({std::clamp((x-m.x)*world.side/float(m.w),1.f,float(world.side-2)),std::clamp((y-m.y)*world.side/float(m.h),1.f,float(world.side-2))});return true;}
 int hit_button(int x,int y)const{for(int i=button_count-1;i>=0;--i)if(buttons[i].r.has(x,y))return buttons[i].action;return -1;}
 void on_pointer(pxa::Context&,const pxa::input::Pointer& p){
  if(!ready)return;
  int x=(pxa::ui::canvas_to_surface_coordinate(p.x,display)-display_plan.offset_x)*w/std::max(1,display_plan.width*display_plan.scale);
  int y=(pxa::ui::canvas_to_surface_coordinate(p.y,display)-display_plan.offset_y)*h/std::max(1,display_plan.height*display_plan.scale);
  using P=pxa::input::PointerPhase;
  if(p.action()==P::down){
   if(pressing)return; // A second finger cannot steal an in-progress selection.
   pointer_owner=p.pointer_id;pressing=true;dragging=false;downx=lastx=x;downy=lasty=y;pointer_world=field.unproject(x,y);
   if(screen==Screen::battle)selection.begin(x,y,battlefield(),minimap_visible()?minimap():Rect{},selection_visible()?selection_button():Rect{},hit_button(x,y)>=0,placing>=0||tactical!=TacticalMode::none);
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
   if(n)sounds.respond(world,UnitCue::select);report_selection();report();render();return;
  }
  if(dragging){last_selected_id=-1;return;}
  int a=hit_button(x,y);if(a>=0&&a==hit_button(downx,downy)){action(a);render();return;}
  if(screen!=Screen::battle||y<field.top||y>=visible_bottom())return;
  if(map_click(x,y))return;
  if(!battlefield().has(x,y))return;
  dock.close();Vec pos=field.unproject(x,y);pos.x=std::clamp(pos.x,1.f,world.side-1.2f);pos.y=std::clamp(pos.y,1.f,world.side-1.2f);
  if(tactical!=TacticalMode::none)tactical_tap(x,y,pos);
  else if(placing==99){if(world.strike(pos))placing=-1;}
  else if(placing>=0){if(world.place(placing,pos)){placing=-1;save();}}
  else {
   int id=combined_unit_at(x,y);if(id<0)id=world.hit(pos,.95f);
   if(id<0||!unit_kind(world.e[id].kind)){int building=building_at(x,y);if(building>=0)id=building;}
   if(id>=0&&world.e[id].team==0){
    if(unit_kind(world.e[id].kind)&&id==last_selected_id&&clock-last_tap<.4f){int kind=world.e[id].kind;for(auto& unit:world.e)unit.selected=deployed(unit)&&unit.team==0&&unit.kind==kind;world.say("已编组全部同型部队");}
    else {world.select_one(id);world.say(world.e[id].kind==factory?"工厂：选车辆或点空地设集结":world.e[id].kind==barracks?"兵营：选步兵或点空地设集结":specs[world.e[id].kind].name);}
    int kind=world.e[id].kind;
    if(kind==hq){tab=1;if(compact_ui())dock.panel=DockPanel::buildings;}
    else if(world.selected_producer()>=0){tab=0;if(compact_ui())dock.panel=DockPanel::units;}
    last_selected_id=id;last_tap=clock;sounds.respond(world,UnitCue::select,id);
   }else if(world.selected()){bool attack=id>=0&&world.e[id].team==1;world.order(pos,attack?id:-1);sounds.respond(world,attack?UnitCue::attack:UnitCue::move);}
   else world.say("点选己方部队，或使用框选和全军");
  }
  report();
 }
 void report_selection(){char line[512];int used=std::snprintf(line,sizeof line,"Iron Dawn selection ids=");for(int i=0;i<MAX;++i)if(world.e[i].hp>0&&world.e[i].selected&&used<int(sizeof line)-6)used+=std::snprintf(line+used,sizeof line-used,"%d,",i);log(line);}
 void clamp_camera(){
  auto focus=field.unproject(field.width/2,(field.top+field.bottom)/2);
  float limit=world.side-1.f;
  field.focus({std::clamp(focus.x,1.f,limit),std::clamp(focus.y,1.f,limit)});
 }
 pxa::BackAction on_back(){if(exiting)return pxa::BackAction::close;if(screen==Screen::radio){action(radio_continue);return pxa::BackAction::stay;}if(screen==Screen::research){action(resume);return pxa::BackAction::stay;}if(screen==Screen::battle){if(dock.open()){dock.close();return pxa::BackAction::stay;}if(tactical!=TacticalMode::none){action(cancel_tactic);return pxa::BackAction::stay;}if(placing>=0){action(cancel);return pxa::BackAction::stay;}if(selection.armed||selection.tracking){selection.cancel();pressing=false;dragging=false;world.say("框选已取消");return pxa::BackAction::stay;}action(pausing);return pxa::BackAction::stay;}if(screen==Screen::pause){action(resume);return pxa::BackAction::stay;}if(screen==Screen::home){save();exit_pending=true;return pxa::BackAction::stay;}screen=Screen::home;return pxa::BackAction::stay;}
 void on_controller(pxa::Context&,const pxa::input::ControllerState& s){using namespace pxa::input;static uint32_t prior=0;uint32_t pressed=s.buttons&~prior;prior=s.buttons;if(!ready)return;if(pressed&controller_b){(void)on_back();return;}if(screen==Screen::battle){if(selection.tracking&&!(pressed&controller_start))return;if(pressed&controller_a)action(select_army);if(pressed&controller_x)action(stop_army);if(pressed&controller_start)action(pausing);if(s.buttons&controller_left)field.camx-=16;if(s.buttons&controller_right)field.camx+=16;if(s.buttons&controller_up)field.camy-=16;if(s.buttons&controller_down)field.camy+=16;clamp_camera();}else {if(pressed&controller_up)focus_action=button_count?buttons[std::max(0,button_index()-1)].action:start;if(pressed&controller_down)focus_action=button_count?buttons[std::min(button_count-1,button_index()+1)].action:start;if(pressed&controller_a)action(focus_action);}}
 int button_index()const{for(int i=0;i<button_count;++i)if(buttons[i].action==focus_action)return i;return 0;}
 pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e){sounds.event(e);if(auto metrics=pxa::ui::decode_display_metrics(e)){update_display(*metrics);return true;}auto tick=pxa::decode_clock_tick(e);if(!tick)return false;now=tick->timestamp_us;float dt=last_tick?std::clamp(float(now-last_tick)*1e-6f,0.f,.25f):.05f;last_tick=now;if(!ready||!ctx->foreground())return true;set_clock(screen==Screen::battle||screen==Screen::research?33:100);clock+=dt;field.clock=clock;save_flash=std::max(0.f,save_flash-dt);sounds.tick(now,dt,screen==Screen::battle||screen==Screen::research,screen==Screen::pause||screen==Screen::radio);
  if(screen==Screen::battle||screen==Screen::research){auto before=UnitVoice::roster(world);for(float left=dt;left>.001f;left-=.05f)world.tick(std::min(left,.05f));for(int i=0;i<MAX;++i)if(!before[i]&&world.e[i].hp>0&&world.e[i].team==0&&world.e[i].kind<hq)sounds.unit_ready(world.e[i].kind);if(world.sound){sounds.play_event(world.sound);world.sound=0;}if(world.extra.mode)defense_best=std::max(defense_best,world.extra.cleared);if(world.won||world.defeated)finish();else if(!world.extra.mode){unsigned pending=world.mission.events&~world.mission.read_events;for(int id=0;id<8;++id)if(pending&(1u<<id)){radio_id=id;radio_page=0;screen=Screen::radio;tactical=TacticalMode::none;selection.cancel();dock.close();save();break;}}if(world.autosave>=12){world.autosave=0;save();report();}}
  render();if(exit_pending&&!saving&&sounds.shutdown()){exiting=true;(void)ctx->window().request_back();}return true;}
 void on_background(pxa::Context&){tactical=TacticalMode::none;dock.close();selection.cancel();pressing=false;dragging=false;if(screen==Screen::battle||screen==Screen::research)screen=Screen::pause;sounds.suspend();save();set_clock(0);last_tick=0;}
 void on_foreground(pxa::Context&){sounds.resume();(void)ctx->window().request_orientation(pxa::OrientationPreference::landscape);(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);last_tick=0;if(ready)set_clock(screen==Screen::battle||screen==Screen::research?33:100);}
 void button(Raster& r,Rect area,int a,std::string_view label,bool primary=false,bool enabled=true){
  if(button_count<int(buttons.size())&&enabled)buttons[button_count++]={area,a};bool held=pressing&&!dragging&&area.has(downx,downy);Skin::button(r,area,primary,held,enabled);
  UiIcon icon=UiIcon::none;if(display.width>=480)switch(a){case pausing:icon=UiIcon::pause;break;case stop_army:icon=UiIcon::stop;break;case repair:icon=UiIcon::repair;break;case strike:icon=UiIcon::air;break;case zooming:icon=UiIcon::zoom;break;case cancel:icon=UiIcon::cancel;break;case units:icon=UiIcon::units;break;case buildings:icon=UiIcon::buildings;break;case select_army:icon=UiIcon::all;break;case base:icon=UiIcon::base;break;case box_select:icon=UiIcon::box;break;default:break;}
  auto color=enabled?(primary?cyan:cream):muted;int icon_size=readable_ui()?std::min(24,r.text_height("中",12)):12;if(icon!=UiIcon::none&&area.w>=r.measure(label,15)+icon_size+12){Skin::icon(r,icon,area.x+5,area.y+(area.h-icon_size)/2,!enabled?muted:icon==UiIcon::stop?red:icon==UiIcon::air?gold:cyan,icon_size);area.x+=icon_size+5;area.w-=icon_size+5;}if(held)++area.y;r.label(area,label,color,15);
 }
 void paragraph(Raster& r,int x,int y,std::initializer_list<const char*> lines,int size=17,uint16_t color=cream,int step=26){for(auto s:lines){r.text(x,y,s,color,size);y+=step;}}
 void insignia(Raster& r,int x,int y,int size,uint16_t c){r.poly({Pt{float(x),float(y-size)},Pt{float(x+size),float(y)},Pt{float(x),float(y+size)},Pt{float(x-size),float(y)}},c);r.line(x-size/2,y,x,y-size/2,navy,3);r.line(x,y-size/2,x+size/2,y,navy,3);r.rect(x-2,y,4,size/2,navy);}
 void portrait(Raster& r,int x,int y,int size,unsigned person=0){Skin::well(r,{x,y,size,size});Sprites::fit(r,art::story_portraits_first+std::min(3u,person),{x+2,y+2,size-4,size-4});}
 void topbar(Raster& r){
  if(readable_ui()){
   int top=field.top,line=compact_layout().line,pause=88,available=w-pause-26,ore=available*38/100,power=available*30/100,time=available-ore-power;char text[64];
   r.rect(0,0,w,top,navy);r.rect(0,top-2,w,2,edge);int x=6;
   for(int i=0;i<3;++i){int width=i==0?ore:i==1?power:time;Skin::panel_frame(r,{x,3,width-4,top-8});const char* label=i==0?"矿石":i==1?(world.power_net<0?"缺电":"电力"):save_flash>0?"已保存":"时间";r.label({x+4,5,width-12,line},label,muted,12);if(i==0)std::snprintf(text,sizeof text,"%d",world.credits);else if(i==1)std::snprintf(text,sizeof text,"%+d",world.power_net);else std::snprintf(text,sizeof text,"%02u:%02u",unsigned(world.time)/60,unsigned(world.time)%60);r.label({x+4,5+line,width-12,line},text,i==0?gold:i==1?(world.power_net<0?red:cyan):cream,12);x+=width;}
   button(r,{w-pause-6,4,pause,top-10},pausing,"暂停");return;
  }
  r.rect(0,0,w,50,navy);r.rect(0,48,w,2,edge);int available=w-102,ore_width=available*36/100,power_width=available*29/100,power_x=14+ore_width,time_x=power_x+power_width+6,time_width=w-82-time_x;char b[80];
  Skin::panel_frame(r,{8,3,ore_width,42},rgb(22,35,38));Skin::icon(r,UiIcon::ore,14,14,gold);if(world.credits>=10000)std::snprintf(b,sizeof b,"%dk",world.credits/1000);else std::snprintf(b,sizeof b,"%04d",world.credits);r.text(31,5,b,gold,20);r.text(31,28,"矿石",muted,10);
  Skin::panel_frame(r,{power_x,3,power_width,42},rgb(22,35,38));Skin::icon(r,UiIcon::power,power_x+6,14,world.power_net<0?red:cyan);std::snprintf(b,sizeof b,"%+d",world.power_net);r.text(power_x+23,6,b,world.power_net<0?red:cyan,18);r.text(power_x+23,28,world.power_net<0?"缺电":"电力",muted,10);
  Skin::panel_frame(r,{time_x,3,time_width,42},rgb(22,35,38));std::snprintf(b,sizeof b,"%02u:%02u",unsigned(world.time)/60,unsigned(world.time)%60);r.label({time_x,save_flash>0?4:9,time_width,save_flash>0?23:30},b,cream,16);if(save_flash>0)r.label({time_x,28,time_width,15},"已保存",cyan,10);button(r,{w-76,4,68,42},pausing,"暂停");
 }
 void draw_minimap(Raster& r){
  auto m=minimap();
  Skin::panel_frame(r,{m.x-3,m.y-3,m.w+6,m.h+6},navy,edge);
  r.rect(m.x-3,m.y-3,14,2,cyan);r.rect(m.x+m.w-11,m.y+m.h+1,14,2,cyan);
  minimap_cache.draw(r,world,m);
  auto point=[&](Vec p){return Pt{m.x+p.x*m.w/world.side,m.y+p.y*m.h/world.side};};
  auto clip=r.clip;r.clip=m;
  for(auto& a:world.e)if(deployed(a)&&(a.team==0||world.fog[cell(a.p)]||(!world.extra.mode&&a.kind==relay))){
   auto p=point(a.p);
   int size=a.kind==supply||a.kind==relay?4:3;
   r.rect(p.x-size/2,p.y-size/2,size,size,a.team==0?cyan:a.team==1?red:a.kind==supply?cyan:gold);
  }
  if(world.authored_map()){
   auto p=point(world.objective_position());
   r.ring(p.x,p.y,4,4,gold);
  }
  if((world.extra.mode||world.count(hq,1))&&world.next_wave>world.time&&world.next_wave-world.time<=10&&(!world.extra.mode||world.wave==int(world.extra.cleared))){
   auto p=point(world.wave_entry(world.wave+1));
   r.ring(p.x,p.y,3+int(world.time*3)%3,3+int(world.time*3)%3,red);
  }
  if(world.chapter==4&&!world.extra.mode){
   auto p=point(world.convoy_stop(3));
   r.ring(p.x,p.y,3,3,gold);
   for(int truck:world.mission.convoy)if(world.active_convoy(truck)){
    auto p=point(world.e[truck].p);r.rect(p.x-1,p.y-1,3,3,cream);
   }
  }
  const Pt corners[]={point(field.unproject(0,field.top)),point(field.unproject(field.width,field.top)),
                      point(field.unproject(field.width,field.bottom)),point(field.unproject(0,field.bottom))};
  for(int i=0;i<4;++i){auto a=corners[i],b=corners[(i+1)%4];r.line(a.x,a.y,b.x,b.y,cream);}
  r.clip=clip;
 }
 void battle_overlays(Raster& r,bool compact){int bottom=visible_bottom(),battle_width=field.width;char b[160];std::snprintf(b,sizeof b,"%s",world.active_objective());if(world.extra.mode)std::snprintf(b,sizeof b,"守备 %u 波 · 纪录 %u",world.extra.cleared,defense_best);else if(world.chapter==3)std::snprintf(b,sizeof b,"电台 %d / 2 · 军工厂 %s",world.captured_nodes(),world.count(factory,1)?"待摧毁":"已摧毁");else if(world.chapter==4)std::snprintf(b,sizeof b,"电台 %d / 2 · 车队 %u / 2",world.captured_nodes(),unsigned(__builtin_popcount(world.mission.delivered)));else if(world.chapter==5)std::snprintf(b,sizeof b,"节点 %d / 3 · %s",world.captured_nodes(),world.mission.stage<2?"核心屏障开启":"核心屏障解除");else if(world.chapter==1)std::snprintf(b,sizeof b,"中继站 %s · 撤离 %02u / 90",world.count(relay,0)?"已占领":"待占领",std::min(90u,unsigned(world.radio_time)));
  if(readable_ui()){
   int line=compact_layout().line,right=minimap_visible()?minimap().x-10:battle_width-8;StoryText goal(r,{b,"",""},right-24);Rect objective{8,field.top+6,right-8,int(std::min(2u,goal.count))*goal.row(r)+8};MissionSkin::draw(r,objective,0);if(button_count<int(buttons.size()))buttons[button_count++]={objective,objective_focus};
   goal.draw(r,{objective.x+8,objective.y+4,objective.w-16,objective.h-8},0);
   int command_y=objective.y+objective.h+4;
   if(world.count(hq,1)||world.extra.mode){int left=std::max(0,int(std::ceil(world.next_wave-world.time)));if(world.extra.mode&&world.army(1))std::snprintf(b,sizeof b,"第%d波 · 敌军%d",world.wave,world.army(1));else std::snprintf(b,sizeof b,"%s %02d秒",world.wave_direction(world.wave+1),left);Rect wave{8,command_y,right-8,line};r.shade(wave,navy,12);r.label(wave,b,left<=12?red:gold,12,0);command_y+=line+4;}
   if(!compact)transport_button(r,{8,command_y,88,line+8});
   if(!dock.open()&&(selection.armed||tactical_hint()||world.notice_time>0)){const char* message=selection.armed?"拖动框选，松手后点地面下令。":world.notice_time>0?world.notice:tactical_hint();StoryText notice(r,{message,"",""},battle_width-40);int rows=std::min(3u,notice.count),height=rows*notice.row(r)+8;Rect area{8,bottom-height-6,battle_width-16,height};MissionSkin::draw(r,area,1);notice.draw(r,{area.x+12,area.y+4,area.w-24,height-8},0);}
   show_build_grid(r);return;
  }
  MissionSkin::draw(r,{7,field.top+5,battle_width-104,26},0);if(button_count<int(buttons.size()))buttons[button_count++]={{7,field.top+5,battle_width-104,26},objective_focus};r.label({17,field.top+6,battle_width-125,24},b,cream,12,0);
  if(world.count(hq,1)||world.extra.mode){int left=std::max(0,int(std::ceil(world.next_wave-world.time)));if(world.extra.mode&&world.army(1))std::snprintf(b,sizeof b,"第%d波 / 敌军%d",world.wave,world.army(1));else std::snprintf(b,sizeof b,"%s %02d秒",world.wave_direction(world.wave+1),left);r.shade({10,field.top+31,111,18},navy,12);r.text(15,field.top+32,b,left<=12?red:gold,11);}
  if(!compact){
   tactical_buttons(r,{129,field.top+32,72,24},{207,field.top+32,72,24});
   support_button(r,{285,field.top+32,75,24});
   transport_button(r,wide_ui()?Rect{366,field.top+32,82,24}:Rect{129,field.top+58,82,24});
  }
  if(compact&&dock.open())return;int toast_height=compact?compact_layout().line:27;
  if(selection.armed){int y=bottom-toast_height-6;MissionSkin::draw(r,{8,y,battle_width-16,toast_height},1);r.label({14,y+2,battle_width-28,toast_height-2},"拖动框选 · 松手后下令 · 再点框选取消",cyan,13,0);}else if(tactical_hint()||world.notice_time>0){int y=bottom-toast_height-6;MissionSkin::draw(r,{8,y,battle_width-16,toast_height},1);r.label({14,y+2,battle_width-28,toast_height-2},world.notice_time>0?world.notice:tactical_hint(),cream,13,0);}else if(!compact)r.label({10,bottom-26,battle_width-20,23},compact?"拖动平移 · 点地面移动 / 点敌人攻击":"拖动战场 · 点选部队 · 点地面移动 / 点敌人攻击",cream,11,0);
  show_build_grid(r);
 }
 #include "production_ui.inc"
 #include "squads_ui.inc"
 #include "tactical_ui.inc"
 void compact_hud(Raster& r){auto layout=compact_layout();char b[200];
  battle_overlays(r,true);
  if(dock.open()){
   auto panel_area=layout.overlay(dock.panel);r.box(panel_area,navy,edge);r.rect(8,panel_area.y,88,2,cyan);
   if(dock.panel==DockPanel::tactics){
    int selected=world.selected();std::snprintf(b,sizeof b,"已选 %d / 部队 %d",selected,world.army());
    if(selected==1)for(auto& a:world.e)if(a.hp>0&&a.selected)std::snprintf(b,sizeof b,"%s · 耐久 %u · %s",specs[a.kind].name,unsigned(std::ceil(a.hp)),world.in_cover(a)?"掩护":world.paved(a.p)?"公路":"野地");
   }else if(dock.panel==DockPanel::squads)std::snprintf(b,sizeof b,"点队号召回，再点定位");
   else if(dock.panel==DockPanel::units)std::snprintf(b,sizeof b,"%s · 队列 %d / 6",production_label(),world.job_count);
   else std::snprintf(b,sizeof b,"%s · 电力 %+d",construction_label(),world.power_net);
   bool building=dock.panel==DockPanel::buildings;
   bool tactics=dock.panel==DockPanel::tactics;
   r.label({12,panel_area.y+4,w-(tactics?(world.selected_commander()>=0?364:276):building?193:24),layout.line},world.notice_time>0?world.notice:b,world.notice_time>0?gold:cyan,12,0);
   if(building)button(r,{w-176,panel_area.y+2,78,layout.line+4},construction_page,combined_page?"基础":"海空",combined_page);
   if(building&&world.build_active())button(r,{w-92,panel_area.y+2,84,layout.line+4},cancel_build,"撤建");
   if(tactics){
    support_button(r,{w-342,panel_area.y+2,80,layout.line+4});
    tactical_buttons(r,{w-256,panel_area.y+2,76,layout.line+4},{w-174,panel_area.y+2,76,layout.line+4});
    button(r,{w-92,panel_area.y+2,84,layout.line+4},squads_open,"编队");
   }
   if(dock.panel==DockPanel::squads){
    for(int i=0;i<squad_count;++i)squad_cards(r,i,layout.tactic(i,dock.panel),layout.tactic(4+i,dock.panel));
    button(r,layout.tactic(7,dock.panel),dock_close,"收起");
   }else if(tactics){
    button(r,layout.tactic(0),box_select,selection.armed?"框选中":"框选");button(r,layout.tactic(1),base,"回基地");button(r,layout.tactic(2),zooming,"缩放");button(r,layout.tactic(3),research_open,"科研");
    button(r,layout.tactic(4),repair,"维修",false,world.selected()>0);
    if(world.strike_cd>0)std::snprintf(b,sizeof b,"%u秒",unsigned(std::ceil(world.strike_cd)));else std::snprintf(b,sizeof b,"空袭");button(r,layout.tactic(5),strike,b,false,world.strike_cd<=0&&world.credits>=180);
    button(r,layout.tactic(6),cancel,placing>=0?"取消部署":"撤单",false,placing>=0||world.job_count>0);if(transport_available())transport_button(r,layout.tactic(7));else button(r,layout.tactic(7),dock_close,"收起");
   }else{
    for(int j=0;j<5;++j){
     int k=production_card(building,j);if(k<0)continue;
     auto card=layout.card(j,dock.panel);auto state=production_state(k);
     if(button_count<int(buttons.size()))buttons[button_count++]={card,produce0+k};
     Skin::card_frame(r,card,state.active);
     auto picture=layout.image(j,dock.panel);Skin::well(r,picture);
     production_picture(r,k,{picture.x+2,picture.y+2,picture.w-4,picture.h-4},state);
     r.label(layout.name(j,dock.panel),short_title(k),state.enabled?cream:muted,12);
     if(state.queued){std::snprintf(b,sizeof b,"%d",state.queued);r.label({card.x+card.w-18,card.y+4,14,layout.line},b,cyan,12);}
     if(state.active)Skin::progress(r,layout.progress(j,dock.panel),state.progress,world.build_ready()&&building?gold:cyan);
     std::snprintf(b,sizeof b,"%d",specs[k].cost);
     r.label(layout.price(j,dock.panel),state.text?state.text:b,state.enabled?(state.active||world.credits>=specs[k].cost?gold:red):muted,12);
    }
   }
  }
  r.rect(0,layout.top,w,h-layout.top,navy);r.rect(0,layout.top,w,1,edge);
  if(dock.panel==DockPanel::units)std::snprintf(b,sizeof b,"收起");else if(world.job_count)std::snprintf(b,sizeof b,"生产 %d",world.job_count);else std::snprintf(b,sizeof b,"生产");button(r,layout.command(0),units,b,dock.panel==DockPanel::units);
  button(r,layout.command(1),placing>=0?cancel:buildings,placing>=0?"取消":dock.panel==DockPanel::buildings?"收起":construction_label(),dock.panel==DockPanel::buildings);
  button(r,layout.command(2),select_army,"全军",true);button(r,layout.command(3),stop_army,"停止");button(r,layout.command(4),tactical!=TacticalMode::none?cancel_tactic:tactics_open,
    tactical!=TacticalMode::none?"取消":dock.panel==DockPanel::tactics?"收起":"战术",
    dock.panel==DockPanel::tactics||dock.panel==DockPanel::squads);
  if(world.job_count){auto a=layout.command(0);auto& job=world.jobs[0];Skin::progress(r,{a.x+4,a.y+a.h-4,a.w-8,2},1-job.left/job.total,cyan);}
 }
 #include "large_hud.inc"
 void hud(Raster& r){
  r.clip={0,0,w,h};
  // World labels are drawn in the native text pass. Keep them off the HUD.
  int top=compact_ui()?field.top:field.top+57,bottom_clip=visible_bottom();
  for(unsigned i=0;i<native_text.count;++i){
   auto& clip=native_text.commands[i].clip;
   int end=std::min(clip.y+clip.h,bottom_clip);
   clip.y=std::max(clip.y,top);
   clip.h=std::max(0,end-clip.y);
  }
  if(wide_ui()){large_hud(r);return;}
  topbar(r);if(minimap_visible())draw_minimap(r);if(selection_visible())button(r,selection_button(),box_select,selection.armed?"框选中":"框选",selection.armed);if(compact_ui()){compact_hud(r);return;}HudLayout layout(h);int bottom=layout.top;bool compact=display.width<400;char b[160];
  r.rect(0,bottom,w,h-bottom,navy);r.rect(0,bottom,w,2,edge);r.rect(8,bottom,56,2,cyan);r.rect(72,bottom,w-152,1,rgb(104,124,113));
  button(r,{8,bottom+8,56,25},units,"生产",tab==0);button(r,{8,bottom+38,56,25},buildings,construction_label(),tab==1);
  if(!tab&&world.selected_producer()<0)r.label({76,bottom+10,w-158,40},production_label(),muted,12,0);
  for(int j=0;j<5;++j){
   int k=production_card(tab==1,j);if(k<0)continue;
   auto state=production_state(k);auto card=layout.card(j);
   if(button_count<int(buttons.size()))buttons[button_count++]={card,produce0+k};
   Skin::card_frame(r,card,state.active);
   auto picture=layout.image(j);Skin::well(r,picture);
   production_picture(r,k,{picture.x+2,picture.y+2,picture.w-4,picture.h-4},state);
   r.label(layout.name(j),compact?short_title(k):specs[k].name,state.enabled?cream:muted,11);
   if(state.active)Skin::progress(r,layout.progress(j),state.progress,cyan);
   std::snprintf(b,sizeof b,"%d",specs[k].cost);
   r.label(layout.price(j),state.text?state.text:b,state.enabled?gold:muted,10);
  }
  button(r,{w-76,bottom+8,68,25},select_army,"全军",true);button(r,{w-76,bottom+38,68,25},placing>=0?cancel:base,placing>=0?"取消":compact?"归位":"回基地");
  button(r,{8,bottom+68,56,25},research_open,"科研");button(r,{w-76,bottom+68,68,25},squads_open,"编队",dock.open());if(tab)button(r,{w-76,bottom+94,68,24},construction_page,combined_page?"基础":"海空");if(layout.card_height>90){std::snprintf(b,sizeof b,"火%d 甲%d",world.extra.damage,world.extra.armor);r.label({8,bottom+97,56,18},b,gold,10);}
  int command_y=layout.commands;
  if(world.job_count){auto& j=world.jobs[0];std::snprintf(b,sizeof b,"生产 %s",compact?short_title(j.kind):specs[j.kind].name);r.text(9,command_y+2,b,cream,11);Skin::progress(r,{158,command_y+11,38,4},1-j.left/j.total,cyan);}else {std::snprintf(b,sizeof b,compact?"选%d / 军%d":"已选 %d / 部队 %d",world.selected(),world.army());if(world.selected()==1)for(auto& a:world.e)if(a.hp>0&&a.selected){if(production_building(a.kind))std::snprintf(b,sizeof b,compact?"点地面设集结":"点空地设置集结点");else if(combat_kind(a.kind)){if(compact)std::snprintf(b,sizeof b,"%s %u星",short_title(a.kind),veteran_rank(a));else std::snprintf(b,sizeof b,"%s · %u星 · %u击毁",specs[a.kind].name,veteran_rank(a),unsigned(a.kills));}}r.text(9,command_y+3,b,muted,11);}
  button(r,{202,command_y,52,24},stop_army,"停止");button(r,{264,command_y,48,24},tab&&placing<0?cancel_build:cancel,tab&&placing<0?"撤建":"撤单");button(r,{317,command_y,48,24},repair,"维修");if(world.strike_cd>0){std::snprintf(b,sizeof b,"%02u秒",unsigned(std::ceil(world.strike_cd)));button(r,{370,command_y,50,24},strike,b,false,false);}else button(r,{370,command_y,50,24},strike,"空袭");button(r,{425,command_y,47,24},zooming,"缩放");
  battle_overlays(r,false);
  if(dock.panel==DockPanel::squads){
   int y=visible_bottom(),column=(w-32)/3;
   r.box({0,y,w,104},navy,edge);
   r.label({10,y+4,w-20,22},"点队号召回，再点定位",cyan,12,0);
   for(int i=0;i<squad_count;++i)squad_cards(r,i,{10+i*column,y+29,column-6,28},{10+i*column,y+62,column-6,28});
  }
  r.rect(8,h-20,w-16,1,edge);int selected=world.selected();const Entity* single=nullptr;if(selected==1)for(auto& a:world.e)if(a.hp>0&&a.selected)single=&a;
  if(single){if(single->kind==harvester)std::snprintf(b,sizeof b,"矿车 · 载矿 %u",unsigned(single->cargo));else std::snprintf(b,sizeof b,"%s · 耐久 %u",specs[single->kind].name,unsigned(std::ceil(single->hp)));}else if(selected>1){int soldiers=0,tanks=0,guns=0;for(auto& a:world.e)if(a.hp>0&&a.selected){soldiers+=a.kind<=rocket;tanks+=a.kind==tank;guns+=a.kind==artillery;}std::snprintf(b,sizeof b,"%d 步兵   %d 坦克   %d 火炮",soldiers,tanks,guns);}else std::snprintf(b,sizeof b,"守住蓝色补给站五秒可获得矿石与维修");r.label({10,h-18,w-20,18},b,muted,10,0);
 }
 void title_background(Raster& r){field.ground.release();bool cached=field.cache_ground;field.cache_ground=false;int oldtop=field.top,oldbottom=field.bottom,oldwidth=field.width;field.width=w;float cx=field.camx,cy=field.camy,oldzoom=field.zoom;field.zoom=std::clamp(w/420.f,1.25f,1.8f);field.top=0;field.bottom=h;field.focus(world.deployment_view());field.camx+=30;field.draw(r,world);field.top=oldtop;field.bottom=oldbottom;field.width=oldwidth;field.camx=cx;field.camy=cy;field.zoom=oldzoom;field.cache_ground=cached;
  for(int y=0;y<h;++y){int opacity=y<h/2?10-y*4/(h/2):6+(y-h/2)*6/(h/2);r.shade({0,y,w,1},navy,std::clamp(opacity,0,16));}
 }
 void page_header(Raster& r,const char* title,const char* subtitle){r.rect(0,0,w,h,navy);int title_height=readable_ui()?r.text_height(title,25):30;r.rect(15,13,3,title_height,cyan);r.label({28,7,w-56,title_height},title,cream,25,0);r.label({29,readable_ui()?title_height+16:41,w-58,readable_ui()?r.text_height(subtitle,12):16},subtitle,muted,12,0);}
 void home_screen(Raster& r){title_background(r);r.native_text=&native_text;int row=readable_ui()?page_row(r):39,small_row=readable_ui()?row:31,gap=10,y=readable_ui()?h-row-small_row-gap-16:std::max(214,h-128),logo_width=std::min(w-64,640),x=(w-480)/2;
  Sprites::fit(r,art::wordmark_first,{(w-logo_width)/2,20,logo_width,std::min(152,y-58)});Skin::panel_frame(r,{x+12,y-12,456,row+small_row+gap+24},navy,edge);
  button(r,{x+24,y,246,row},start,career.unlocked?"继续战役  >":"开启战役  >",true);button(r,{x+280,y,176,row},resume,"返回战场",false,has_run);button(r,{x+24,y+row+gap,102,small_row},missions,"战役档案");button(r,{x+134,y+row+gap,102,small_row},endless,"无尽守备");button(r,{x+244,y+row+gap,102,small_row},help,"指挥手册");button(r,{x+354,y+row+gap,102,small_row},difficulty,career.easy?"休闲":"标准");
  if(!readable_ui())r.text(w-18,h-17,"1.7.2",muted,10,2);
 }
 void campaign_screen(Raster& r){page_header(r,"战役档案",campaign_page?"第二幕 / 把城市还给居民":"第一幕 / 重建求救的信号");int row=readable_ui()?page_row(r):30,top=readable_ui()?page_top(r)+8:74,footer=readable_ui()?h-2*row-28:h-91,step=std::min(112,(footer-top)/3);char b[80];
  for(int k=0;k<3;++k){int j=campaign_page*3+k,y=top+k*step;auto& c=chapters[j];bool locked=unsigned(j)>career.unlocked;Skin::panel_frame(r,{20,y,w-40,step-8},panel,locked?edge:cyan);int name_y=y+5+(readable_ui()?r.text_height(c.place,12)+4:18);r.label({34,y+5,w-160,readable_ui()?r.text_height(c.place,12):16},c.place,locked?muted:gold,12,0);r.label({34,name_y,w-160,readable_ui()?r.text_height(c.name,12):24},c.name,locked?muted:cream,18,0);if(!readable_ui()&&step>=76)r.text(34,y+49,c.map,muted,12);std::snprintf(b,sizeof b,"%s",locked?"待解锁":career.wins&(1u<<j)?"重温":"出发");button(r,{w-113,y+8,77,readable_ui()?std::min(row,step-20):30},mission0+j,b,!locked,!locked);}
  int tab_y=readable_ui()?footer+4:h-83;button(r,{24,tab_y,126,readable_ui()?row:29},campaign_prev,"第一幕",campaign_page==0);button(r,{w-150,tab_y,126,readable_ui()?row:29},campaign_next,"第二幕",campaign_page==1);button(r,{20,readable_ui()?h-row-12:h-43,w-40,row},home,"返回指挥部");
 }
 void briefing_screen(Raster& r){auto& c=chapters[world.chapter];page_header(r,world.extra.mode?"最后一班补给车":c.name,world.extra.mode?"无尽守备 / 北境临时前哨":c.place);unsigned person=world.extra.mode?1:c.speaker;bool standing=w>=720&&h>=400&&person<2,small=display.width<400;int row=page_row(r),portrait_y=readable_ui()?page_top(r)+8:76,tx=standing?224:small?24:94,ty=readable_ui()?portrait_y+r.text_height("中",14)+10:small?130:104,yy=readable_ui()?h-row-136:h-143,mx=standing?224:24;
  if(standing){Skin::well(r,{24,72,176,h-165});Sprites::fit(r,art::story_standing_first+(person==1?1:0),{28,76,168,h-173});}else portrait(r,24,portrait_y,small?44:56,person);r.text(standing?224:small?84:94,readable_ui()?portrait_y:78,cast_names[person],cyan,14);
  auto paragraphs=world.extra.mode?std::array<const char*,3>{"补给车还在风雪里，前哨就是他们的路标。","守住指挥中心，击退每一波灰塔袭击。","利用科研和战地补给，把防线坚持得更久。"}:c.briefing;Rect area{tx,ty,w-tx-24,yy-ty-8};StoryText text(r,paragraphs,area.w);brief_pages=text.pages(r,area);brief_page=std::clamp(brief_page,0,brief_pages-1);text.draw(r,area,brief_page);
  if(w>=720&&h>=440){int used=std::min(int(text.capacity(r,area)),int(text.count)-brief_page*int(text.capacity(r,area)));int my=ty+used*text.row(r)+28,ms=std::min(128,yy-my-8);if(ms>=64){Rect map{w-ms-28,my,ms,ms};Skin::well(r,{map.x-4,map.y-4,ms+8,ms+8});minimap_cache.draw(r,world,map,true);for(auto& a:world.e)if(a.hp>0&&(a.kind==hq||a.kind==relay))r.rect(map.x+a.p.x*ms/world.side-2,map.y+a.p.y*ms/world.side-2,4,4,a.kind==relay?gold:a.team?red:cyan);char size[32];std::snprintf(size,sizeof size,"%d×%d",world.side,world.side);r.text(map.x,map.y-20,size,muted,12);}}
  Skin::panel_frame(r,{mx,yy,w-mx-24,readable_ui()?104:65});char label[64];std::snprintf(label,sizeof label,"作战目标 · 简报 %d/%d",brief_page+1,brief_pages);r.text(mx+14,yy+8,label,gold,12);const char* goal=world.extra.mode?"守住指挥中心；每五波迎战重装突袭":world.active_objective();if(readable_ui()){StoryText objective(r,{goal,"",""},w-mx-52);objective.draw(r,{mx+14,yy+40,w-mx-52,60},0);}else r.label({mx+14,yy+28,w-mx-52,27},goal,cream,14,0);button(r,{24,readable_ui()?h-row-16:h-64,w-(readable_ui()?145:121),readable_ui()?row:40},brief_page+1<brief_pages?brief_next:begin,brief_page+1<brief_pages?"继续简报  >":"部署部队 / 开始行动",true);button(r,{w-(readable_ui()?109:85),readable_ui()?h-row-16:h-64,readable_ui()?85:61,readable_ui()?row:40},brief_page?brief_previous:home,brief_page?"上页":"返回");}
 void radio_screen(Raster& r){field.draw(r,world);r.clip={0,0,w,h};r.shade({0,0,w,h},navy,12);int yy=15;Skin::panel_frame(r,{20,yy,w-40,h-30});r.native_text=&native_text;auto& script=world.authored_map()&&radio_id<3?new_route_radio[radio_id]:radio_scripts[radio_id];portrait(r,36,yy+18,72,script.speaker);r.text(124,yy+22,cast_names[script.speaker],cyan,14);int subtitle=readable_ui()?yy+26+r.text_height("中",14):yy+48;r.label({124,subtitle,w-160,r.text_height("中",12)},"战地通信 / 战局已暂停",muted,12,0);int row=page_row(r),button_y=readable_ui()?h-row-20:h-65,top=std::max(yy+106,subtitle+r.text_height("中",12)+12);Rect area{38,top,w-76,button_y-top-12};StoryText text(r,script.lines,area.w);radio_pages=text.pages(r,area);radio_page=std::clamp(radio_page,0,radio_pages-1);text.draw(r,area,radio_page);button(r,{38,button_y,w-76,row},radio_continue,radio_page+1<radio_pages?"继续收听  >":"收到 / 继续行动",true);}
 void pause_screen(Raster& r){field.draw(r,world);r.clip={0,0,w,h};r.shade({0,0,w,h},navy,12);r.native_text=&native_text;int row=page_row(r),title=r.text_height("中",26),caption=r.text_height("中",13),height=readable_ui()?title+caption+4*row+80:248,yy=(h-height)/2,x=readable_ui()?32:70,inner=x+22,iw=w-2*inner,first=readable_ui()?yy+title+caption+36:yy+80;
  Skin::panel_frame(r,{x,yy,w-2*x,height},navy,edge);r.text(w/2,yy+15,"战术暂停",cream,26,1);r.text(w/2,readable_ui()?yy+title+22:yy+51,"战局已保存 · 时间已停止",muted,13,1);
  button(r,{inner,first,iw,readable_ui()?row:34},resume,"继续行动",true);button(r,{inner,readable_ui()?first+row+8:yy+123,iw,readable_ui()?row:30},help,"指挥手册");int third=readable_ui()?first+2*(row+8):yy+161;button(r,{inner,third,(iw-10)/2,readable_ui()?row:30},restart,"重新部署");button(r,{inner+(iw+10)/2,third,(iw-10)/2,readable_ui()?row:30},sound_toggle,sounds.enabled?"声音：开":"声音：关");button(r,{inner,readable_ui()?first+3*(row+8):yy+201,iw,readable_ui()?row:30},save_return,"保存并返回指挥部");}

 void help_screen(Raster& r){page_header(r,"指挥手册","每一条指令，都通向黎明");bool compact=h<390;int top=64,step=std::min(55,(h-118)/5);
  const char* titles[]={"01 / 编队指挥","02 / 经济与建设","03 / 道路与补给","04 / 掩护与老兵","05 / 火力与支援"};
  const char* body[]={"选兵后可编队、护卫车辆或指定撤退。","矿车自动采集；建筑就绪后再选地部署。","公路加速车辆；守补给五秒获矿石维修。","步兵借林地掩护，击毁敌军晋星。","破甲反装甲，火炮溅射；空袭破防。"};
  if(readable_ui()){
   int row=page_row(r),y=page_top(r)+8,bottom=h-row-22;std::array<int,5> starts{},ends{};int pages=0;
   for(int j=0;j<5;){starts[pages]=j;int used=y;while(j<5){StoryText paragraph(r,{body[j],"",""},w-48);int needed=r.text_height(titles[j],15)+8+int(paragraph.count)*paragraph.row(r)+12;if(used+needed>bottom&&j>starts[pages])break;used+=needed;++j;}ends[pages++]=j;}
   help_page=std::clamp(help_page,0,pages-1);for(int j=starts[help_page];j<ends[help_page];++j){r.text(24,y,titles[j],cyan,15);y+=r.text_height(titles[j],15)+8;StoryText paragraph(r,{body[j],"",""},w-48);paragraph.draw(r,{24,y,w-48,int(paragraph.count)*paragraph.row(r)},0);y+=int(paragraph.count)*paragraph.row(r)+12;}
   int bw=(w-64)/3;button(r,{24,h-row-12,bw,row},help_previous,"上页",false,help_page>0);button(r,{32+bw,h-row-12,bw,row},has_run?resume:home,"返回");button(r,{40+2*bw,h-row-12,bw,row},help_next,"下页",true,help_page+1<pages);return;
  }
  for(int j=0;j<5;++j){r.text(24,top+j*step,titles[j],cyan,compact?13:15);r.label({24,top+j*step+21,w-48,20},body[j],cream,compact?12:14,0);}button(r,{24,h-43,w-48,30},has_run?resume:home,has_run?"返回战场":"返回指挥部");}
 void research_screen(Raster& r){page_header(r,"战地科研","本战局科技 / 战局继续进行");char text[120];
  if(readable_ui()){
   int row=page_row(r),line=r.text_height("中",12),top=page_top(r)+8;r.label({24,top,w-48,line},"工厂建成后可研发两级科技",muted,12,0);top+=line+12;int step=(h-top-row-66)/2;
   for(int k=1;k<=2;++k){int y=top+(k-1)*step;Skin::panel_frame(r,{24,y,w-48,step-8});std::snprintf(text,sizeof text,"%s %u/2",k==1?"弹药":"装甲",world.tech_level(k));r.label({38,y+8,w-220,line},text,cyan,12,0);
    if(world.extra.research==k)std::snprintf(text,sizeof text,"%s %us",world.ready_count(factory)?world.power_net<0?"缺电":"剩余":"待工厂",unsigned(std::ceil(world.extra.research_left)));else std::snprintf(text,sizeof text,"%s",k==1?"火力 +12%":"减伤 10%");r.label({38,y+line+14,w-220,line},text,cream,12,0);
    if(world.tech_level(k)==2)std::snprintf(text,sizeof text,"已完成");else std::snprintf(text,sizeof text,"%d 矿石",world.research_cost(k));bool enabled=!world.research_blocked(k);button(r,{w-170,y+8,132,std::min(row,step-24)},k==1?research_ammo:research_armor,text,enabled,enabled);
    if(world.extra.research==k)Skin::progress(r,{38,y+step-14,w-76,4},1-world.extra.research_left/world.research_duration(k),cyan);
   }
   if(world.extra.research)button(r,{24,h-row-58,w-48,40},research_cancel,"取消研发 / 退还矿石");else{std::snprintf(text,sizeof text,"矿石 %d · 火力%u 装甲%u",world.credits,world.extra.damage,world.extra.armor);r.label({24,h-row-54,w-48,line},text,gold,12);}
   button(r,{24,h-row-12,w-48,row},resume,"返回战场",true);return;
  }
  r.text(24,66,"建成装甲工厂后，可研发两级作战科技。",muted,12);
  int step=std::min(100,(h-175)/2);for(int k=1;k<=2;++k){int y=94+(k-1)*step;Skin::panel_frame(r,{24,y,w-48,step-10});auto level=world.tech_level(k);r.text(38,y+7,k==1?"弹药改良":"复合装甲",cyan,18);std::snprintf(text,sizeof text,"%u / 2",level);r.text(w-145,y+9,text,gold,12);
   if(world.extra.research==k){if(!world.ready_count(factory))std::snprintf(text,sizeof text,"等待工厂重建");else std::snprintf(text,sizeof text,world.power_net<0?"缺电 · 剩余 %u 秒":"研发中 · 剩余 %u 秒",unsigned(std::ceil(world.extra.research_left)));r.text(38,y+34,text,gold,12);Skin::progress(r,{38,y+56,w-210,5},1-world.extra.research_left/world.research_duration(k),cyan);}else r.text(38,y+34,k==1?(display.width<400?"火力 +12% / 级":"每级增加作战火力 12%"):(display.width<400?"减伤 10% / 级":"每级减少所受伤害 10%"),cream,12);
   bool enabled=!world.research_blocked(k);if(level==2)std::snprintf(text,sizeof text,"已完成");else std::snprintf(text,sizeof text,"%d 矿石",world.research_cost(k));button(r,{w-133,y+30,95,30},k==1?research_ammo:research_armor,text,enabled,enabled);
  }
  if(world.extra.research)button(r,{24,h-89,w-48,30},research_cancel,"取消研发 / 全额退回");else {std::snprintf(text,sizeof text,"可用矿石 %d · 火力 %u级 / 装甲 %u级",world.credits,world.extra.damage,world.extra.armor);r.label({24,h-90,w-48,30},text,gold,12);}
  button(r,{24,h-48,w-48,32},resume,"返回战场",true);
 }
 void result_screen(Raster& r){title_background(r);r.native_text=&native_text;bool won=world.won;
  if(readable_ui()){
   int row=page_row(r),title=r.text_height("中",28),line=r.text_height("中",12);r.label({24,24,w-48,title},world.extra.mode?"前哨守备结束":won?"作战成功":"战线失守",won?cyan:gold,28);int y=title+40;if(!world.extra.mode){r.label({24,y,w-48,line},chapters[world.chapter].name,cream,12);y+=line+16;int stars=won?(world.lost<4?3:world.lost<10?2:1):0;for(int j=0;j<3;++j)insignia(r,w/2+(j-1)*40,y+12,12,j<stars?gold:edge);y+=32;}
   StoryText ending(r,{world.extra.mode?"补给信号仍在。下一班车会记得这条路。":won?chapters[world.chapter].ending:"守住矿线，补足电力。北境仍在等待你。","",""},w-48);ending.draw(r,{24,y,w-48,int(ending.count)*ending.row(r)},0);y+=int(ending.count)*ending.row(r)+16;char text[120];if(world.extra.mode)std::snprintf(text,sizeof text,"击退 %u 波 · 纪录 %u 波",world.extra.cleared,defense_best);else std::snprintf(text,sizeof text,"作战 %02u:%02u · 击毁 %d · 损失 %d",unsigned(world.time)/60,unsigned(world.time)%60,world.killed,world.lost);r.label({24,y,w-48,line},text,gold,12);if(!world.extra.mode){std::snprintf(text,sizeof text,"采矿 %d",world.earned);r.label({24,y+line+8,w-48,line},text,gold,12);}button(r,{24,h-2*row-22,w-48,row},world.extra.mode?restart:won?next:restart,world.extra.mode?"再次守备":won?"继续前进":"重新部署",true);button(r,{24,h-row-12,w-48,row},home,"返回指挥部");return;
  }
  if(world.extra.mode){r.text(w/2,35,"前哨守备结束",gold,28,1);r.text(w/2,88,"补给信号仍在。下一班车会记得这条路。",cream,14,1);char text[100];std::snprintf(text,sizeof text,"击退 %u 波 · 历史纪录 %u 波",world.extra.cleared,defense_best);r.label({24,144,w-48,40},text,cyan,20);button(r,{24,h-87,w-48,36},restart,"再次守备",true);button(r,{24,h-41,w-48,28},home,"返回指挥部");return;}r.text(w/2,29,won?"作战成功":"战线失守",won?cyan:red,34,1);r.text(w/2,82,chapters[world.chapter].name,cream,20,1);int stars=won?(world.lost<4?3:world.lost<10?2:1):0;for(int j=0;j<3;++j)insignia(r,w/2+(j-1)*40,130,12,j<stars?gold:edge);
  r.label({24,153,w-48,32},won?chapters[world.chapter].ending:"守住矿线，补足电力。北境仍在等待你。",cream,14);
  char b[120];std::snprintf(b,sizeof b,"作战 %02u:%02u    击毁 %d    损失 %d    采矿 %d",unsigned(world.time)/60,unsigned(world.time)%60,world.killed,world.lost,world.earned);r.label({24,194,w-48,27},b,gold,14);button(r,{24,h-87,w-48,36},won?next:restart,won?(world.chapter==CHAPTERS-1?"战役档案":"下一章 / 继续前进"):"重新部署",true);button(r,{24,h-41,w-48,28},home,"返回指挥部");}
 void render(){if(!ready||!surface||!ctx->foreground())return;
#ifdef IRON_PROFILE
  auto perf_begin=RenderProfile::now();
#endif
auto lease=surface->acquire();if(!lease)return;auto dest=lease->pixels();int stride=lease->stride_bytes()/2;auto output=reinterpret_cast<uint16_t*>(dest.data());bool direct=display_plan.width==w&&display_plan.height==h;
field.clock=(screen==Screen::battle||screen==Screen::pause||screen==Screen::radio)?world.time:clock;native_text.reset(display_plan.width,display_plan.height,w,h,display_plan.text_scale(w),readable_ui()?display_plan.minimum_text_scale():1);Raster r(direct?output:pixels.data(),w,h,direct?stride:w);if(screen==Screen::battle||screen==Screen::campaign||screen==Screen::briefing||screen==Screen::help||screen==Screen::research)r.native_text=&native_text;if(screen!=Screen::battle)r.rect(0,0,w,h,navy);button_count=0;switch(screen){case Screen::radio:radio_screen(r);break;case Screen::research:research_screen(r);break;case Screen::home:home_screen(r);break;case Screen::campaign:campaign_screen(r);break;case Screen::briefing:briefing_screen(r);break;case Screen::battle:field.draw(r,world);selection.draw(r,world,field);hud(r);break;case Screen::pause:pause_screen(r);break;case Screen::help:help_screen(r);break;case Screen::result:result_screen(r);break;}
#ifdef IRON_PROFILE
  auto perf_drawn=RenderProfile::now();
#endif
  if(!direct)frame_scaler.draw(output,stride,pixels.data(),w);
  Raster lettering(output,display_plan.width,display_plan.height,stride);draw_native_text(native_text,lettering);
  auto shown=lease->present(++frame_id);if(!shown&&shown.error()!=pxa::Error::would_block)log("Iron Dawn present failed");
#ifdef IRON_PROFILE
  if(shown){static RenderProfile profile;profile.present(*ctx,perf_begin,perf_drawn,RenderProfile::now());}
#endif
 }
};
}
PXA_APPLICATION(iron::App)
