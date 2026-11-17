// SPDX-License-Identifier: MIT
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include "../common/cpp/audio.hpp"
#include "core.hpp"
#include "renderer.hpp"
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
 std::optional<pxa::game::Renderer> target;
 pxa::game::DrawBuffer<49152> commands;
 pxa::input::Controller pad;
 std::optional<pxa::Asset> title_art,panel_art,button_art,background_art;
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
     const float scale=std::clamp(std::min(display.width/296.f,display.height/240.f),.8f,1.6f);
     int px=int(76*scale+display.safe.top);
     int h=std::max(80,int((int64_t(px)*65536+display.density_q16-1)/display.density_q16));
     return Overlay(Canvas(hud_ui,0,h).input_only().on_pointer(PointerHandler{this})).fill().height(Dp{h});
    },
    [this]{return Overlay(Canvas(native_ui).input_only().on_pointer(PointerHandler{this})).fill().fill_height();}
   ).fill_height()
  ).fill().fill_height().background(0x00000000u);
 }
 uint64_t last_us=0,perf_us=0,frame_id=0;float accumulator=0,footstep=0;
 bool initializing=false,loaded=false,save_pending=false,dirty=false,gesture=false,moved=false,ready=false,music_requested=false;
 uint8_t pointer=0;int sx=0,sy=0,focus=0;uint32_t sound_coins=0;uint64_t rejected=0;
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){
  ctx=&c;if(auto m=pxa::ui::decode_start_display(config))display=*m;
  auto fullscreen=c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(!fullscreen)return fullscreen;
  audio.start(c,-3*256,{1200,0,256});
  (void)c.tasks().start(load_art());
  return c.tasks().start(start_render());
 }
 pxa::Task<void> start_render(){auto saved=co_await load();if(!saved)co_return std::unexpected(saved.error());co_return co_await initialize();}
 pxa::Task<void> load_art(){
  auto art=co_await ctx->assets().load(pxa::AssetKind::image,"assets/ui/title-logo.pxr");
  if(art){title_art.emplace(std::move(*art));scene.title_logo=&*title_art;ui_has_previous=false;draw();(void)ctx->log().write(pxa::LogLevel::info,"RELIC title artwork loaded");}
  else (void)ctx->log().write(pxa::LogLevel::warning,"RELIC title artwork fallback");
  auto panel=co_await ctx->assets().load(pxa::AssetKind::image,"assets/ui/panel.pxr");
  if(panel){panel_art.emplace(std::move(*panel));scene.panel_skin=&*panel_art;}
  auto button=co_await ctx->assets().load(pxa::AssetKind::image,"assets/ui/button.pxr");
  if(button){button_art.emplace(std::move(*button));scene.button_skin=&*button_art;}
  ui_has_previous=false;draw();
  (void)ctx->log().write(pxa::LogLevel::info,scene.panel_skin&&scene.button_skin?"RELIC nine-slice skins loaded":"RELIC nine-slice fallback");
  if(display.height>display.width){
   auto background=co_await ctx->assets().load(pxa::AssetKind::image,"assets/ui/title-background.pxr");
   if(background){background_art.emplace(std::move(*background));scene.title_background=&*background_art;ui_has_previous=false;draw();(void)ctx->log().write(pxa::LogLevel::info,"RELIC title background loaded");}
   else{char message[100];snprintf(message,sizeof(message),"RELIC title background unavailable %d",int(background.error()));(void)ctx->log().write(pxa::LogLevel::warning,message);}
  }
  co_return pxa::Result<void>{};
 }
 pxa::Task<void> load(){
  std::array<std::byte,20> data{};auto r=co_await ctx->storage().get("progress",data);
  if(r&&*r==20&&pxa::wire::get32(data.data())==0x524c4331){
   progress.best=pxa::wire::get32(data.data()+4);progress.coins=pxa::wire::get32(data.data()+8);progress.runs=pxa::wire::get32(data.data()+12);
   progress.quality=std::clamp(int(data[16]),0,1);progress.sound=std::clamp(int(data[17]),0,1);
  }
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
 void tone(int hz,int ms=85){if(!progress.sound||!audio.session)return;pxa::Tone t;t.frequency_hz=uint16_t(hz);t.duration_ms=uint16_t(ms);t.gain_db_q8=-18*256;t.waveform=pxa::Waveform::sine;(void)audio.session->tone(t);}
 void draw(){if(!ready||initializing||!target||!ctx->foreground())return;scene.safe_x=std::max(float(display.safe.left),float(std::max(display.corners[0],display.corners[1]))*.45f)*scene.width/display.width;scene.safe_y=float(display.safe.top)*scene.height/display.height;
  ++frame_id;pxa::game::Frame f(*target,commands.bytes());
  hud_only.set(state.mode==Mode::Running);
  auto& active_ui=state.mode==Mode::Running?hud_ui:native_ui;
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
  action(state,a);if(old!=state.mode){last_us=0;accumulator=0;focus=0;if(state.mode==Mode::Running&&(old==Mode::Title||old==Mode::Dead)&&a==Action::Confirm){sound_coins=0;progress.runs++;checkpoint();tone(660,160);}}
  else if(state.mode==Mode::Running){tone(a==Action::Jump?520:a==Action::Slide?220:350,55);}
  if(audio.session&&state.mode!=old){if(state.mode==Mode::Paused||state.mode==Mode::Dead)(void)audio.session->control_music(pxa::MusicAction::pause);else if(state.mode==Mode::Running&&old==Mode::Paused)(void)audio.session->control_music(pxa::MusicAction::resume);}
  log_state("input");draw();
 }
 void quality(){progress.quality^=1;scene.quality=progress.quality;checkpoint();tone(700);log_state("quality");if(!initializing)(void)ctx->tasks().start(initialize());}
 void bank_run(){progress.best=std::max(progress.best,state.score);progress.coins+=state.coins-state.banked_coins;state.banked_coins=state.coins;checkpoint();}
 void home(){if(state.mode==Mode::Running||state.mode==Mode::Paused||state.mode==Mode::Dead)bank_run();if(audio.session)(void)audio.session->control_music(pxa::MusicAction::stop);music_requested=false;auto seed=state.seed;state=State{};state.seed=seed;gesture=false;last_us=0;draw();}
 void tap(int x,int y){float u=scene.ui_scale();float cy=scene.height*.5f;
  if(state.mode==Mode::Title){if(y>scene.height-36*u&&y<scene.height-8*u){if(x>scene.width/2)quality();else state.mode=Mode::Help;}else act(Action::Confirm);}
  else if(state.mode==Mode::Help)home();
  else if(state.mode==Mode::Running){if(x>scene.width-scene.safe_x-48*u&&y<scene.safe_y+48*u)act(Action::Pause);else act(Action::Jump);}
  else if(state.mode==Mode::Paused){float top=cy-86*u;if(y>top+125*u)home();else if(y>top+89*u){progress.sound^=1;checkpoint();draw();}else act(Action::Confirm);}
  else if(x<scene.width*.5f){if(!state.rescues)act(Action::Rescue);else home();}else act(Action::Confirm);draw();
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
  if(state.mode==Mode::Paused){if(pad.pressed(controller_select)||pad.pressed(controller_b))home();else if(pad.pressed(controller_x)){progress.sound^=1;checkpoint();draw();}else if(pad.pressed(controller_a)||pad.pressed(controller_start))act(Action::Confirm);return;}
  if(state.mode==Mode::Dead){if(pad.pressed(controller_b)){if(!state.rescues)act(Action::Rescue);else home();}else if(pad.pressed(controller_a)||pad.pressed(controller_start))act(Action::Confirm);return;}
  if(pad.pressed(controller_start))act(Action::Pause);
  if(pad.pressed(controller_left))act(Action::Left);if(pad.pressed(controller_right))act(Action::Right);
  if(pad.pressed(controller_up)||pad.pressed(controller_a))act(state.mode==Mode::Dead?Action::Confirm:Action::Jump);
  if(pad.pressed(controller_down)||pad.pressed(controller_b))act(Action::Slide);
 }
 pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e){
  if(e.is<pxa::ui::DisplayMetrics>()){if(auto m=pxa::ui::decode_display_metrics(e)){bool resize=m->width!=display.width||m->height!=display.height;display=*m;if(resize&&!initializing)(void)ctx->tasks().start(initialize());}return true;}
  return false;
 }
 void on_frame(pxa::Context&,const pxa::game::FrameTick& tick){
  uint64_t now=tick.timestamp_us;
  float dt=last_us&&now>=last_us?std::min(.12f,float(now-last_us)*1e-6f):0;last_us=now;
  if(audio.session&&!progress.sound&&music_requested){(void)audio.session->control_music(pxa::MusicAction::stop);music_requested=false;}
  if(audio.session&&progress.sound&&state.mode==Mode::Running&&!music_requested){auto music=audio.session->music("assets/temple.ogg",true,-18*256);music_requested=bool(music);}
  scene.time+=dt;accumulator+=dt;Mode old=state.mode;auto old_feedback=state.feedback;float feedback_left=state.feedback_t;
  for(int i=0;i<8&&accumulator>=1.f/60;i++){update(state,1.f/60);accumulator-=1.f/60;}
  if(state.feedback_t>feedback_left||state.feedback!=old_feedback){if(state.feedback==Feedback::Rush){tone(880,180);log_state("rush");}else if(state.feedback==Feedback::Quest){tone(1320,150);log_state("quest");}}
  if(state.coins>sound_coins){sound_coins=state.coins;tone(1100+(state.combo%4)*160,45);}
  if(old==Mode::Running&&state.mode==Mode::Dead){state.record=state.score>progress.best;bank_run();state.saved=true;tone(130,280);if(audio.session)(void)audio.session->control_music(pxa::MusicAction::stop);music_requested=false;log_state("dead");}
  // Static menus retain their last frame. Retry until the native Canvas mounts;
  // explicit actions and asynchronous artwork loads request their own redraw.
  // Clock stays active for persistence, input and system lifecycle notifications.
  if(state.mode==Mode::Running||(state.mode==Mode::Dead&&state.death_t<1.4f)||!ui_has_previous||state.mode!=ui_mode)draw();
  if(now-perf_us>=5000000&&target){perf_us=now;if(dirty&&loaded&&!save_pending)(void)ctx->tasks().start(save());if(auto t=target->telemetry()){char b[300];snprintf(b,sizeof(b),"RELIC perf submitted=%llu rendered=%llu visible=%llu raster_us=%llu queue_us=%llu present_us=%llu dropped=%llu rejected=%llu bytes=%u faces=%d peak=%d mode=%d distance=%u clipped=%u",(unsigned long long)t->submitted_frames,(unsigned long long)t->rendered_frames,(unsigned long long)t->visible_frames,(unsigned long long)t->host_raster_us,(unsigned long long)t->queue_wait_us,(unsigned long long)t->present_us,(unsigned long long)t->dropped_frames,(unsigned long long)rejected,t->last_draw_list_bytes,scene.count,scene.peak_faces,int(state.mode),unsigned(state.distance),scene.skipped);(void)ctx->log().write(pxa::LogLevel::info,b);}}
 }
 void on_background(pxa::Context&){if(state.mode==Mode::Running)state.mode=Mode::Paused;gesture=false;pad.reset();last_us=0;accumulator=0;audio.pause();}
 void on_foreground(pxa::Context&){last_us=0;gesture=false;audio.resume();(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);draw();}
};
}
PXA_APPLICATION(TempleRunner)
