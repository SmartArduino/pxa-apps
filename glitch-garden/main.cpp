// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/surface.hpp>
#include "model.hpp"
#include "art.hpp"
#include "art_data.hpp"
#include "paint.hpp"
#include "layout.hpp"
#include "perf.hpp"
#include "scene.hpp"
#include "hud_underlay.hpp"
#include "hud.hpp"
#ifdef GARDEN_BENCHMARK
#include "tests/dense_scene.hpp"
#endif
#include "../common/motion-games/sound.hpp"
#include <cstdio>
#include <time.h>

namespace garden {

constexpr auto cream=rgb(249,241,205),gold=rgb(239,196,90),forest=rgb(23,49,40),moss=rgb(54,87,51),border=rgb(119,143,69),red=rgb(210,100,74);
constexpr size_t memory_limit=6*1024*1024;
enum class Screen {home,play,pause,help,won,lost};

struct App {
 pxa::Context* ctx=nullptr;pxa::ui::DisplayMetrics display;
 std::optional<pxa::Surface> surface;FilteredArt art;std::vector<uint16_t> backdrop,base_backdrop;uint64_t frame_id=1,paint_us=0,paint_max=0,paint_count=0;
 HudUnderlay hud_underlay;HudPatch currency_patch,progress_patch;uint64_t hud_key=~uint64_t(0),cards_key=~uint64_t(0);bool hud_valid=false;uint64_t shadow_key=0;bool backdrop_battle=false;
 FramePerf perf;uint64_t perf_report=0,benchmark_start=0;
 FrameRestore frame_restore;Rect board_damage_bounds;uint64_t background_revision=1;
#ifdef GARDEN_HUD_BENCHMARK
 uint64_t card_request_us=0;
#endif
 motion::Sounds sounds;pxa::input::Controller controller;Game game;Layout layout;
 Screen screen=Screen::home,help_return=Screen::home;
 uint64_t last=0,now=0,repeat_at=0,help_pose=~uint64_t(0);float accumulator=0;
 float scale=1;int w=296,h=240,selection=2,row=2,col=0,menu=0,help_page=0,press_x=0,press_y=0,press_native_x=0,press_native_y=0;
 uint8_t pointer_id=0,unlocked=1;
 uint64_t backdrop_key=~uint64_t(0),startup_us=0;
 bool ready=false,dirty=true,touching=false,using_keys=false,saving=false,save_pending=false,exit_requested=false,backgrounded=false,wait_release=false,resize_pending=false,renderer_pending=false,backdrop_dirty=true;
 const char* notice="";float notice_time=0;
 void log(std::string_view s){(void)ctx->log().write(pxa::LogLevel::info,s);}
 void on_error(pxa::Context&,pxa::Error error){char text[80];std::snprintf(text,sizeof text,"Garden asynchronous error=%d",int(error));log(text);exit();}
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){ctx=&c;startup_us=monotonic_us();auto d=pxa::ui::decode_start_display(config);if(!d)return std::unexpected(d.error());display=*d;
  auto orientation=c.window().request_orientation(pxa::OrientationPreference::landscape);log(orientation?"Garden landscape requested":"Garden landscape unsupported; using actual display");
  (void)c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);return c.tasks().start(initialize());}
 pxa::Task<void> initialize(){std::array<std::byte,4> data{};auto saved=co_await ctx->storage().get("garden-progress-v1",data);
  if(saved&&*saved==4&&data[0]==std::byte{'G'}&&data[1]==std::byte{1}&&unsigned(data[2])>=1&&unsigned(data[2])<=stage_count&&data[3]==std::byte(unsigned(data[2])^0xa5))unlocked=unsigned(data[2]);
#ifdef GARDEN_TEST_CONTENT
  unlocked=stage_count;
#endif
  renderer_pending=true;co_return ctx->clock().set_period(33);}
 void defer_renderer(){pause();ready=false;resize_pending=true;renderer_pending=true;last=0;(void)ctx->clock().set_period(33);}
 void on_window_changed(const pxa::WindowMetrics& value){if(!value.pixel_width||!value.pixel_height)return;bool changed=display.width!=value.pixel_width||display.height!=value.pixel_height;display.width=value.pixel_width;display.height=value.pixel_height;if(changed&&ready&&!resize_pending)defer_renderer();}
 pxa::Task<void> open_renderer(){ready=false;surface.reset();std::vector<uint16_t>().swap(backdrop);std::vector<uint16_t>().swap(base_backdrop);art.clear_cache();hud_underlay={};currency_patch={};progress_patch={};hud_valid=false;touching=false;last=0;accumulator=0;
  if(art.bytes.empty()){art.bytes.resize(garden_art::pixel_bytes);if(!unpack(garden_art::packed_pixels,art.bytes))co_return std::unexpected(pxa::Error::protocol_error);}
  scale=std::max(1.f,std::min(float(display.width)/296.f,float(display.height)/240.f));w=std::lround(display.width/scale);h=std::lround(display.height/scale);layout_update();
  size_t pixels=size_t(display.width)*display.height,frame_memory=pixels*6,encoded=garden_art::packed_pixels.size()+garden_art::packed_backgrounds.size()+garden_art::packed_prepared.size()+65536;
  bool baked_geometry=false;for(auto b:garden_art::backgrounds)if(b.width==display.width&&b.height==display.height&&b.bx==layout.bx&&b.by==layout.by&&b.cell==layout.cell)baked_geometry=true;
  hud_underlay.configure(display.width,display.height,std::lround((layout.y+56)*scale),std::lround((layout.y+layout.h-38)*scale));
  size_t used=art.bytes.size()+frame_memory+encoded+262144+size_t(std::ceil(3720*scale*scale))*2+hud_underlay.pixels.size()*2+garden_art::source_rows.size()*sizeof(garden_art::SourceRow)+garden_art::source_first.size()*sizeof(unsigned);
  if(used+131072>memory_limit)co_return std::unexpected(pxa::Error::resource_limit);
  if(!baked_geometry&&used+pixels*2+512*1024<=memory_limit){base_backdrop.resize(pixels);used+=pixels*2;}
  art.cache_budget=std::min(size_t(1536*1024),size_t(memory_limit)-used);
  pxa::SurfaceOptions options{.width=uint16_t(display.width),.height=uint16_t(display.height),.buffers=2,.direct_scanout=true,.max_buffer_bytes=2*1024*1024};
  auto made=co_await ctx->surface().create_mapped(options);if(!made)co_return std::unexpected(made.error());surface.emplace(std::move(*made));
  auto configured=co_await surface->configure({.width=options.width,.height=options.height});if(!configured)co_return configured;
  if(options.width!=display.width||options.height!=display.height){renderer_pending=true;co_return ctx->clock().set_period(33);}
  backdrop.resize(size_t(display.width)*display.height,forest);backdrop_dirty=true;frame_restore={};background_revision=1;
  ready=true;dirty=true;resize_pending=false;(void)ctx->clock().set_period(33);char text[160];std::snprintf(text,sizeof text,"Garden ready display=%ux%u logical=%dx%d bilinear=1 field=%d,%d cell=%d",display.width,display.height,w,h,layout.bx,layout.by,layout.cell);render();log(text);std::snprintf(text,sizeof text,"Garden startup first_frame_us=%llu",monotonic_us()-startup_us);log(text);(void)ctx->tasks().start(open_audio());co_return pxa::Result<void>{};}
 pxa::Task<void> open_audio(){auto r=co_await sounds.open(*ctx);if(!r)log("Garden audio unavailable");co_return pxa::Result<void>{};}
 void layout_update(){auto previous_field=layout.field();auto inset=[&](uint32_t n){return logical_inset(n,scale);};
  int left=inset(display.safe.left),top=inset(display.safe.top),right=inset(display.safe.right),bottom=inset(std::max(display.safe.bottom,display.system_gestures_known?display.system_gestures.bottom:0u));
  if(display.shape==2){left=std::max(left,w*15/100);right=std::max(right,w*15/100);top=std::max(top,h*15/100);bottom=std::max(bottom,h*15/100);}
  layout.x=left;layout.y=top;layout.w=w-left-right;layout.h=h-top-bottom;layout.cell=std::max(8,std::min((layout.w-54)/columns,(layout.h-84)/rows));
  layout.bx=left+(layout.w-layout.cell*columns)/2+3;layout.by=top+53+std::max(0,(layout.h-84-layout.cell*rows)/2);board_damage_bounds=board_damage(layout,scale,display.width,display.height);keep_seed_visible();if(previous_field!=layout.field())backdrop_dirty=true;}
 void keep_seed_visible(){int visible=layout.seed_visible();layout.seed_offset=std::clamp(layout.seed_offset,0,plant_count-visible);if(selection>0){int index=selection-1;if(index<layout.seed_offset)layout.seed_offset=index;if(index>=layout.seed_offset+visible)layout.seed_offset=index-visible+1;}}
 void shift_seeds(int direction){int visible=layout.seed_visible(),before=layout.seed_offset;layout.seed_offset=std::clamp(before+direction,0,plant_count-visible);if(before==layout.seed_offset)return;if(selection>0)selection=std::clamp(selection-1,layout.seed_offset,layout.seed_offset+visible-1)+1;sounds.play(motion::Cue::click);dirty=true;log("Garden seed tray scrolled");}
 void save(){save_pending=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(save_pending){save_pending=false;std::array<std::byte,4> d{std::byte{'G'},std::byte{1},std::byte{unlocked},std::byte(unlocked^0xa5)};auto r=co_await ctx->storage().set("garden-progress-v1",d);log(r?"Garden progress saved":"Garden progress save failed");}saving=false;co_return pxa::Result<void>{};}
 void begin(int stage){(void)ctx->window().keep_screen_on(true);paint_us=paint_max=paint_count=0;perf={};perf_report=0;benchmark_start=monotonic_us();game.start(stage);screen=Screen::play;selection=2;layout.seed_offset=0;keep_seed_visible();last=0;accumulator=0;notice="";notice_time=0;menu=0;dirty=true;log("Garden begin");
#ifdef GARDEN_TEST_CONTENT
 game.stars=999;game.next_spawn=999;for(int c=0;c<plant_count;++c)game.place(c<4?1:3,(c%4)*2,Defender(c+1));for(int r=0;r<4;++r){game.spawn(r,EnemyKind(r));game.enemies[r].x=8.2f;}game.stars=999;
#endif
 }
 void feedback(const char* text){notice=text;notice_time=1.8f;dirty=true;}
 void plant(){bool done=selection==0?game.remove(row,col):game.place(row,col,Defender(selection));
  if(done){sounds.play(selection?motion::Cue::bamboo:motion::Cue::paper);char text[100];std::snprintf(text,sizeof text,selection?"Garden plant placed row=%d col=%d":"Garden plant removed row=%d col=%d",row,col);log(text);}
  else feedback(selection==0?"空地":game.plants[row*columns+col].kind!=Defender::none?"已有植物":"露珠不足");dirty=true;}
 void pause(){if(screen!=Screen::play)return;(void)ctx->window().keep_screen_on(false);screen=Screen::pause;menu=0;touching=false;sounds.stop();dirty=true;log("Garden paused");char text[160];std::snprintf(text,sizeof text,"Garden filter cpu avg_us=%llu max_us=%llu frames=%llu cache=%u hits=%llu misses=%llu",paint_count?paint_us/paint_count:0,paint_max,paint_count,unsigned(art.cache_bytes),art.hits,art.misses);log(text);}
 void resume(){art.release_hud_cache();(void)ctx->window().keep_screen_on(true);screen=Screen::play;last=0;accumulator=0;menu=0;dirty=true;log("Garden resumed");}
 void exit(){sounds.stop();exit_requested=true;if(!ctx->window().request_back())exit_requested=false;}
 pxa::BackAction on_back(){if(exit_requested)return pxa::BackAction::close;if(screen==Screen::play)pause();else if(screen==Screen::pause)resume();else if(screen==Screen::help){screen=help_return;dirty=true;}else if(screen==Screen::home)exit();else {screen=Screen::home;menu=0;dirty=true;}return pxa::BackAction::stay;}
 void activate(int index){sounds.play(motion::Cue::click);
  if(screen==Screen::home){if(index<stage_count){if(index<unlocked)begin(index);else feedback("选择关卡");}else {help_return=screen;help_page=0;screen=Screen::help;}}
  else if(screen==Screen::pause){if(index==0)resume();else if(index==1){help_return=screen;help_page=0;screen=Screen::help;}else if(index==2)begin(game.level);else if(index==3){screen=Screen::home;menu=0;}else exit();}
  else if(screen==Screen::help){screen=help_return;menu=0;}
  else if(index==0){if(screen==Screen::won)begin(std::min(stage_count-1,game.level+1));else begin(game.level);}else {screen=Screen::home;menu=0;}
  dirty=true;}
 int menu_count()const{return screen==Screen::home?stage_count+1:screen==Screen::pause?5:screen==Screen::help?1:2;}
 int modal_y()const{return layout.y+(layout.h-240)/2;}
 Rect sound_rect()const{return {layout.x+layout.w/2+47,modal_y()+209,52,20};}
 Rect menu_rect(int index)const{if(screen==Screen::home){if(index==stage_count)return {layout.x+layout.w-35,layout.y+layout.h-36,30,30};int tw=std::min(42,(layout.w-38)/4);return {layout.x+layout.w/2-(tw*4+18)/2+(index%4)*(tw+6),layout.y+83+(index/4)*30,tw,27};}
  int count=menu_count(),bh=std::min(29,(layout.h-70)/count),step=bh+3;int mw=std::min(layout.w-54,170);return {layout.x+(layout.w-mw)/2,modal_y()+49+index*step+(screen==Screen::pause?0:44),mw,bh};}
 void key(uint32_t keys){using namespace pxa::input;using_keys=true;if(keys&controller_b){on_back();return;}
  if(screen==Screen::help){if(keys&(controller_left|controller_up))help_page=(help_page+2)%3;if(keys&(controller_right|controller_down|controller_x))help_page=(help_page+1)%3;if(keys&(controller_a|controller_start))activate(0);dirty=true;return;}
  if(screen==Screen::play){if(keys&controller_start){pause();return;}if(keys&controller_left)col=(col+columns-1)%columns;if(keys&controller_right)col=(col+1)%columns;
   if(keys&controller_up)row=(row+rows-1)%rows;if(keys&controller_down)row=(row+1)%rows;if(keys&controller_l)selection=(selection+plant_count)%(plant_count+1);if(keys&(controller_r|controller_x))selection=(selection+1)%(plant_count+1);if(keys&controller_a)plant();
  }else if(screen==Screen::home){if(keys&controller_left)menu=(menu+stage_count)%(stage_count+1);if(keys&controller_right)menu=(menu+1)%(stage_count+1);if(keys&controller_up)menu=(menu+stage_count-3)%(stage_count+1);if(keys&controller_down)menu=(menu+4)%(stage_count+1);if(keys&(controller_a|controller_start))activate(menu);
  }else{if(keys&(controller_up|controller_left))menu=(menu+menu_count()-1)%menu_count();if(keys&(controller_down|controller_right))menu=(menu+1)%menu_count();if(keys&(controller_a|controller_start))activate(menu);}keep_seed_visible();dirty=true;}
 void on_controller(pxa::Context&,const pxa::input::ControllerState& state){if(!ready||!controller.update(state))return;if(!state.connected){controller.reset();wait_release=true;return;}if(wait_release){if(!controller.buttons())wait_release=false;return;}
  uint32_t edges=0;for(unsigned n=0;n<12;++n){uint32_t mask=1u<<n;if(controller.pressed(mask))edges|=mask;}if(edges&15)repeat_at=now+330000;if(!(controller.buttons()&15))repeat_at=0;if(edges)key(edges);}
 void pointer(pxa::input::PointerPhase phase,int x,int y,int native_x,int native_y){using P=pxa::input::PointerPhase;using_keys=false;
  if(phase==P::down){press_x=x;press_y=y;press_native_x=native_x;press_native_y=native_y;if(screen==Screen::play){

    for(int i=0;i<=plant_count;++i)if(layout.card_hit(i).contains(x,y)){selection=i==plant_count?0:i+1;dirty=true;
#ifdef GARDEN_HUD_BENCHMARK
     card_request_us=monotonic_us();
#endif
     return;}}}
  if(phase!=P::up)return;
  if(screen==Screen::play&&layout.scroll_seeds()&&std::abs(x-press_x)<12&&std::abs(y-press_y)<12){for(bool right:{false,true})if(layout.seed_arrow(right).contains(x,y)&&layout.seed_arrow(right).contains(press_x,press_y)){shift_seeds(right?1:-1);return;}}
  if(screen==Screen::play&&x>layout.x+layout.w-30&&press_x>layout.x+layout.w-30&&y<layout.y+54&&press_y<layout.y+54){pause();return;}
  if(screen==Screen::play){auto end=layout.hit(native_x,native_y,scale),start=layout.hit(press_native_x,press_native_y,scale);if(end&&end==start&&std::abs(native_x-press_native_x)<12*scale&&std::abs(native_y-press_native_y)<12*scale){row=end->first;col=end->second;plant();}}
  else if(std::abs(x-press_x)<12&&std::abs(y-press_y)<12){if(screen==Screen::home&&layout.start().contains(x,y)&&layout.start().contains(press_x,press_y)){activate(menu<stage_count?menu:0);return;}if(screen==Screen::help){if(x<layout.x+55&&y>layout.y+layout.h-36){help_page=(help_page+2)%3;dirty=true;}else if(x>layout.x+layout.w-55&&y>layout.y+layout.h-36){help_page=(help_page+1)%3;dirty=true;}else if(y>layout.y+layout.h-36)activate(0);return;}if(screen==Screen::pause&&sound_rect().contains(x,y)&&sound_rect().contains(press_x,press_y)){sounds.toggle();dirty=true;return;}for(int i=0;i<menu_count();++i)if(menu_rect(i).contains(x,y)&&menu_rect(i).contains(press_x,press_y)){activate(i);break;}}}
 void on_pointer(pxa::Context&,const pxa::input::Pointer& p){if(!ready)return;using P=pxa::input::PointerPhase;if(p.action()==P::down){if(touching)return;touching=true;pointer_id=p.pointer_id;}if(!touching||p.pointer_id!=pointer_id)return;
  int px=pxa::ui::canvas_to_surface_coordinate(p.x,display),py=pxa::ui::canvas_to_surface_coordinate(p.y,display);pointer(p.action(),px/scale,py/scale,px,py);if(p.ended())touching=false;}
 void on_background(pxa::Context&){backgrounded=true;touching=false;controller.reset();wait_release=true;repeat_at=0;last=0;sounds.suspend();pause();(void)ctx->clock().set_period(0);}
 void on_foreground(pxa::Context&){last=0;wait_release=backgrounded;dirty=true;sounds.resume();(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);(void)ctx->window().keep_screen_on(screen==Screen::play);if(ready||renderer_pending)(void)ctx->clock().set_period(33);}
 pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event){sounds.event(event);
  if(auto metrics=pxa::ui::decode_display_metrics(event)){bool changed=display.width!=metrics->width||display.height!=metrics->height;display=*metrics;if(changed&&ready&&!resize_pending)defer_renderer();layout_update();dirty=true;return true;}
  auto tick=pxa::decode_clock_tick(event);if(!tick)return false;now=tick->timestamp_us;if(renderer_pending&&ctx->foreground()){renderer_pending=false;auto r=ctx->tasks().start(open_renderer());if(!r)return std::unexpected(r.error());return true;}if(!ready||!ctx->foreground())return true;float dt=last&&now>last?std::min(.1f,float(now-last)*1e-6f):0;last=now;
  if(repeat_at&&now>=repeat_at){repeat_at=now+110000;key(controller.buttons()&15);}if(notice_time>0){notice_time-=dt;dirty=true;}
  auto update_started=monotonic_us();
 if(screen==Screen::play){
#ifdef GARDEN_BENCHMARK
   dense_scene(game,float(monotonic_us()-benchmark_start)*1e-6f,((monotonic_us()-benchmark_start)/30000000)%2==0);
#endif
 accumulator+=dt;int generated=game.generated,kills=game.kills;unsigned steps=0;while(accumulator>=.025f&&steps++<4){game.step(.025f);accumulator-=.025f;}
#ifdef GARDEN_BENCHMARK
   dense_scene(game,float(monotonic_us()-benchmark_start)*1e-6f,((monotonic_us()-benchmark_start)/30000000)%2==0);
#endif
   if(game.generated!=generated)sounds.play(motion::Cue::star);else if(game.kills!=kills)sounds.play(motion::Cue::impact);
   if(game.outcome==Outcome::won){screen=Screen::won;(void)ctx->window().keep_screen_on(false);menu=0;sounds.play(motion::Cue::win);unlocked=std::max(int(unlocked),std::min(stage_count,game.level+2));save();log("Garden won");}
   if(game.outcome==Outcome::lost){screen=Screen::lost;(void)ctx->window().keep_screen_on(false);menu=0;sounds.play(motion::Cue::impact);log("Garden lost");}dirty=true;}

  perf.update+=monotonic_us()-update_started;
  if(screen==Screen::help&&help_pose!=now/50000){help_pose=now/50000;dirty=true;}
  if(dirty)render();return true;}
 #include "game_view.hpp"
 static uint64_t monotonic_us(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000;}
 void render(){if(!ready||!surface||!ctx->foreground())return;auto acquired=surface->acquire();if(!acquired){dirty=true;return;}auto view=acquired->rgb565();if(!view){dirty=true;return;}auto started=monotonic_us();
  bool battle=screen==Screen::play||screen==Screen::pause;
  unsigned moving=0;if(battle){for(const auto& e:game.enemies)moving+=e.active?2:0;for(const auto& shot:game.shots)moving+=shot.active;}
  art.cached_motion=moving>=48;
  uint64_t key=battle?(1ull<<63)|(uint64_t(game.stars)<<24)|(uint64_t(selection)<<20)|(uint64_t(layout.seed_offset)<<16)|(uint64_t(game.level)<<8)|unsigned(std::min(1.f,game.elapsed/game.duration())*55):0;
  uint64_t plants=0;if(battle)for(unsigned i=0;i<game.plants.size();++i)if(game.plants[i].kind!=Defender::none)plants|=1ull<<i;
  uint64_t affordable=0;for(int i=1;i<=plant_count;++i)if(game.stars>=cost[i])affordable|=1ull<<i;
  uint64_t card_key=(uint64_t(selection)<<32)|(uint64_t(layout.seed_offset)<<24)|affordable;
  bool rebuild_hud=backdrop_dirty||!hud_valid||uint64_t(game.level)!=hud_key;
  bool cards_changed=card_key!=cards_key;
  bool rebuild=backdrop_dirty||battle!=backdrop_battle||(battle&&(shadow_key!=plants||rebuild_hud));
  Paint background{backdrop.data(),int(display.width),int(display.height),int(display.width),art,scale,false};
  if(rebuild){bool baked=false;
   int upper=std::lround((layout.y+56)*scale),lower=std::lround((layout.y+layout.h-38)*scale);
   if(hud_underlay.top!=upper||hud_underlay.bottom!=lower)hud_underlay.configure(display.width,display.height,upper,lower);
   if(!backdrop_dirty&&!base_backdrop.empty()){std::copy(base_backdrop.begin(),base_backdrop.end(),backdrop.begin());baked=true;}
   if(!baked)for(auto info:garden_art::backgrounds)if(info.width==display.width&&info.height==display.height&&info.bx==layout.bx&&info.by==layout.by&&info.cell==layout.cell){auto bytes=std::span(reinterpret_cast<uint8_t*>(backdrop.data()),backdrop.size()*2);baked=unpack(std::span(garden_art::packed_backgrounds).subspan(info.offset,info.size),bytes);if(!baked){log("Garden packed background invalid");return;}break;}
   if(!baked){scenery(background);if(!base_backdrop.empty())std::copy(backdrop.begin(),backdrop.end(),base_backdrop.begin());}
   if(battle)for(int r=0;r<rows;++r)for(int c=0;c<columns;++c)if(game.plants[r*columns+c].kind!=Defender::none){auto tile=layout.tile(r,c);background.shadow(tile.x+tile.w/2,tile.y+tile.h-1,layout.cell);}
   if(battle){
    if(rebuild_hud){background.cached=true;draw_hud_base(background,game,layout);hud_underlay.capture(backdrop.data());hud_key=game.level;hud_valid=true;}
    else hud_underlay.restore(backdrop.data());
    int rw=layout.w<260?40:48;auto currency=background.physical({layout.x+6,layout.y+25,rw-12,17}),progress=background.physical(layout.wave());
    currency_patch.capture(backdrop.data(),display.width,currency.x,currency.y,currency.w,currency.h);
    progress_patch.capture(backdrop.data(),display.width,progress.x,progress.y,progress.w,progress.h);
   }
   shadow_key=plants;backdrop_battle=battle;
  }
  if(battle&&(rebuild||cards_changed)){if(!rebuild)hud_underlay.restore(backdrop.data());background.cached=true;draw_hud_cards(background,game,layout,selection);art.release_hud_cache();cards_key=card_key;}
  if(battle&&(rebuild||cards_changed||key!=backdrop_key)){currency_patch.restore(backdrop.data(),display.width);progress_patch.restore(backdrop.data(),display.width);background.cached=true;draw_hud_dynamic(background,game,layout);}
  if(rebuild||(battle&&(cards_changed||key!=backdrop_key)))++background_revision;
  backdrop_dirty=false;backdrop_key=key;
  auto background_done=monotonic_us();
  auto pixels=acquired->pixels();int stride=acquired->stride_bytes()/2;auto dest=reinterpret_cast<uint16_t*>(pixels.data());auto buffer_index=acquired->buffer_index();frame_restore.restore(buffer_index,dest,stride,backdrop.data(),display.width,display.height,background_revision);
  auto copy_done=monotonic_us();
  Paint paint{dest,int(display.width),int(display.height),stride,art,scale};draw(paint);frame_restore.finish(buffer_index,screen==Screen::play?board_damage_bounds:Rect{0,0,int(display.width),int(display.height)});auto elapsed=monotonic_us()-started;paint_us+=elapsed;paint_max=std::max(paint_max,elapsed);++paint_count;auto sent=acquired->present(frame_id++);dirty=!sent;
  perf.frame(started,uint32_t(background_done-started),uint32_t(copy_done-background_done),uint32_t(started+elapsed-copy_done),uint32_t(monotonic_us()-started-elapsed));
#ifdef GARDEN_HUD_BENCHMARK
  if(card_request_us){char text[220];std::snprintf(text,sizeof text,"Garden card selected=%d latency_us=%llu background_us=%llu render_us=%llu world_rebuild=%u misses=%llu",selection,monotonic_us()-card_request_us,background_done-started,elapsed,unsigned(rebuild),art.misses);log(text);card_request_us=0;}
#endif
#ifdef GARDEN_BENCHMARK
  if(screen==Screen::play&&started-perf_report>=5000000){perf_report=started;char text[400];unsigned n=std::max(1u,perf.count);std::snprintf(text,sizeof text,"Garden perf dense=%u frames=%u fps_x100=%llu p95_us=%u max_us=%u update_us=%llu background_us=%llu copy_us=%llu draw_us=%llu present_us=%llu hits=%llu misses=%llu",unsigned(((started-benchmark_start)/30000000)%2==0),n,perf.last>perf.began?(uint64_t(n-1)*100000000/(perf.last-perf.began)):0,perf.percentile(95),perf.maximum,perf.update/n,perf.background/n,perf.copy/n,perf.draw/n,perf.present/n,art.hits,art.misses);log(text);perf={};}
#endif
  if(!sent&&sent.error()!=pxa::Error::would_block){char text[80];std::snprintf(text,sizeof text,"Garden render error=%d",int(sent.error()));log(text);}}

};
}
PXA_APPLICATION(garden::App)
