#include <pxa/app.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/ui_controller.hpp>
#include <cstdio>
#include "../common/cpp/input.hpp"
#include "assets.hpp"
#include "audio.hpp"
#include "dungeon.hpp"
#include "font.hpp"
#include "font_data.hpp"
#include "game.hpp"
#include "input.hpp"
#include "layout.hpp"
#include "render.hpp"
#include "strings.hpp"
#include "rng.hpp"
#define PD_ACTIVE_PERIOD_MS 40u
#define PD_IDLE_PERIOD_MS 240u
#define PD_AUDIO_PERIOD_MS 20u
using namespace dungeon;
namespace {
constexpr const char* kSaveKeys[PD_SAVE_SLOTS]={"pixel-dungeon.save","pixel-dungeon.save.2","pixel-dungeon.save.3","pixel-dungeon.save.4","pixel-dungeon.save.5"};
constexpr const char* PD_RANK_KEY="pixel-dungeon.ranks";
constexpr const char* PD_ZOOM_KEY="pixel-dungeon.zoom";
struct PixelDungeon {
    pxa::Context* context=nullptr;
    pxa::ui::DisplayMetrics display;
    std::optional<pxa::game::Renderer> renderer;
    uint8_t g_storage_packet[1152]{},g_save_blob[1024]{},g_slot_blob[PD_SAVE_SLOTS][1024]{};
    uint8_t g_rank_blob[2+PD_RANK_COUNT*10]{};
    uint16_t g_slot_length[PD_SAVE_SLOTS]{};
    uint8_t g_upload[1024+20]{},g_draw[PD_MAX_DRAW_BYTES]{},g_fog_texels[256]{},g_search_texels[1024]{};
    uint32_t g_display_width=296,g_display_height=240,g_render_width=296,g_render_height=240;
    uint32_t g_capabilities=0,g_controller_buttons=0,g_anim_ms=0,g_render_phase=0;
    uint64_t g_frame_id=0,perf_start=0;
    uint16_t g_clock_period=0;
    int g_safe_top=8,g_safe_right=10,g_safe_bottom=8,g_safe_left=10;
    uint32_t g_display_shape=0,g_corner_radii[4]{};
    uint8_t g_have_controller=0,g_progress_dirty=0,g_seeded=0,g_result_recorded=0;
    pd_game_t g_game{};pd_layout_t g_layout{};pd_audio_t g_audio;
    bool initializing=false,catalog_ready=false,storage_busy=false,rank_dirty=false,zoom_dirty=false;
    uint8_t save_dirty=0,remove_dirty=0;
    auto view(){return arcade::GameInput{[this](const pxa::ui::CanvasPointer& p){if(!initializing&&catalog_ready)handle_pointer(p);}};}
    static int map_coordinate(int value,uint16_t source,uint16_t target){return source?int(int64_t(value)*target/source):0;}
    void apply_display(pxa::ui::DisplayMetrics metrics){display=metrics;g_display_width=metrics.width;g_display_height=metrics.height;
        g_safe_top=metrics.safe.top;g_safe_right=metrics.safe.right;g_safe_bottom=metrics.safe.bottom;g_safe_left=metrics.safe.left;
        g_display_shape=metrics.shape;std::copy(metrics.corners.begin(),metrics.corners.end(),g_corner_radii);}
    pxa::Result<void> on_start(pxa::Context& ctx,std::span<const std::byte> config){
        context=&ctx;arcade::logging_transport=&ctx.transport();
        if(auto metrics=pxa::ui::decode_start_display(config))apply_display(*metrics);
        // START record 12 contains nested system configuration TLVs.
        for(size_t at=0;at+4<=config.size();){
            auto tag=pxa::wire::get16(config.data()+at),n=pxa::wire::get16(config.data()+at+2);at+=4;
            if(n>config.size()-at)break;
            if(tag==12)for(size_t sub=at;sub+4<=at+n;){
                auto key=pxa::wire::get16(config.data()+sub),length=pxa::wire::get16(config.data()+sub+2);sub+=4;
                if(length>at+n-sub)break;
                if(key==1&&length>=2)pd_strings_set_language(config[sub]==std::byte{'z'}&&config[sub+1]==std::byte{'h'});
                sub+=length;
            }
            at+=n;
        }
        pd_game_reset(&g_game,0x51ed270b);rebuild_layout();pd_audio_set_music(&g_audio,PD_MUSIC_TITLE);
        auto full=ctx.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);if(!full)return full;
        return ctx.tasks().start(initialize());
    }
    pxa::Task<void> load_catalog(){
        for(int slot=0;slot<PD_SAVE_SLOTS;++slot){auto got=co_await context->storage().get(kSaveKeys[slot],std::as_writable_bytes(std::span{g_slot_blob[slot]}));
            if(got&&*got<=1024&&pd_game_save_summary(g_slot_blob[slot],int(*got),&g_game.slots[slot]))g_slot_length[slot]=uint16_t(*got);}
        auto ranks=co_await context->storage().get(PD_RANK_KEY,std::as_writable_bytes(std::span{g_rank_blob}));
        if(ranks&&*ranks==sizeof(g_rank_blob)&&g_rank_blob[0]=='R'&&g_rank_blob[1]==1)
            for(int i=0;i<PD_RANK_COUNT;++i){auto* r=g_rank_blob+2+i*10;auto& rank=g_game.rankings[i];
                if(r[0]!=1||r[1]>=PD_CLASS_COUNT||r[2]<1||r[2]>25)continue;
                rank={1,r[1],r[2],r[3],r[4],r[5],uint16_t(r[6]|r[7]<<8),uint16_t(r[8]|r[9]<<8)};}
        auto zoom=co_await context->storage().get_value<uint8_t>(PD_ZOOM_KEY);
        if(zoom&&*zoom>=1&&*zoom<=3)pd_layout_set_zoom(&g_layout,*zoom);
        catalog_ready=true;render_frame();co_return pxa::Result<void>{};
    }
    void start_storage(){if(storage_busy)return;storage_busy=true;
        auto started=context->tasks().start(flush_storage());if(!started){storage_busy=false;(void)context->log().write(pxa::LogLevel::error,"PDCPP storage task unavailable");}}
    pxa::Task<void> flush_storage(){
        while(save_dirty||remove_dirty||rank_dirty||zoom_dirty){
            for(int slot=0;slot<PD_SAVE_SLOTS;++slot){auto bit=uint8_t(1u<<slot);
                if(remove_dirty&bit){remove_dirty&=~bit;save_dirty&=~bit;auto done=co_await context->storage().remove(kSaveKeys[slot]);
                    if(!done)(void)context->log().write(pxa::LogLevel::error,"PDCPP save removal failed");}
                else if(save_dirty&bit){save_dirty&=~bit;auto done=co_await context->storage().set(kSaveKeys[slot],std::as_bytes(std::span{g_slot_blob[slot]}).first(g_slot_length[slot]),std::as_writable_bytes(std::span{g_storage_packet}));
                    (void)context->log().write(done?pxa::LogLevel::info:pxa::LogLevel::error,done?"PDCPP save committed":"PDCPP save failed");}
            }
            if(rank_dirty){rank_dirty=false;auto done=co_await context->storage().set(PD_RANK_KEY,std::as_bytes(std::span{g_rank_blob}),std::as_writable_bytes(std::span{g_storage_packet}));if(!done)(void)context->log().write(pxa::LogLevel::error,"PDCPP rankings failed");}
            if(zoom_dirty){zoom_dirty=false;auto done=co_await context->storage().set_value(PD_ZOOM_KEY,uint8_t(g_layout.zoom));if(!done)(void)context->log().write(pxa::LogLevel::error,"PDCPP zoom failed");}
        }
        storage_busy=false;co_return pxa::Result<void>{};
    }
    void save_progress(){int slot=g_game.active_slot;if(slot>=PD_SAVE_SLOTS)return;
        if(g_game.phase!=PD_PHASE_PLAY&&g_game.phase!=PD_PHASE_BAG&&g_game.phase!=PD_PHASE_INFO&&g_game.phase!=PD_PHASE_SETTINGS&&g_game.phase!=PD_PHASE_PAUSE&&g_game.phase!=PD_PHASE_SHOP)return;
        int n=pd_game_serialize(&g_game,g_save_blob,sizeof(g_save_blob));if(n<=0)return;
        std::copy_n(g_save_blob,n,g_slot_blob[slot]);g_slot_length[slot]=n;(void)pd_game_save_summary(g_save_blob,n,&g_game.slots[slot]);
        remove_dirty&=~(1u<<slot);save_dirty|=1u<<slot;start_storage();}
    void clear_progress(){auto slot=g_game.active_slot;if(slot>=PD_SAVE_SLOTS)return;g_slot_length[slot]=0;g_game.slots[slot].occupied=0;remove_dirty|=1u<<slot;start_storage();}
    void save_zoom(){zoom_dirty=true;start_storage();}
    void render_frame(){if(!renderer||initializing||!context->foreground())return;
        (void)pd_render_present(*renderer,g_capabilities,&g_game,&g_layout,g_draw,sizeof(g_draw),++g_frame_id,g_anim_ms);}
    void log_phase(uint8_t previous){if(previous==g_game.phase)return;
        char message[32];std::snprintf(message,sizeof(message),"PDCPP phase=%u",g_game.phase);
        (void)context->log().write(pxa::LogLevel::info,message);}
    pxa::Task<void> initialize(){
        initializing=true;renderer.reset();
        // Pixel art uses the physical DPI to choose its integer presentation
        // scale; controls and fonts retain their intended touch/readable size.
        const auto desired=std::clamp((display.density_q16+32768u)/65536u,1u,4u);
        const auto feasible=std::clamp(std::min(display.width,display.height)/160u,1u,4u);
        uint8_t scale=uint8_t(std::min(desired,feasible));
        if(std::min(display.width,display.height)>=400&&std::max(display.width,display.height)>=600)scale=std::max<uint8_t>(scale,2);
        pxa::game::RenderOptions options;options.scale=scale;options.buffers=3;options.direct_scanout=true;options.max_draw_bytes=sizeof(g_draw);
        auto created=co_await context->game().create(options);
        if(created)renderer.emplace(std::move(*created));
        else{options.scale=0;options.width=uint16_t(display.width/scale);options.height=uint16_t(display.height/scale);
            auto fallback=co_await context->game().create(options);
            if(!fallback){initializing=false;co_return std::unexpected(fallback.error());}
            renderer.emplace(std::move(*fallback));}
        g_capabilities=renderer->capabilities();g_render_width=renderer->info().render_width;g_render_height=renderer->info().render_height;
        rebuild_layout();auto resources=co_await load_resources();if(!resources||!upload_resources()){
            (void)context->log().write(pxa::LogLevel::error,"PDCPP resource initialization failed");
            renderer.reset();initializing=false;co_return std::unexpected(pxa::Error::internal);}
        if(!catalog_ready){auto catalog=co_await load_catalog();if(!catalog)co_return catalog;
            pd_audio_init(&g_audio,*context);}
        initializing=false;render_frame();update_clock_period();
        char msg[180];std::snprintf(msg,sizeof(msg),"PDCPP ready pixels=%u,%u dpi_q16=%u render=%u,%u first=%d,%d",display.width,display.height,display.density_q16,g_render_width,g_render_height,g_layout.menu_primary.x+g_layout.menu_primary.w/2,g_layout.menu_primary.y+g_layout.menu_primary.h/2);
        (void)context->log().write(pxa::LogLevel::info,msg);co_return pxa::Result<void>{};
    }
void rebuild_layout(void) {
    const int display_w = g_display_width ? (int)g_display_width : 1;
    const int display_h = g_display_height ? (int)g_display_height : 1;
    const int zoom = g_layout.zoom ? g_layout.zoom :
                     (g_display_shape == 2 && g_display_width >= 360 &&
                      g_display_height >= 360 ? 2 : 1);
    int radii[4];
    pd_layout_build(&g_layout, (int)g_render_width, (int)g_render_height,
                    g_safe_top * (int)g_render_height / display_h,
                    g_safe_right * (int)g_render_width / display_w,
                    g_safe_bottom * (int)g_render_height / display_h,
                    g_safe_left * (int)g_render_width / display_w);
    for (int index = 0; index < 4; ++index)
        radii[index] = (int)((uint64_t)g_corner_radii[index] *
                             g_render_width / (uint32_t)display_w);
    pd_layout_fit_display_shape(&g_layout, g_display_shape, radii);
    pd_layout_set_zoom(&g_layout, zoom);
}

int game_animating(void) {
    if (g_game.walk_active) return 1;
    if (g_game.hero_moving > 0) return 1;
    if (g_game.potion_hint > 0) return 1;
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        if (g_game.mobs[index].dying > 0 ||
            g_game.mobs[index].moving > 0 ||
            g_game.mobs[index].attacking > 0) return 1;
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        if (g_game.effects[index].ttl > 0) return 1;
    return 0;
}

void update_clock_period(void) {
    uint16_t wanted;
    if (pd_audio_active(&g_audio)) {
        /* Music decoding is owned by Host; 20 ms ticks advance fades and
         * gameplay animation while rendering remains on demand. */
        wanted = (uint16_t)PD_AUDIO_PERIOD_MS;
    } else {
        wanted = game_animating() ? (uint16_t)PD_ACTIVE_PERIOD_MS
                                  : (uint16_t)PD_IDLE_PERIOD_MS;
    }
    if (wanted == g_clock_period) return;
    if (context->clock().set_period(wanted))
        g_clock_period = wanted;
}

void resume_slot(void) {
    const int slot = g_game.selected_slot;
    if (slot >= PD_SAVE_SLOTS || !g_slot_length[slot] ||
        !pd_game_restore(&g_game, g_slot_blob[slot], g_slot_length[slot])) {
        g_game.phase = PD_PHASE_SAVES;
        return;
    }
    g_game.active_slot = (uint8_t)slot;
    g_game.phase = PD_PHASE_PLAY;
    g_result_recorded = 0;
    g_layout.camera_dx = 0;
    g_layout.camera_dy = 0;
    g_layout.camera_manual = 0;
}

void record_result(void) {
    pd_save_slot_t entry;
    int position = 0;
    if (g_result_recorded) return;
    g_result_recorded = 1;
    entry = (pd_save_slot_t){1, g_game.hero.cls, g_game.depth,
                             g_game.hero.level, g_game.deepest, g_game.kills,
                             g_game.hero.gold, g_game.turn};
    while (position < PD_RANK_COUNT &&
           g_game.rankings[position].occupied &&
           (g_game.rankings[position].deepest > entry.deepest ||
            (g_game.rankings[position].deepest == entry.deepest &&
             g_game.rankings[position].kills >= entry.kills)))
        ++position;
    if (position >= PD_RANK_COUNT) return;
    for (int index = PD_RANK_COUNT - 1; index > position; --index)
        g_game.rankings[index] = g_game.rankings[index - 1];
    g_game.rankings[position] = entry;
    g_rank_blob[0] = 'R';
    g_rank_blob[1] = 1;
    for (int index = 0; index < PD_RANK_COUNT; ++index) {
        const pd_save_slot_t *rank = &g_game.rankings[index];
        uint8_t *record = g_rank_blob + 2 + index * 10;
        record[0] = rank->occupied;
        record[1] = rank->cls;
        record[2] = rank->depth;
        record[3] = rank->level;
        record[4] = rank->deepest;
        record[5] = rank->kills;
        record[6] = (uint8_t)rank->gold;
        record[7] = (uint8_t)(rank->gold >> 8);
        record[8] = (uint8_t)rank->turns;
        record[9] = (uint8_t)(rank->turns >> 8);
    }
    rank_dirty=true;start_storage();
}

void handle_pointer(const pxa::ui::CanvasPointer& value) {
    auto pointer=value;
    int x;
    int y;
    pointer.x = pxa::ui::canvas_to_surface_coordinate(pointer.x, display);
    pointer.y = pxa::ui::canvas_to_surface_coordinate(pointer.y, display);
    x = map_coordinate(pointer.x, (uint16_t)g_display_width,
                                  (uint16_t)g_render_width);
    y = map_coordinate(pointer.y, (uint16_t)g_display_height,
                                  (uint16_t)g_render_height);
    {
        const uint8_t phase_before = g_game.phase;
        const int zoom_before = g_layout.zoom;
        pd_input_pointer(&g_game, &g_layout, x, y, pointer.pointer_id,
                         pointer.phase,
                         pointer.timestamp_us);
        if (phase_before == PD_PHASE_SAVES && g_game.phase == PD_PHASE_PLAY)
            resume_slot();
        if (zoom_before != g_layout.zoom) save_zoom();
        log_phase(phase_before);
        if (phase_before == PD_PHASE_CLASS && g_game.phase == PD_PHASE_PLAY) {
            g_game.active_slot = g_game.selected_slot;
            g_result_recorded = 0;
            g_layout.camera_dx = g_layout.camera_dy = 0;
            g_layout.camera_manual = 0;
            save_progress();
        }
        if (phase_before == PD_PHASE_PAUSE && g_game.phase == PD_PHASE_TITLE) {
            g_game.phase = PD_PHASE_PAUSE;
            save_progress();
            g_game.phase = PD_PHASE_TITLE;
        }
        if (g_game.phase != phase_before &&
            (g_game.phase == PD_PHASE_DEAD || g_game.phase == PD_PHASE_WON)) {
            record_result();
            clear_progress();
        } else if (g_game.phase != phase_before &&
                   (g_game.phase == PD_PHASE_AMULET ||
                    phase_before == PD_PHASE_AMULET)) {
            save_progress();
        }
    }
    g_progress_dirty = 1;
    pd_audio_set_music(&g_audio,
        (g_game.phase == PD_PHASE_TITLE || g_game.phase == PD_PHASE_SAVES ||
         g_game.phase == PD_PHASE_CLASS) ? PD_MUSIC_TITLE :
        (uint8_t)((g_game.depth - 1) / 5));
    render_frame();
    update_clock_period();
}

void handle_controller(const pxa::ui::ControllerState& controller) {
    uint8_t phase_before;
    if (!controller.connected) {
        g_have_controller = 0;
        return;
    }
    if (!g_have_controller) {
        g_have_controller = 1;
        g_controller_buttons = controller.buttons;
        return;
    }
    phase_before = g_game.phase;
    pd_input_controller(&g_game, controller.buttons, g_controller_buttons);
    if (phase_before == PD_PHASE_SAVES && g_game.phase == PD_PHASE_PLAY)
        resume_slot();
    if (phase_before == PD_PHASE_CLASS && g_game.phase == PD_PHASE_PLAY) {
        g_game.active_slot = g_game.selected_slot;
        g_result_recorded = 0;
        g_layout.camera_dx = g_layout.camera_dy = 0;
        g_layout.camera_manual = 0;
        save_progress();
    }
    if (g_game.phase != phase_before &&
        (g_game.phase == PD_PHASE_DEAD || g_game.phase == PD_PHASE_WON)) {
        record_result();
        clear_progress();
    } else if (g_game.phase != phase_before &&
               (g_game.phase == PD_PHASE_AMULET ||
                phase_before == PD_PHASE_AMULET)) {
        save_progress();
    }
    g_controller_buttons = controller.buttons;
    g_progress_dirty = 1;
    pd_audio_set_music(&g_audio,
        (g_game.phase == PD_PHASE_TITLE || g_game.phase == PD_PHASE_SAVES ||
         g_game.phase == PD_PHASE_CLASS) ? PD_MUSIC_TITLE :
        (uint8_t)((g_game.depth - 1) / 5));
    render_frame();
    update_clock_period();
}

void pump_audio(void) {
    uint8_t sounds[8];
    const int count = pd_game_take_sounds(&g_game, sounds, 8);
    for (int index = 0; index < count; ++index)
        pd_audio_play(&g_audio, sounds[index]);
    pd_audio_tick(&g_audio);
}

void handle_tick(uint64_t timestamp_us) {
    g_anim_ms = (uint32_t)(timestamp_us / 1000u);
    pump_audio();
    if (!g_seeded) {
        g_game.run_seed = pd_rng_mix((uint32_t)(timestamp_us >> 8),
                                     (uint32_t)timestamp_us);
        g_seeded = 1;
    }
    if (g_game.phase != PD_PHASE_PLAY) {
        if (g_progress_dirty) {
            render_frame();
            g_progress_dirty = 0;
        }
        return;
    }
    {
        const uint16_t turn_before = g_game.turn;
        pd_game_tick(&g_game);
        if (g_game.turn != turn_before) {
            /* Descending, dying or the world moving all want a new frame. */
            g_progress_dirty = 1;
        }
        if (g_game.phase == PD_PHASE_DEAD) {
            record_result();
            clear_progress();
        } else if (g_game.phase == PD_PHASE_WON) {
            record_result();
            clear_progress();
        } else if (g_game.phase == PD_PHASE_AMULET) {
            save_progress();
        } else if (g_game.turn != turn_before &&
                   (g_game.turn % 40u) == 0u) {
            save_progress();
        }
    }
    if (pd_audio_active(&g_audio)) {
        const uint32_t render_interval = game_animating() ? 2u : 5u;
        if (g_progress_dirty || g_render_phase % render_interval == 0u)
            render_frame();
        ++g_render_phase;
    } else {
        render_frame();
    }
    g_progress_dirty = 0;
    update_clock_period();
}

pxa::Task<void> load_resources() {
    struct Texture {uint8_t slot;const char* path;};
    static constexpr Texture textures[]{
        {PD_TEXTURE_TILES,"assets/raster/pd_tile_atlas.pxr"},
        {PD_TEXTURE_WALLS,"assets/raster/pd_wall_atlas.pxr"},
        {PD_TEXTURE_SPRITES,"assets/raster/pd_sprite_atlas.pxr"},
        {PD_TEXTURE_FONT_ASCII,"assets/raster/pd_font_ascii.pxr"},
        {PD_TEXTURE_FONT_CJK,"assets/raster/pd_font_cjk.pxr"},
        {PD_TEXTURE_FONT_CJK_EXTRA,"assets/raster/pd_font_cjk_extra.pxr"},
        {PD_TEXTURE_UI,"assets/raster/pd_ui_atlas.pxr"},
        {PD_TEXTURE_TITLE,"assets/raster/pd_title_atlas.pxr"},
        {PD_TEXTURE_FIRE,"assets/raster/pd_fire_atlas.pxr"}};
    for(const auto& texture:textures){
        auto asset=co_await context->assets().load(pxa::AssetKind::texture,texture.path);
        if(!asset){char message[160];std::snprintf(message,sizeof(message),"PDCPP asset load failed: %s (%d)",texture.path,int(asset.error()));
            (void)context->log().write(pxa::LogLevel::error,message);co_return std::unexpected(asset.error());}
        auto bound=renderer->bind_asset(*asset,{texture.slot});
        if(!bound)co_return bound;
        // Renderer retains the resource. Release the temporary Guest handle.
    }
    co_return pxa::Result<void>{};
}
int upload_resources(void) {
    if(!pxa::game::Upload(*renderer,std::as_writable_bytes(std::span{g_upload})).palette(pd_palette))return 0;
    {
        /* One-colour texture used by the remembered-terrain overlay. */
        for (uint32_t index = 0; index < 16u * 16u; ++index)
            g_fog_texels[index] = PD_FOG_INDEX;
        if (arcade::upload_texture_index8(
                *renderer, PD_TEXTURE_FOG, 16, 16, g_fog_texels, g_upload,
                sizeof(g_upload)) !=
            (int32_t)(20u + 16u * 16u))
            return 0;
    }
    for (uint32_t row = 0; row < 16u; ++row) {
        for (uint32_t column = 0; column < 64u; ++column) {
            const uint32_t fade = column / 16u;
            static const uint8_t pattern_values[16] = {
                0, 8, 2, 10, 12, 4, 14, 6,
                3, 11, 1, 9, 15, 7, 13, 5};
            const uint32_t pattern =
                pattern_values[(column & 3u) + (row & 3u) * 4u];
            g_search_texels[row * 64u + column] =
                pattern < 16u - fade * 4u ? PD_SEARCH_INDEX : 0;
        }
    }
    if (arcade::upload_texture_index8(
            *renderer, PD_TEXTURE_SEARCH, 64, 16, g_search_texels,
            g_upload, sizeof(g_upload)) !=
        (int32_t)(20u + 64u * 16u))
        return 0;
    return 1;
}
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event){
        pd_audio_event(&g_audio,event);
        if(event.service==3&&event.opcode==0x8002){if(auto metrics=pxa::ui::decode_display_metrics(event.payload)){
            bool resize=metrics->width!=display.width||metrics->height!=display.height||metrics->density_q16!=display.density_q16;apply_display(*metrics);
            if(resize&&!initializing)(void)context->tasks().start(initialize());else{rebuild_layout();render_frame();}}
            return true;}
        if(auto controller=pxa::ui::decode_controller(event)){if(catalog_ready&&!initializing)handle_controller(*controller);return true;}
        if(event.service==4&&event.opcode==0x8001&&event.payload.size()==8){auto now=pxa::wire::get64(event.payload.data());handle_tick(now);
            if(renderer&&now-perf_start>=5000000){perf_start=now;if(auto t=renderer->telemetry()){
                char msg[224];std::snprintf(msg,sizeof(msg),"PDCPP perf submitted=%llu rendered=%llu visible=%llu raster_us=%llu queue_us=%llu present_us=%llu dropped=%llu draw=%u turn=%u phase=%u",(unsigned long long)t->submitted_frames,(unsigned long long)t->rendered_frames,(unsigned long long)t->visible_frames,(unsigned long long)t->host_raster_us,(unsigned long long)t->queue_wait_us,(unsigned long long)t->present_us,(unsigned long long)t->dropped_frames,t->last_draw_list_bytes,g_game.turn,g_game.phase);
                (void)context->log().write(pxa::LogLevel::info,msg);}}
            return true;}
        return false;
    }
    pxa::BackAction on_back(){if(!pd_input_back(&g_game))return pxa::BackAction::close;g_progress_dirty=1;render_frame();update_clock_period();return pxa::BackAction::stay;}
    void on_background(pxa::Context&){save_progress();pd_audio_stop(&g_audio);(void)context->clock().set_period(0);}
    void on_foreground(pxa::Context& ctx){g_audio.bank.resume();g_clock_period=0;update_clock_period();(void)ctx.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);g_progress_dirty=1;render_frame();}
};
}
PXA_APPLICATION(PixelDungeon)
