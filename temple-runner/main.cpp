// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include "../common/cpp/audio.hpp"
#include "core.hpp"
#include "renderer.hpp"
#include "soundscape.hpp"
#include <algorithm>
#include <cstdio>
namespace {
using namespace relic;
struct TempleRunner {
 static constexpr pxa::game::LoopOptions loop_options{16667,33,4};
 pxa::Context* ctx=nullptr;
 pxa::ui::DisplayMetrics display;
 State state;Save progress;
 Renderer scene;
 arcade::AudioBank audio;
 Soundscape sounds;
 uint64_t audio_us=0,music_instance=0,music_retry_us=0;
 uint8_t warm_sound=0;
 std::optional<pxa::game::Renderer> target;
 pxa::game::DrawBuffer<49152> commands;
 pxa::input::Controller pad;
 std::optional<pxa::Asset> title_art,background_art,heading_art;
 std::array<std::optional<pxa::Asset>,9> panel_art;
 std::array<std::optional<pxa::Asset>,3> button_art;
 bool art_loading=false,base_art_ready=false;int art_state=-3,heading_variant=0;
 struct PointerHandler {
  TempleRunner* owner=nullptr;
  void operator()(const pxa::ui::CanvasPointer& p) const {if(owner)owner->pointer_event(p);}
 };
 pxa::ui::CanvasRef native_ui,hud_ui;
 pxa::ui::State<bool> hud_only{false};
 pxa::ui::CanvasCommands<4000> ui_commands,ui_previous;
 uint64_t ui_last_us=0;Mode ui_mode=Mode::Help;bool ui_has_previous=false;
 auto view(){
  using namespace pxa::ui;
  return Stack(
   Canvas().input_only().on_pointer(PointerHandler{this}),
   When(hud_only,
    [this]{
     const float scale=ui_pixel_scale(display.width,display.height);
     float sx=std::max(float(display.safe.left),float(std::max(display.corners[0],display.corners[1]))*.45f);
     int px=int(76*scale+ui_hud_top(display.width,sx,display.safe.top));
     int h=std::max(80,int((int64_t(px)*65536+display.density_q16-1)/display.density_q16));
     return Overlay(Canvas(hud_ui,0,h).input_only().on_pointer(PointerHandler{this})).fill().height(Dp{h});
    },
    [this]{return Overlay(Canvas(native_ui).input_only().on_pointer(PointerHandler{this})).fill().fill_height();}
   ).fill_height()
  ).fill().fill_height().background(0x00000000u);
 }
 uint64_t last_us=0,perf_us=0,frame_id=0;float accumulator=0;
 bool initializing=false,loaded=false,save_pending=false,dirty=false,gesture=false,moved=false,ready=false,music_requested=false;
 uint8_t pointer=0;int sx=0,sy=0,focus=0;uint64_t rejected=0;
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){
  ctx=&c;if(auto m=pxa::ui::decode_start_display(config))display=*m;
  auto fullscreen=c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(!fullscreen)return fullscreen;
  audio.start(c,-3*256,{1200,0,256});
  (void)c.tasks().start(load_art());
  return c.tasks().start(start_render());
 }
 pxa::Task<void> start_render(){auto saved=co_await load();if(!saved)co_return std::unexpected(saved.error());co_return co_await initialize();}
 int desired_art()const{return state.mode==Mode::Title?-2:state.mode==Mode::Running?-1:state.mode==Mode::Paused?0:state.mode==Mode::Help?1:state.record?3:2;}
 pxa::Task<void> sync_art(){
  if(art_loading)co_return pxa::Result<void>{};art_loading=true;
  while(art_state!=desired_art()||(art_state>=0&&heading_variant!=(std::min(display.width,display.height)>600?96:48))){
   const int requested=desired_art();art_state=requested;
   scene.heading_skins.fill(nullptr);heading_art.reset();
   // Drop retained images before loading another menu. The old Canvas pins
   // its title artwork even after the Guest's Asset handle is closed.
   if(native_ui.mounted()){pxa::ui::CanvasCommands<4> empty;(void)native_ui.present(empty);}
   (void)co_await ctx->clock().yield();
   ui_has_previous=false;draw();
   if(requested==-2){
    if(!title_art){auto art=co_await ctx->assets().load(pxa::AssetKind::image,"assets/ui/title-logo.pxr");
     if(art){title_art.emplace(std::move(*art));scene.title_logo=&*title_art;}
     else (void)ctx->log().write(pxa::LogLevel::warning,"RELIC title artwork fallback");}
    if(requested!=desired_art())continue;
    if(display.height>display.width&&!background_art){auto art=co_await ctx->assets().load(pxa::AssetKind::image,"assets/ui/title-background.pxr");
     if(art){background_art.emplace(std::move(*art));scene.title_background=&*background_art;}
     else (void)ctx->log().write(pxa::LogLevel::warning,"RELIC title background fallback");}
   }else{
    // Title and menu typography never occupy the bounded image cache together.
    scene.title_logo=nullptr;scene.title_background=nullptr;title_art.reset();background_art.reset();
    ui_has_previous=false;draw();
    if(requested>=0){
     heading_variant=std::min(display.width,display.height)>600?96:48;
     char path[64];snprintf(path,sizeof(path),"assets/ui/heading-%d-%d.pxr",heading_variant,requested);
     auto art=co_await ctx->assets().load(pxa::AssetKind::image,path);
     if(art){heading_art.emplace(std::move(*art));scene.heading_skins[requested]=&*heading_art;scene.heading_pixels=heading_variant;}
     else {char message[96];snprintf(message,sizeof(message),"RELIC heading font fallback error=%d path=%s",int(art.error()),path);(void)ctx->log().write(pxa::LogLevel::warning,message);}
    }
   }
  }
  art_loading=false;ui_has_previous=false;draw();
  (void)ctx->log().write(pxa::LogLevel::info,"RELIC interface artwork ready");co_return pxa::Result<void>{};
 }
 pxa::Task<void> load_art(){
  for(unsigned i=0;i<9;i++){
   char path[40];snprintf(path,sizeof(path),"assets/ui/panel-%u.pxr",i);
   auto panel=co_await ctx->assets().load(pxa::AssetKind::image,path);
   if(panel){panel_art[i].emplace(std::move(*panel));scene.panel_slices[i]=&*panel_art[i];}
  }
  for(unsigned i=0;i<3;i++){
   char path[40];snprintf(path,sizeof(path),"assets/ui/button-%u.pxr",i);
   auto button=co_await ctx->assets().load(pxa::AssetKind::image,path);
   if(button){button_art[i].emplace(std::move(*button));scene.button_slices[i]=&*button_art[i];}
  }
  base_art_ready=true;ui_has_previous=false;draw();
  (void)ctx->log().write(pxa::LogLevel::info,std::all_of(scene.panel_slices.begin(),scene.panel_slices.end(),[](auto p){return p!=nullptr;})&&std::all_of(scene.button_slices.begin(),scene.button_slices.end(),[](auto p){return p!=nullptr;})?"RELIC nine-slice skins loaded":"RELIC nine-slice fallback");
  co_return co_await sync_art();
 }
 pxa::Task<void> load(){
  std::array<std::byte,20> data{};auto r=co_await ctx->storage().get("progress",data);
  if(r&&*r==20&&pxa::wire::get32(data.data())==0x524c4331){
   progress.best=pxa::wire::get32(data.data()+4);progress.coins=pxa::wire::get32(data.data()+8);progress.runs=pxa::wire::get32(data.data()+12);
   progress.quality=std::clamp(int(data[16]),0,1);progress.sound=std::clamp(int(data[17]),0,1);
  }
  sounds.enable(progress.sound);
  loaded=(!r&&r.error()==pxa::Error::not_found)||(r&&*r==20&&pxa::wire::get32(data.data())==0x524c4331);
  char message[100];snprintf(message,sizeof(message),"RELIC loaded best=%u coins=%u quality=%u sound=%u writable=%d",progress.best,progress.coins,progress.quality,progress.sound,loaded);(void)ctx->log().write(pxa::LogLevel::info,message);scene.quality=progress.quality;draw();co_return pxa::Result<void>{};}
 pxa::Task<void> save(){save_pending=true;dirty=false;std::array<std::byte,20> data{};
  pxa::wire::put32(data.data(),progress.magic);pxa::wire::put32(data.data()+4,progress.best);pxa::wire::put32(data.data()+8,progress.coins);pxa::wire::put32(data.data()+12,progress.runs);data[16]=std::byte(progress.quality);data[17]=std::byte(progress.sound);
  auto r=co_await ctx->storage().set("progress",data);if(!r)dirty=true;save_pending=false;
  (void)ctx->log().write(pxa::LogLevel::info,r?"RELIC saved":"RELIC save failed");co_return pxa::Result<void>{};}
 void checkpoint(){dirty=true;if(loaded&&!save_pending)(void)ctx->tasks().start(save());}
 pxa::Task<void> initialize(){initializing=true;ready=false;target.reset();
  float scale=1.f;while(display.width*scale>360||display.height*scale>480)scale*=.5f;
  // Keep text native-sized while reducing only the 3D workload on large screens.
  if(progress.quality&&scale<1.f)scale=std::min(scale,.25f);
  for(int attempt=0;attempt<3;attempt++){
   pxa::game::RenderOptions o;o.width=uint16_t(std::max(96,int(display.width*scale)));o.height=uint16_t(std::max(96,int(display.height*scale)));
   o.buffers=2;o.direct_scanout=true;o.max_draw_bytes=49152;o.scratch=attempt==0?pxa::game::Scratch::depth16:pxa::game::Scratch::none;
   auto r=co_await ctx->game().create(o);if(!r){if(attempt)scale*=.75f;continue;}
   target.emplace(std::move(*r));scene.width=o.width;scene.height=o.height;
   scene.depth=o.scratch==pxa::game::Scratch::depth16;
   if(!scene.upload(*target)){target.reset();scale*=.75f;continue;}
   ready=true;ui_has_previous=false;break;
  }
  initializing=false;
  if(!ready)co_return std::unexpected(pxa::Error::resource_limit);
  last_us=0;draw();
  char b[120];snprintf(b,sizeof(b),"RELIC ready display=%ux%u render=%dx%d quality=%d depth=%d",display.width,display.height,scene.width,scene.height,scene.quality,scene.depth);(void)ctx->log().write(pxa::LogLevel::info,b);
  co_return pxa::Result<void>{};
 }
 void stop_sounds(){if(audio.session)for(unsigned i=0;i<3;i++)(void)audio.session->control_sound(pxa::SoundTrack(i),pxa::MusicAction::stop);}
 void stop_music(){music_requested=false;music_instance=0;if(audio.session)(void)audio.session->control_music(pxa::MusicAction::stop);}
 void toggle_sound(){
  progress.sound^=1;sounds.enable(progress.sound);
  if(!progress.sound){stop_sounds();stop_music();}
  else sounds.request(Cue::Confirm,audio_us);
  (void)ctx->log().write(pxa::LogLevel::info,progress.sound?"RELIC audio enabled=1":"RELIC audio enabled=0");
  checkpoint();draw();
 }
 void pump_sound(){
  if(!audio.session)return;
  if(warm_sound<8&&audio.prepare(warm_sound,cue_info[warm_sound].path)){
   if(++warm_sound==8)(void)ctx->log().write(pxa::LogLevel::info,"RELIC sound palette ready clips=8 bytes=27200 tracks=3");
  }
  for(unsigned track=0;track<3;track++)if(auto pending=sounds.ready(track,audio_us)){
   const unsigned id=unsigned(pending->cue);
   if(auto asset=audio.prepare(uint8_t(id),cue_info[id].path)){
    if(audio.session->sound_track(*asset,pxa::SoundTrack(track),false,pending->gain)){
     const bool first=!(sounds.played_mask&(1u<<id));sounds.accepted(track,audio_us);
     if(first){char message[72];snprintf(message,sizeof(message),"RELIC sound clip=%u track=%u",id,track);(void)ctx->log().write(pxa::LogLevel::info,message);}
    }
   }
  }
 }
 void draw(){if(!ready||initializing||!target||!ctx->foreground())return;scene.safe_x=std::max(float(display.safe.left),float(std::max(display.corners[0],display.corners[1]))*.45f)*scene.width/display.width;scene.safe_y=ui_hud_top(display.width,scene.safe_x*display.width/scene.width,display.safe.top)*scene.height/display.height;
  hud_only.set(state.mode==Mode::Running);
  auto& active_ui=state.mode==Mode::Running?hud_ui:native_ui;
  const int menu_art=desired_art();
  if(state.mode!=Mode::Running&&(!active_ui.mounted()||(menu_art>=0&&(art_state!=menu_art||art_loading))))return;
  ++frame_id;pxa::game::Frame f(*target,commands.bytes());
  ui_commands.reset();scene.overlay=active_ui.mounted()?&ui_commands:nullptr;scene.display=&display;
  scene.render(f,state,progress);if(!f.submit())rejected++;
  if(scene.overlay&&(!ui_has_previous||state.mode!=ui_mode||last_us-ui_last_us>=125000)){
   if(!ui_has_previous||!ui_commands.same_commands(ui_previous)){
    auto shown=active_ui.present(ui_commands);
    if(shown){ui_previous=ui_commands;ui_has_previous=true;}
    else (void)ctx->log().write(pxa::LogLevel::warning,"RELIC native UI present deferred");
   }
   ui_last_us=last_us;ui_mode=state.mode;
  }
 }
 void log_state(const char* event){char b[260];snprintf(b,sizeof(b),"RELIC %s mode=%d distance=%u lane=%d jump=%u slide=%u coins=%u turns=%d score=%u combo=%d rush=%u avoided=%u quests=%u rescue=%u",event,int(state.mode),unsigned(state.distance),state.lane,unsigned(state.jump*100),unsigned(state.slide*100),state.coins,state.turns,state.score,state.combo,unsigned(state.rush*100),state.avoided,state.quests,state.rescues);(void)ctx->log().write(pxa::LogLevel::info,b);}
 void act(Action a){auto old=state.mode;if(state.mode==Mode::Help){state.mode=Mode::Title;draw();return;}
  SoundSnapshot before(state);action(state,a);sounds.action(a,before,state,audio_us);
  if(old!=state.mode){last_us=0;accumulator=0;focus=0;if(state.mode==Mode::Running&&(old==Mode::Title||old==Mode::Dead)&&a==Action::Confirm){progress.runs++;checkpoint();}}
  if(audio.session&&state.mode!=old){
   if(state.mode==Mode::Paused){stop_sounds();audio.pause();}
   else if(state.mode==Mode::Running&&progress.sound)audio.resume();
  }
  log_state("input");draw();
 }
 void quality(){progress.quality^=1;scene.quality=progress.quality;checkpoint();sounds.request(Cue::Confirm,audio_us);log_state("quality");if(!initializing)(void)ctx->tasks().start(initialize());}
 void bank_run(){progress.best=std::max(progress.best,state.score);progress.coins+=state.coins-state.banked_coins;state.banked_coins=state.coins;checkpoint();}
 void home(){if(state.mode==Mode::Running||state.mode==Mode::Paused||state.mode==Mode::Dead)bank_run();stop_music();stop_sounds();sounds.clear();auto seed=state.seed;state=State{};state.seed=seed;gesture=false;last_us=0;draw();}
 void tap(int x,int y){const UiLayout ui(scene.width,scene.height,scene.ui_scale(),scene.safe_x,scene.safe_y);
  if(state.mode==Mode::Title){if(ui.help.contains(x,y)){state.mode=Mode::Help;draw();}else if(ui.quality.contains(x,y))quality();else if(ui.start.contains(x,y))act(Action::Confirm);}
  else if(state.mode==Mode::Help)home();
  else if(state.mode==Mode::Running){if(ui.pause.contains(x,y))act(Action::Pause);else act(Action::Jump);}
  else if(state.mode==Mode::Paused){if(ui.home.contains(x,y))home();else if(ui.sound.contains(x,y))toggle_sound();else if(ui.resume.contains(x,y))act(Action::Confirm);}
  else if(ui.rescue.contains(x,y)){if(!state.rescues)act(Action::Rescue);else home();}else if(ui.retry.contains(x,y))act(Action::Confirm);draw();
 }
 void pointer_event(const pxa::input::Pointer& p){if(!ready)return;
  int x=pxa::ui::canvas_to_surface_coordinate(p.x,display)*scene.width/display.width,y=pxa::ui::canvas_to_surface_coordinate(p.y,display)*scene.height/display.height;
  using P=pxa::input::PointerPhase;
  if(p.action()==P::down){if(gesture)return;gesture=true;pointer=p.pointer_id;sx=x;sy=y;moved=false;}
  if(!gesture||p.pointer_id!=pointer)return;
  if(p.action()==P::cancel){gesture=false;return;}
  int dx=x-sx,dy=y-sy;int threshold=std::max(12,std::min(scene.width,scene.height)/12);
  if((p.action()==P::move||p.action()==P::up)&&!moved&&state.mode==Mode::Running&&std::max(abs(dx),abs(dy))>=threshold){
   moved=true;act(abs(dx)>abs(dy)?(dx<0?Action::Left:Action::Right):(dy<0?Action::Jump:Action::Slide));
  }
  if(p.action()==P::up){gesture=false;if(!moved)tap(x,y);}
 }
 void on_controller(pxa::Context&,const pxa::input::ControllerState& p){using namespace pxa::input;if(!pad.update(p))return;if(!p.connected){gesture=false;if(state.mode==Mode::Running)act(Action::Pause);return;}
  if(state.mode==Mode::Title){if(pad.pressed(controller_b)){state.mode=Mode::Help;draw();}else if(pad.pressed(controller_x))quality();else if(pad.pressed(controller_a)||pad.pressed(controller_start))act(Action::Confirm);return;}
  if(state.mode==Mode::Help){if(pad.pressed(controller_a)||pad.pressed(controller_b)||pad.pressed(controller_start))home();return;}
  if(state.mode==Mode::Paused){if(pad.pressed(controller_select)||pad.pressed(controller_b))home();else if(pad.pressed(controller_x))toggle_sound();else if(pad.pressed(controller_a)||pad.pressed(controller_start))act(Action::Confirm);return;}
  if(state.mode==Mode::Dead){if(pad.pressed(controller_b)){if(!state.rescues)act(Action::Rescue);else home();}else if(pad.pressed(controller_a)||pad.pressed(controller_start))act(Action::Confirm);return;}
  if(pad.pressed(controller_start))act(Action::Pause);
  if(pad.pressed(controller_left))act(Action::Left);if(pad.pressed(controller_right))act(Action::Right);
  if(pad.pressed(controller_up)||pad.pressed(controller_a))act(state.mode==Mode::Dead?Action::Confirm:Action::Jump);
  if(pad.pressed(controller_down)||pad.pressed(controller_b))act(Action::Slide);
 }
 pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e){
  if(auto playback=pxa::decode_playback(e);playback&&audio.session&&playback->session==audio.session->handle()&&playback->instance==music_instance){
   if(playback->state==pxa::PlaybackState::ready)(void)ctx->log().write(pxa::LogLevel::info,"RELIC music ready");
   else {music_requested=false;music_instance=0;music_retry_us=audio_us+2000000;if(playback->state==pxa::PlaybackState::error)(void)ctx->log().write(pxa::LogLevel::warning,"RELIC music playback error");}
   return true;
  }
  if(e.is<pxa::ui::DisplayMetrics>()){if(auto m=pxa::ui::decode_display_metrics(e)){bool resize=m->width!=display.width||m->height!=display.height;display=*m;ui_has_previous=false;if(resize&&!initializing)(void)ctx->tasks().start(initialize());else draw();}return true;}
  return false;
 }
 void on_frame(pxa::Context&,const pxa::game::FrameTick& tick){
  uint64_t now=tick.timestamp_us;audio_us=now;
  float dt=last_us&&now>=last_us?std::min(.12f,float(now-last_us)*1e-6f):0;last_us=now;
  if(audio.session&&progress.sound&&state.mode==Mode::Running&&!music_requested&&now>=music_retry_us){
   auto music=audio.session->music("assets/temple.ogg",true,-9*256);
   if(music){music_instance=*music;music_requested=true;}else music_retry_us=now+2000000;
  }
  SoundSnapshot sound_before(state);
  scene.time+=dt;accumulator+=dt;Mode old=state.mode;auto old_feedback=state.feedback;float feedback_left=state.feedback_t;
  for(int i=0;i<8&&accumulator>=1.f/60;i++){update(state,1.f/60);accumulator-=1.f/60;}
  sounds.frame(sound_before,state,now);
  if(state.feedback_t>feedback_left||state.feedback!=old_feedback){if(state.feedback==Feedback::Rush)log_state("rush");else if(state.feedback==Feedback::Quest)log_state("quest");}
  if(old==Mode::Running&&state.mode==Mode::Dead){state.record=state.score>progress.best;bank_run();state.saved=true;stop_sounds();stop_music();log_state("dead");}
  pump_sound();
  if(base_art_ready&&!art_loading&&(art_state!=desired_art()||(art_state>=0&&heading_variant!=(std::min(display.width,display.height)>600?96:48))))(void)ctx->tasks().start(sync_art());
  // Static menus retain their last frame. Retry until the native Canvas mounts;
  // explicit actions and asynchronous artwork loads request their own redraw.
  // Clock stays active for persistence, input and system lifecycle notifications.
  if(state.mode==Mode::Running||(state.mode==Mode::Dead&&state.death_t<1.4f)||!ui_has_previous||state.mode!=ui_mode)draw();
  if(now-perf_us>=5000000&&target){perf_us=now;if(dirty&&loaded&&!save_pending)(void)ctx->tasks().start(save());if(auto t=target->telemetry()){char b[300];snprintf(b,sizeof(b),"RELIC perf submitted=%llu rendered=%llu visible=%llu raster_us=%llu queue_us=%llu present_us=%llu dropped=%llu rejected=%llu bytes=%u faces=%d peak=%d mode=%d distance=%u clipped=%u",(unsigned long long)t->submitted_frames,(unsigned long long)t->rendered_frames,(unsigned long long)t->visible_frames,(unsigned long long)t->host_raster_us,(unsigned long long)t->queue_wait_us,(unsigned long long)t->present_us,(unsigned long long)t->dropped_frames,(unsigned long long)rejected,t->last_draw_list_bytes,scene.count,scene.peak_faces,int(state.mode),unsigned(state.distance),scene.skipped);(void)ctx->log().write(pxa::LogLevel::info,b);}}
 }
 void on_background(pxa::Context&){if(state.mode==Mode::Running)state.mode=Mode::Paused;gesture=false;pad.reset();last_us=0;accumulator=0;sounds.clear();stop_sounds();audio.pause();}
 void on_foreground(pxa::Context&){last_us=0;gesture=false;if(state.mode==Mode::Running&&progress.sound)audio.resume();(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);draw();}
};
}
PXA_APPLICATION(TempleRunner)
