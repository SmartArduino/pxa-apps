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
 pxa::game::DrawBuffer<32768> commands;
 pxa::input::Controller pad;
 uint64_t last_us=0,perf_us=0,frame_id=0;float accumulator=0,footstep=0;
 bool initializing=false,loaded=false,save_pending=false,dirty=false,gesture=false,moved=false,ready=false,music_requested=false;
 uint8_t pointer=0;int sx=0,sy=0,focus=0;uint32_t sound_coins=0;uint64_t rejected=0;
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){
  ctx=&c;if(auto m=pxa::ui::decode_start_display(config))display=*m;
  auto fullscreen=c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(!fullscreen)return fullscreen;
  audio.start(c,-3*256,{1200,0,256});
  auto save=c.tasks().start(load());if(!save)return save;
  return c.tasks().start(initialize());
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
  for(int attempt=0;attempt<3;attempt++){
   pxa::game::RenderOptions o;o.width=uint16_t(std::max(96,int(display.width*scale)));o.height=uint16_t(std::max(96,int(display.height*scale)));
   o.buffers=2;o.direct_scanout=true;o.max_draw_bytes=32768;o.scratch=pxa::game::Scratch::none;
   auto r=co_await ctx->game().create(o);if(!r){scale*=.75f;continue;}
   target.emplace(std::move(*r));scene.width=o.width;scene.height=o.height;
   if(!scene.upload(*target)){target.reset();scale*=.75f;continue;}
   ready=true;break;
  }
  initializing=false;
  if(!ready)co_return std::unexpected(pxa::Error::resource_limit);
  last_us=0;draw();
  char b[120];snprintf(b,sizeof(b),"RELIC ready display=%ux%u render=%dx%d quality=%d",display.width,display.height,scene.width,scene.height,scene.quality);(void)ctx->log().write(pxa::LogLevel::info,b);
  co_return pxa::Result<void>{};
 }
 void tone(int hz,int ms=85){if(!progress.sound||!audio.session)return;pxa::Tone t;t.frequency_hz=uint16_t(hz);t.duration_ms=uint16_t(ms);t.gain_db_q8=-18*256;t.waveform=pxa::Waveform::sine;(void)audio.session->tone(t);}
 void draw(){if(!ready||initializing||!target||!ctx->foreground())return;scene.safe_x=std::max(float(display.safe.left),float(std::max(display.corners[0],display.corners[1]))*.45f)*scene.width/display.width;scene.safe_y=float(display.safe.top)*scene.height/display.height;
  ++frame_id;pxa::game::Frame f(*target,commands.bytes());scene.render(f,state,progress);if(!f.submit())rejected++;}
 void log_state(const char* event){char b[180];snprintf(b,sizeof(b),"RELIC %s mode=%d distance=%u lane=%d jump=%u slide=%u coins=%u turns=%d score=%u",event,int(state.mode),unsigned(state.distance),state.lane,unsigned(state.jump*100),unsigned(state.slide*100),state.coins,state.turns,state.score);(void)ctx->log().write(pxa::LogLevel::info,b);}
 void act(Action a){auto old=state.mode;if(state.mode==Mode::Help){state.mode=Mode::Title;draw();return;}
  action(state,a);if(old!=state.mode){last_us=0;accumulator=0;focus=0;if(state.mode==Mode::Running&&old!=Mode::Paused){sound_coins=0;progress.runs++;checkpoint();tone(660,160);}}
  else if(state.mode==Mode::Running){tone(a==Action::Jump?520:a==Action::Slide?220:350,55);}
  if(audio.session&&state.mode!=old){if(state.mode==Mode::Paused||state.mode==Mode::Dead)(void)audio.session->control_music(pxa::MusicAction::pause);else if(state.mode==Mode::Running&&old==Mode::Paused)(void)audio.session->control_music(pxa::MusicAction::resume);}
  log_state("input");draw();
 }
 void quality(){progress.quality^=1;scene.quality=progress.quality;checkpoint();tone(700);log_state("quality");draw();}
 void home(){if(audio.session)(void)audio.session->control_music(pxa::MusicAction::stop);music_requested=false;auto seed=state.seed;state=State{};state.seed=seed;gesture=false;last_us=0;draw();}
 void tap(int x,int y){float u=std::clamp(std::min(scene.width/296.f,scene.height/240.f),.8f,1.5f);float cy=scene.height*.5f;
  if(state.mode==Mode::Title){if(y>scene.height-38*u&&y<scene.height-14*u){if(x>scene.width/2)quality();else state.mode=Mode::Help;}else act(Action::Confirm);}
  else if(state.mode==Mode::Help)home();
  else if(state.mode==Mode::Running){if(x>scene.width-scene.safe_x-48*u&&y<scene.safe_y+48*u)act(Action::Pause);else act(Action::Jump);}
  else if(state.mode==Mode::Paused){float top=cy-86*u;if(y>top+125*u)home();else if(y>top+89*u){progress.sound^=1;checkpoint();draw();}else act(Action::Confirm);}
  else act(Action::Confirm);draw();
 }
 void on_pointer(pxa::Context&,const pxa::input::Pointer& p){if(!ready)return;
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
  scene.time+=dt;accumulator+=dt;Mode old=state.mode;
  for(int i=0;i<8&&accumulator>=1.f/60;i++){update(state,1.f/60);accumulator-=1.f/60;}
  if(state.coins>sound_coins){sound_coins=state.coins;tone(1100+(state.combo%4)*160,45);}
  if(old==Mode::Running&&state.mode==Mode::Dead){progress.best=std::max(progress.best,state.score);progress.coins+=state.coins;state.saved=true;checkpoint();tone(130,280);if(audio.session)(void)audio.session->control_music(pxa::MusicAction::stop);music_requested=false;log_state("dead");}
  // Idle result screens stop redrawing after the fall animation. Clock remains
  // active for asynchronous persistence and system lifecycle notifications.
  if(state.mode!=Mode::Dead||state.death_t<1.4f)draw();
  if(now-perf_us>=5000000&&target){perf_us=now;if(dirty&&loaded&&!save_pending)(void)ctx->tasks().start(save());if(auto t=target->telemetry()){char b[300];snprintf(b,sizeof(b),"RELIC perf submitted=%llu rendered=%llu visible=%llu raster_us=%llu queue_us=%llu present_us=%llu dropped=%llu rejected=%llu bytes=%u faces=%d peak=%d mode=%d distance=%u",(unsigned long long)t->submitted_frames,(unsigned long long)t->rendered_frames,(unsigned long long)t->visible_frames,(unsigned long long)t->host_raster_us,(unsigned long long)t->queue_wait_us,(unsigned long long)t->present_us,(unsigned long long)t->dropped_frames,(unsigned long long)rejected,t->last_draw_list_bytes,scene.count,scene.peak_faces,int(state.mode),unsigned(state.distance));(void)ctx->log().write(pxa::LogLevel::info,b);}}
 }
 void on_background(pxa::Context&){if(state.mode==Mode::Running)state.mode=Mode::Paused;gesture=false;pad.reset();last_us=0;accumulator=0;audio.pause();}
 void on_foreground(pxa::Context&){last_us=0;gesture=false;audio.resume();(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);draw();}
};
}
PXA_APPLICATION(TempleRunner)
