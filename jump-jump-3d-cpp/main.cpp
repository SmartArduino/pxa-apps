#include <pxa/app.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/game_quality.hpp>
#include <cstdio>
#include "jump3d_game.hpp"
#include "jump3d_render.hpp"
#include "jump3d_palette.hpp"
#include "jump3d_font.hpp"
#include "jump3d_audio.hpp"
#include "frame_memory.hpp"
#include "jump3d_clock.hpp"
#define TICK_MS 20u
#define J3_BGM_MODE 1
#define J3_BGM_LOOP 0
#ifndef J3_TRACE_TIMING
#define J3_TRACE_TIMING 0
#endif
#ifndef J3_FORCE_SCALE_SHIFT
#define J3_FORCE_SCALE_SHIFT (-1)
#endif
using namespace jump;
namespace {
struct JumpJump {
    pxa::Context* context=nullptr;
    pxa::ui::DisplayMetrics display;
    std::optional<pxa::game::Renderer> renderer;
    FrameMemory frame_memory;
    j3_game_t g_game{};
    j3_render_t render;
    j3_audio_t g_audio;
    GameClock stepper;
    pxa::game::AdaptiveResolution quality;
    uint8_t quality_ticks=0;
    uint64_t frame_id=0,window_start=0;
    uint32_t best_saved=0;
    uint8_t g_last_state=0,g_bonus_played=0,scheme=0,scale_shift=0;
    uint16_t g_last_jumps=0;
    uint32_t g_last_score=0;
    float g_bonus_repeat_timer=0,save_timer=0;
    bool g_bgm_started=false,initializing=false,saving=false;
    #include "sound_triggers.inc"
    auto view(){return pxa::ui::Canvas().input_only().on_pointer([this](const pxa::ui::CanvasPointer& p){pointer(p);});}
    pxa::Result<void> on_start(pxa::Context& ctx,std::span<const std::byte> config){
        context=&ctx;arcade::logging_transport=&ctx.transport();
        if(auto metrics=pxa::ui::decode_start_display(config))display=*metrics;
        j3_game_reset(&g_game,0x9e3779b9);g_last_state=g_game.state;
        if constexpr(J3_FORCE_SCALE_SHIFT>=0)scale_shift=J3_FORCE_SCALE_SHIFT;
        auto full=ctx.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(!full)return full;
        j3_audio_start(&g_audio,ctx);
        auto started=ctx.tasks().start(load_best());if(!started)return started;
        return ctx.tasks().start(initialize());
    }
    pxa::Task<void> load_best(){auto best=co_await context->storage().get_value<uint32_t>("best");
        if(best){g_game.best=*best;best_saved=*best;}co_return pxa::Result<void>{};}
    pxa::Task<void> save_best(uint32_t value){auto saved=co_await context->storage().set_value("best",value);
        if(saved)best_saved=value;saving=false;co_return pxa::Result<void>{};}
    pxa::Task<void> initialize(){
        initializing=true;renderer.reset();
        for(;scale_shift<=2;++scale_shift){
            pxa::game::RenderOptions options;
            options.width=uint16_t(std::max(64u,(display.width+(1u<<scale_shift)-1)>>scale_shift));
            options.height=uint16_t(std::max(64u,(display.height+(1u<<scale_shift)-1)>>scale_shift));
            options.buffers=3;options.direct_scanout=true;options.max_draw_bytes=FrameMemory::draw_capacity;
            auto created=co_await context->game().create(options);
            if(!created)continue;
            renderer.emplace(std::move(*created));
            j3_render_configure(&render,options.width,options.height,renderer->capabilities());
            j3_render_adapt(&render,display);
            auto upload=frame_memory.upload();auto palette=frame_memory.palette();
            j3_palette_build(palette.data());scheme=uint8_t(g_game.jump_count/15%J3_BG_SCHEMES);
            if(!j3_render_upload_resources(*renderer,upload.data(),upload.size(),palette.data(),scheme)||
                !pxa::game::Upload(*renderer,std::as_writable_bytes(std::span{upload})).palette(palette,J3_LIGHT_LEVELS)){
                renderer.reset();continue;
            }
            auto fonts=co_await load_fonts();if(!fonts){
                renderer.reset();continue;
            }
            (void)context->clock().set_period(TICK_MS);stepper.reset();initializing=false;draw_frame();
            char msg[128];std::snprintf(msg,sizeof(msg),"J3CPP ready pixels=%u,%u dpi_q16=%u render=%u,%u scale=%u",display.width,display.height,display.density_q16,options.width,options.height,scale_shift);
            (void)context->log().write(pxa::LogLevel::info,msg);co_return pxa::Result<void>{};
        }
        initializing=false;co_return std::unexpected(pxa::Error::limit_exceeded);
    }
    pxa::Task<void> load_fonts(){
        for(uint8_t font=0;font<J3_FONT_FACES;++font){
            const auto& face=j3_font_face(font);
            auto asset=co_await context->assets().load(pxa::AssetKind::texture,j3_font_asset_path(font));
            if(!asset)co_return std::unexpected(asset.error());
            if(asset->descriptor().width!=face.atlas_width||asset->descriptor().height!=face.atlas_height)
                co_return std::unexpected(pxa::Error::protocol_error);
            auto bound=renderer->bind_asset(*asset,{font});if(!bound)co_return bound;
        }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> reload_fonts(){
        initializing=true;
        auto fonts=co_await load_fonts();initializing=false;
        if(!fonts){renderer.reset();co_return fonts;}
        draw_frame();co_return pxa::Result<void>{};
    }
    void draw_frame(){if(!renderer||initializing||!context->foreground())return;
        auto draw=frame_memory.draw();
        if(!j3_render_frame(&render,&g_game,*renderer,draw.data(),draw.size(),++frame_id))
            (void)context->log().write(pxa::LogLevel::warning,"J3CPP frame not accepted");
    }
    void pointer(const pxa::ui::CanvasPointer& p){if(!renderer||initializing)return;
#if J3_TRACE_TIMING
        const auto trace_charge_us=unsigned(g_game.charge*1e6f);
#endif
        if(p.phase==pxa::ui::pointer_phase_down){
            if(g_game.state==J3_STATE_OVER){auto best=g_game.best;j3_game_reset(&g_game,0x9e3779b9^uint32_t(frame_id));g_game.best=best;
                g_last_state=g_game.state;g_last_score=0;g_last_jumps=0;g_bonus_played=0;g_bonus_repeat_timer=0;
                j3_audio_play(&g_audio,J3_CHANNEL_LAND,J3_CLIP_START,J3_GAIN_FULL,0);
                j3_audio_stop(&g_audio,J3_CHANNEL_COMBO);j3_audio_stop(&g_audio,J3_CHANNEL_BONUS);
            }else j3_game_press(&g_game);
        }else if(p.phase==pxa::ui::pointer_phase_up) {
            j3_game_release(&g_game);
        }
        else if(p.phase==pxa::ui::pointer_phase_cancel){if(g_game.state==J3_STATE_CHARGING){g_game.state=J3_STATE_READY;g_game.charge=0;}}
#if J3_TRACE_TIMING
        char trace[200];std::snprintf(trace,sizeof(trace),"J3CPP input phase=%u now=%llu state=%u charge_us=%u vx_q6=%d vy_q6=%d vz_q6=%d land_x_q6=%d land_z_q6=%d",p.phase,(unsigned long long)p.timestamp_us,g_game.state,trace_charge_us,int(g_game.vx*1e6f),int(g_game.vy*1e6f),int(g_game.vz*1e6f),int(g_game.land_x*1e6f),int(g_game.land_z*1e6f));
        (void)context->log().write(pxa::LogLevel::info,trace);
#endif
        play_state_sounds();draw_frame();
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event){
        j3_audio_event(&g_audio,event);
        if(event.service==3&&event.opcode==0x8002){if(auto metrics=pxa::ui::decode_display_metrics(event.payload)){
            bool resize=metrics->width!=display.width||metrics->height!=display.height;display=*metrics;
            if(resize&&!initializing){scale_shift=J3_FORCE_SCALE_SHIFT>=0?J3_FORCE_SCALE_SHIFT:0;quality={};(void)context->tasks().start(initialize());}
            else if(renderer&&!initializing){const auto previous_tier=j3_font_tier();j3_render_adapt(&render,display);
                if(previous_tier!=j3_font_tier()){
                    auto started=context->tasks().start(reload_fonts());
                    if(!started){j3_font_set_tier(previous_tier);render.big_cell_h=j3_font_cell_height(J3_FONT_BIG);
                        (void)context->log().write(pxa::LogLevel::warning,"J3CPP font reload deferred");draw_frame();}
                }else draw_frame();}}
            return true;}
        if(event.service!=4||event.opcode!=0x8001||event.payload.size()!=8)return false;
        auto now=pxa::wire::get64(event.payload.data());j3_audio_tick(&g_audio,now);
#if J3_TRACE_TIMING
        const auto previous=stepper.previous_timestamp();
#endif
        auto steps=stepper.advance(now);
        for(unsigned i=0;i<steps;++i)j3_game_tick(&g_game,.02f);
#if J3_TRACE_TIMING
        if(g_game.state==J3_STATE_CHARGING){char trace[136];std::snprintf(trace,sizeof(trace),"J3CPP tick previous=%llu now=%llu steps=%u charge_us=%u",(unsigned long long)previous,(unsigned long long)now,steps,unsigned(g_game.charge*1e6f));(void)context->log().write(pxa::LogLevel::info,trace);}
#endif
        if(steps){play_state_sounds();save_timer-=steps*.02f;
            if(!saving&&save_timer<=0&&g_game.best>best_saved){saving=true;save_timer=2;if(!context->tasks().start(save_best(g_game.best)))saving=false;}
            auto next=uint8_t(g_game.jump_count/15%J3_BG_SCHEMES);
            if(renderer&&next!=scheme){scheme=next;auto upload=frame_memory.upload();auto palette=frame_memory.palette();j3_palette_build(palette.data());
                (void)j3_render_upload_sky(*renderer,palette.data(),scheme,upload.data(),upload.size());
                (void)pxa::game::Upload(*renderer,std::as_writable_bytes(std::span{upload})).palette(palette,J3_LIGHT_LEVELS);}}
        // Presentation stays independent of simulation rounding: a short
        // callback must not discard an otherwise available display frame.
        draw_frame();
        if constexpr(J3_FORCE_SCALE_SHIFT<0){
            if(renderer&&!initializing&&++quality_ticks>=48){quality_ticks=0;
                if(auto t=renderer->telemetry())if(int change=quality.observe(*t,scale_shift)){
                    const auto previous=scale_shift;scale_shift=uint8_t(int(scale_shift)+change);
                    if(!context->tasks().start(initialize()))scale_shift=previous;
                }}
        }
        if(renderer&&now-window_start>=5000000){window_start=now;if(auto t=renderer->telemetry()){
            char line[232];std::snprintf(line,sizeof(line),"J3CPP perf submitted=%llu rendered=%llu visible=%llu raster_us=%llu queue_us=%llu present_us=%llu dropped=%llu draw=%u score=%u state=%u",(unsigned long long)t->submitted_frames,(unsigned long long)t->rendered_frames,(unsigned long long)t->visible_frames,(unsigned long long)t->host_raster_us,(unsigned long long)t->queue_wait_us,(unsigned long long)t->present_us,(unsigned long long)t->dropped_frames,t->last_draw_list_bytes,g_game.score,g_game.state);
            (void)context->log().write(pxa::LogLevel::info,line);}}
        return true;
    }
    void on_background(pxa::Context&){j3_audio_pause(&g_audio);(void)context->clock().set_period(0);if(g_game.state==J3_STATE_CHARGING){g_game.state=J3_STATE_READY;g_game.charge=0;}stepper.reset();}
    void on_foreground(pxa::Context& ctx){j3_audio_resume(&g_audio);stepper.reset();(void)ctx.clock().set_period(TICK_MS);(void)ctx.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);draw_frame();}
};
}
PXA_APPLICATION(JumpJump)
