// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/game_upload.hpp>
#include <cstdio>
#include <unistd.h>
#include "motion.hpp"
#include "controls.hpp"
namespace motion {
// Shared device plumbing only. Each Derived is a separate, signed application;
// its own state machine, assets and storage schema are compiled into its package.
// UI pointers are logical pixels; GameRender geometry uses native Surface pixels.
 template<class Derived> struct Runtime {
 pxa::Context* ctx=nullptr;pxa::ui::DisplayMetrics display;
 std::optional<pxa::game::Renderer> renderer;std::array<std::optional<pxa::Asset>,font_pages> fonts;
 #ifdef MOTION_SYSTEM_FONT
 SystemText system_text;
 auto view(){return SystemText::View{system_text};}
 #endif
 pxa::game::DrawBuffer<24576> buffer;Controls controls;Random random;Motion motion;
 pxa::SensorSubscription acceleration,rotation;pxa::Permission acc_permission,gyro_permission;
 pxa::input::Controller controller;
 bool acceleration_denied=false,rotation_denied=false,exit_requested=false;
 bool ready=false,dirty=true,connecting=false,want_motion=false,want_gyro=false,backgrounded=false,wait_release=false,pointer_down=false;
 uint8_t pointer_owner=0;uint64_t now=0,last_tick=0,last_report=0,connection_epoch=0,repeat_at=0;
 unsigned gyro_samples=0;float gyro_energy=0;int w=240,h=284,scale=1;
 std::string last_focus;
 std::string sensor_status="正在连接体感";
 Derived& game(){return static_cast<Derived&>(*this);}
 static constexpr uint32_t frame_period(){if constexpr(requires{Derived::frame_period_ms;})return Derived::frame_period_ms;else return 33;}
 static constexpr uint32_t sensor_period(){if constexpr(requires{Derived::sensor_period_ms;})return Derived::sensor_period_ms;else return 50;}
 void log(std::string_view s){(void)ctx->log().write(pxa::LogLevel::info,s);}
 pxa::Result<void> on_start(pxa::Context& c,std::span<const std::byte> config){ctx=&c;if(auto d=pxa::ui::decode_start_display(config))display=*d;
   uint32_t seed=0;if(getentropy(&seed,sizeof seed)||!seed)seed=0x9e3779b9;random.state=seed;
   (void)c.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);return c.tasks().start(initialize());}
 pxa::Result<void> palette(){std::array<uint16_t,256> data{};data.fill(white);return pxa::game::Upload(*renderer,buffer.bytes()).palette(data);}
 pxa::Task<void> initialize(){
   auto loaded=co_await game().load();if(!loaded)log("Motion profile unavailable; using defaults");
   if constexpr(requires(Derived& app){app.sounds;}){auto sound=co_await game().sounds.open(*ctx);if(!sound)log("Motion sound unavailable");}
   scale=std::min(display.width,display.height)>=400&&!(display.width%2)&&!(display.height%2)?2:1;w=int(display.width/scale);h=int(display.height/scale);
   // On older direct-scanout Hosts, showing the first permission modal over
   // an existing Surface can leave presentation/input suspended. Acquire the
   // initial permissions before creating any graphics resource.
   if(game().initial_motion()){auto granted=co_await preauthorize();if(!granted)log("Motion preauthorization unavailable");}
   pxa::game::RenderOptions options;options.width=w;options.height=h;options.buffers=2;options.direct_scanout=true;options.max_draw_bytes=buffer.capacity();
   auto created=co_await ctx->game().create(options);if(!created)co_return std::unexpected(created.error());renderer.emplace(std::move(*created));
#ifdef MOTION_SYSTEM_FONT
   if(!system_text.create(ctx->transport(),display,scale))co_return std::unexpected(pxa::Error::protocol_error);
#else
   if(auto sent=palette();!sent)co_return sent;
   if constexpr(requires(Derived& app){app.upload_fonts();}){if(auto loaded_fonts=game().upload_fonts();!loaded_fonts)co_return loaded_fonts;}
   else for(unsigned i=0;i<font_pages;++i){char path[64];std::snprintf(path,sizeof path,"assets/font-%u.pxr",i);auto a=co_await ctx->assets().load(pxa::AssetKind::texture,path);
     if(!a)co_return std::unexpected(a.error());fonts[i].emplace(std::move(*a));if(auto bound=renderer->bind_asset(*fonts[i],{uint8_t(i)});!bound)co_return bound;}
#endif
   ready=true;game().started();last_tick=0;(void)ctx->clock().set_period(frame_period());log(game().ready_message());render();co_return pxa::Result<void>{};
 }
 pxa::Task<void> preauthorize(){
   std::array<pxa::SensorDescriptor,8> list{};auto count=co_await ctx->sensors().list(list);
   if(!count||!ctx->foreground())co_return pxa::Result<void>{};
   bool acc=false,gyro=false;for(unsigned i=0;i<*count;++i){auto &d=list[i];if(d.dimensions!=3)continue;
     acc|=d.semantic.view()=="motion.acceleration"&&d.unit==pxa::SensorUnit::micro_meters_per_second_squared;
     gyro|=d.semantic.view()=="motion.angular-velocity"&&d.unit==pxa::SensorUnit::micro_radians_per_second;}
   if(!acc)co_return pxa::Result<void>{};
   auto ap=co_await ctx->permissions().acquire("sensor.read","motion.acceleration");
   if(!ap){acceleration_denied=true;co_return pxa::Result<void>{};}
   if(!ctx->foreground())co_return pxa::Result<void>{};
   if(game().initial_gyro()&&gyro){auto gp=co_await ctx->permissions().acquire("sensor.read","motion.angular-velocity");if(!gp)rotation_denied=true;}
   co_return pxa::Result<void>{};
 }
 void close_motion(){ctx->foreground_tasks().cancel();connecting=false;++connection_epoch;acceleration.reset();rotation.reset();acc_permission.reset();gyro_permission.reset();motion.suspend();gyro_energy=0;}
 void request_motion(bool enabled,bool gyro=false){if(want_motion!=enabled||want_gyro!=gyro)close_motion();want_motion=enabled;want_gyro=gyro;
   if(!enabled){if(acceleration||rotation||connecting)close_motion();return;}connect();}
 void connect(){if(!ready||!want_motion||connecting||acceleration||!ctx->foreground())return;if(acceleration_denied){sensor_status="体感未授权，仍可按键玩";dirty=true;return;}connecting=true;sensor_status="请允许使用体感";
   if(!ctx->foreground_tasks().start(connect_sensors(connection_epoch))){connecting=false;sensor_status="体感暂不可用";dirty=true;}}
 struct ApprovalClock {
   Runtime& owner;
   explicit ApprovalClock(Runtime& value):owner(value){(void)owner.ctx->clock().set_period(0);}
   ~ApprovalClock(){owner.last_tick=0;owner.dirty=true;if(owner.ready&&owner.ctx->foreground())(void)owner.ctx->clock().set_period(frame_period());}
 };
 pxa::Task<void> connect_sensors(uint64_t epoch){
   std::array<pxa::SensorDescriptor,8> list{};auto count=co_await ctx->sensors().list(list);
   if(epoch!=connection_epoch||!ctx->foreground())co_return pxa::Result<void>{};
   int ai=-1,gi=-1;if(count)for(unsigned i=0;i<*count;++i){auto &d=list[i];if(d.dimensions!=3)continue;
     if(d.semantic.view()=="motion.acceleration"&&d.unit==pxa::SensorUnit::micro_meters_per_second_squared)ai=int(i);
     if(d.semantic.view()=="motion.angular-velocity"&&d.unit==pxa::SensorUnit::micro_radians_per_second)gi=int(i);}
   if(ai<0){sensor_status="没有体感，可用按键触屏";connecting=false;dirty=true;co_return pxa::Result<void>{};}
   // Complete approvals before starting either stream, so no sample queue grows
   // behind a second native permission prompt on older Hosts.
   // Native permission dialogs may stay open for an arbitrary time. Stop our
   // clock while awaiting approval, then explicitly rearm it before streaming.
   { ApprovalClock approval{*this};
   auto ap=co_await ctx->permissions().acquire("sensor.read","motion.acceleration");
   if(epoch!=connection_epoch||!ctx->foreground())co_return pxa::Result<void>{};
   if(!ap){acceleration_denied=true;sensor_status="体感未授权，仍可按键玩";connecting=false;dirty=true;co_return pxa::Result<void>{};}
   acc_permission=std::move(*ap);
   if(want_gyro&&gi>=0&&!rotation_denied){auto gp=co_await ctx->permissions().acquire("sensor.read","motion.angular-velocity");
     if(epoch!=connection_epoch||!ctx->foreground())co_return pxa::Result<void>{};if(gp)gyro_permission=std::move(*gp);else rotation_denied=true;}
   }
   auto &ad=list[ai];auto as=co_await ctx->sensors().subscribe(ad,std::clamp(sensor_period(),ad.min_period_ms,ad.max_period_ms),acc_permission);
   if(epoch!=connection_epoch||!ctx->foreground())co_return pxa::Result<void>{};if(as)acceleration=std::move(*as);
   if(gyro_permission&&gi>=0){auto &gd=list[gi];auto gs=co_await ctx->sensors().subscribe(gd,std::clamp(sensor_period(),gd.min_period_ms,gd.max_period_ms),gyro_permission);
     if(epoch!=connection_epoch||!ctx->foreground())co_return pxa::Result<void>{};if(gs)rotation=std::move(*gs);}
   connecting=false;sensor_status=acceleration?"体感已连接":"体感暂不可用，可用按键";dirty=true;
   log(rotation?"Motion connected acc=1 gyro=1":acceleration?"Motion connected acc=1 gyro=0":"Motion unavailable");co_return pxa::Result<void>{};
 }
 void render(){if(!ready||!renderer||!ctx->foreground())return;pxa::game::Frame frame(*renderer,buffer.bytes());if constexpr(requires(Derived& d){d.background_color();})frame.clear({game().background_color()});else frame.clear({paper});
   Paint p{frame,renderer->supports(pxa::game::RenderCapability::sprite_texel_alpha),w,h};
#ifdef MOTION_SYSTEM_FONT
   system_text.begin();p.system_text=&system_text;
#endif
   controls.begin();game().draw(p);controls.paint(p);
   if constexpr(requires(Derived& d,Paint& p){d.overlay(p);})game().overlay(p);
   auto sent=frame.submit();dirty=!sent;
#ifdef MOTION_SYSTEM_FONT
   if(sent&&!system_text.present()){dirty=true;log("Motion system text present deferred");}
#endif
   if(sent){std::string_view label;for(unsigned i=0;i<controls.count;++i)if(controls.items[i].id==controls.focus)label=controls.items[i].text.data();
     if(label!=last_focus){last_focus=label;log("Motion focus "+last_focus);}}if(!sent&&sent.error()!=pxa::Error::would_block){char s[100];std::snprintf(s,sizeof s,"Motion frame rejected error=%d bytes=%u",int(sent.error()),unsigned(frame.bytes_used()));log(s);}}
 void on_controller(pxa::Context&,const pxa::input::ControllerState& value){if(!ready||!controller.update(value))return;
   if(!value.connected){controller.reset();wait_release=true;repeat_at=0;return;}
   if(wait_release){if(!controller.buttons())wait_release=false;return;}
   uint32_t edges=0;using namespace pxa::input;for(auto mask:{controller_up,controller_down,controller_a,controller_b,controller_start})if(controller.pressed(mask))edges|=mask;
   if(edges&(controller_up|controller_down))repeat_at=now+350000;
   if(!(controller.buttons()&(controller_up|controller_down)))repeat_at=0;
   if(edges){if(edges&controller_start)edges|=controller_a;game().key(edges);}}
 void on_pointer(pxa::Context&,const pxa::input::Pointer& p){if(!ready)return;
   if(p.action()==pxa::input::PointerPhase::down){if(pointer_down)return;pointer_down=true;pointer_owner=p.pointer_id;}
   if(!pointer_down||pointer_owner!=p.pointer_id)return;
   game().pointer(p.action(),pxa::ui::canvas_to_surface_coordinate(p.x,display)/scale,pxa::ui::canvas_to_surface_coordinate(p.y,display)/scale);
   if(p.ended())pointer_down=false;}
 void request_exit(){if constexpr(requires(Derived& app){app.sounds;})game().sounds.stop();exit_requested=true;if(!ctx->window().request_back())exit_requested=false;}
 pxa::BackAction on_back(){if(exit_requested){log("Motion exiting");return pxa::BackAction::close;}return game().back();}
 pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event){
   if constexpr(requires(Derived& app){app.sounds;})game().sounds.event(event);
   if(event.is<pxa::SensorSample>()){if(!ctx->foreground())return true;
     if(auto v=acceleration.sample(event)){if(v->timestamp_us<=motion.last)return true;bool restored=!motion.fresh(v->timestamp_us),before=motion.calibrating;
       bool shaken=motion.acceleration(v->values,v->timestamp_us);game().motion_changed(shaken,restored||before!=motion.calibrating);}
     else if(auto v=rotation.sample(event)){++gyro_samples;float energy=0;for(auto x:v->values)energy+=std::abs(float(x))*.000001f;gyro_energy=std::min(1.4f,energy);}return true;}
   auto tick=pxa::decode_clock_tick(event);if(!tick)return false;now=tick->timestamp_us;float dt=last_tick&&now>=last_tick?std::min(.05f,float(now-last_tick)*1e-6f):0;last_tick=now;
   if(!ready||!ctx->foreground())return true;
   if(repeat_at&&now>=repeat_at){repeat_at=now+110000;game().key(controller.buttons()&(pxa::input::controller_up|pxa::input::controller_down));}
   game().tick(dt);if(dirty)render();
   if(now-last_report>=5000000&&renderer){last_report=now;if(auto t=renderer->telemetry()){char text[240];std::snprintf(text,sizeof text,"Motion perf app=%s frames=%llu visible=%llu raster_us=%u rejected=%u samples=%u gyro=%u",game().id(),(unsigned long long)t->submitted_frames,(unsigned long long)t->visible_frames,t->last_host_raster_us,t->rejected_lists,motion.samples,gyro_samples);log(text);}}
   return true;
 }
 void on_background(pxa::Context&){backgrounded=true;if constexpr(requires(Derived& app){app.sounds;})game().sounds.suspend();close_motion();controller.reset();wait_release=true;repeat_at=0;pointer_down=false;controls.pressed=-1;last_tick=0;game().background();(void)ctx->clock().set_period(0);dirty=true;}
 void on_foreground(pxa::Context&){last_tick=0;wait_release=backgrounded;dirty=true;if constexpr(requires(Derived& app){app.sounds;})game().sounds.resume();(void)ctx->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(ready){game().foreground();if(want_motion)connect();}(void)ctx->clock().set_period(frame_period());}
};
}
