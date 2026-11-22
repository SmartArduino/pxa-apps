#include "ui.hpp"
#include "clouds.hpp"

#include <pxa/app.hpp>
#include <pxa/ui_geometry.hpp>
#include <bit>
#include <chrono>
#include <optional>

namespace sky {

class App {
public:
    static constexpr pxa::game::LoopOptions loop_options{
        .simulation_step_us=16667,.frame_period_ms=33,.maximum_updates=8};

    auto view() {
        return pxa::ui::Canvas().input_only().on_pointer(
            [this](const pxa::ui::CanvasPointer& event) { pointer(event); });
    }
    pxa::Result<void> on_start(pxa::Context& ctx,std::span<const std::byte> config) {
        context=&ctx;
        if (auto value=pxa::ui::decode_start_display(config)) display=*value;
        world.generate();
        game.reset(Phase::title);
        return ctx.window().configure({
            .edge_to_edge=true,
            .status_bar_mode=pxa::WindowBarMode::hidden,
            .navigation_bar_mode=pxa::WindowBarMode::hidden});
    }
    void on_window_changed(const pxa::WindowMetrics& metrics) {
        if (metrics.revision<window.revision) return;
        if (window.focused && !metrics.focused) {
            if (game.in_air() && menu==Menu::none) menu=Menu::paused;
            clear_input();
        }
        window=metrics;
        dirty=true;
        update_layout();
    }
    void on_background(pxa::Context&) {
        if (game.in_air() && menu==Menu::none) menu=Menu::paused;
        clear_input();
        dirty=true;
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e) {
        if (e.service!=3 || e.opcode!=0x8002 || e.token) return false;
        auto next=pxa::ui::decode_display_metrics(e);
        if (!next) return std::unexpected(next.error());
        display=*next;
        clear_input();
        update_layout();
        return true;
    }
    pxa::BackAction on_back() {
        if (menu==Menu::help) activate(Action::help);
        else if (game.in_air()) activate(Action::pause);
        else if (game.phase!=Phase::title) activate(Action::home);
        else return pxa::BackAction::close;
        return pxa::BackAction::stay;
    }
    void on_controller(pxa::Context&,const pxa::input::ControllerState& e) {
        using namespace pxa::input;
        if (!controller.update(e)) return;
        if (menu==Menu::none && controller.pressed(controller_select)) {
            if (game.phase==Phase::title) { activate(Action::fleet); return; }
            if (game.phase==Phase::flight && game.sortie==Sortie::fleet) { activate(Action::deck); return; }
        }
        if (controller.pressed(controller_a)) {
            if (menu==Menu::help) activate(Action::help);
            else if (menu==Menu::paused) activate(Action::pause);
            else if (!game.in_air()) activate(Action::start);
            else if (game.phase==Phase::ejection) deploy();
            else if (game.fleet.deck.phase==DeckPhase::parked) activate(Action::deck);
            else if (game.phase==Phase::flight) game.shoot();
        }
        if (controller.pressed(controller_b)) {
            if (menu==Menu::help) activate(Action::help);
            else if (game.phase==Phase::flight && menu==Menu::none) activate(Action::missile);
            else if (menu==Menu::paused || !game.in_air()) activate(Action::help);
        }
        if (controller.pressed(controller_y)) {
            if (game.phase==Phase::title && menu==Menu::none) activate(Action::escort);
            else if (menu==Menu::paused || (!game.in_air() && game.phase!=Phase::title)) activate(Action::home);
            else if (menu==Menu::none) activate(Action::eject);
        }
        if (controller.pressed(controller_start)) {
            if (menu==Menu::help) activate(Action::help);
            else if (game.in_air()) activate(Action::pause);
            else activate(Action::start);
        }
        if (menu==Menu::none && controller.pressed(controller_l)) {
            if (game.phase==Phase::title) activate(Action::aircraft);
            else if (game.phase==Phase::flight) activate(Action::roll);
        }
        if (menu==Menu::none && controller.pressed(controller_r)) {
            if (game.phase==Phase::title) activate(Action::aircraft);
            else if (game.in_air()) activate(Action::camera);
        }
    }
    void on_update(pxa::Context&,std::uint32_t delta_us) {
        if (!surface || menu!=Menu::none) return;
        const float dt=delta_us*.000001f;
        display_time+=dt;
        lock_notice=std::max(0.f,lock_notice-dt);
        const Phase old=game.phase;
        const unsigned score=game.score;
        const unsigned allied=game.wingman_score;
        const unsigned hull=game.hull;
        const int wing=game.player.damage.missing_tip;
        const unsigned ship_hull=game.convoy.hull;
        const bool supplied=game.convoy.resupplied;
        const int objectives=game.sortie==Sortie::fleet ? game.fleet.remaining() : 0;
        const bool carrier_supplied=game.fleet.supplied;
        const unsigned deck_sorties=game.fleet.launched;
        const auto deck_phase=game.fleet.deck.phase;
        game.update(input(),dt);
        if (game.score!=score) report("Enemy down");
        if (game.wingman_score!=allied) report("Wingman scored");
        if (game.hull<hull) report("Airframe hit");
        if (!wing && game.player.damage.missing_tip) report("Wing tip lost");
        if (game.convoy.hull<ship_hull) report("Convoy hit");
        if (!supplied && game.convoy.resupplied) report("Convoy resupply");
        if (objectives && game.fleet.remaining()<objectives) report("Surface target destroyed");
        if (!carrier_supplied && game.fleet.supplied) report("Carrier resupply");
        if (deck_sorties!=game.fleet.launched) report("Enemy carrier launched fighter");
        if (deck_phase!=game.fleet.deck.phase && game.phase==Phase::flight) {
            report_deck();
            dirty=true;
        }
        if (old!=game.phase) { report_phase(); dirty=true; }
        if (!game.in_air() && old!=game.phase) clear_contacts();
    }
    void on_frame(pxa::Context& ctx,pxa::game::FrameTick tick) {
        if (!surface || display_w!=int(window.pixel_width) || display_h!=int(window.pixel_height)) {
            if (!opening && tick.timestamp_us>=retry_at) {
                opening=true;
                retry_at=tick.timestamp_us+700000;
                if (!ctx.tasks().start(open_surface())) opening=false;
            }
            return;
        }
        if (!clouds.ready()) {
            if (!cloud_seed) {
                const auto stamp=tick.timestamp_us;
                cloud_seed=hash(std::uint32_t(stamp),std::uint32_t(stamp>>32));
            }
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(10);
            do {
                clouds.generate_next(*cloud_seed);
            } while (!clouds.ready() && std::chrono::steady_clock::now()<deadline);
            dirty=true;
        }
        if (!dirty && (menu!=Menu::none || (!game.in_air() && game.phase!=Phase::title))) return;
        auto frame=surface->acquire();
        if (!frame) return;
        // The mapped RGB565 buffers are allocated on 64-byte boundaries by
        // the Guest SDK. Wasm and the supported AOT targets are little-endian.
        static_assert(std::endian::native==std::endian::little);
        auto pixels=frame->pixels();
        const auto stride=frame->stride_bytes();
        if (pixels.empty() || stride%sizeof(Color) ||
            reinterpret_cast<std::uintptr_t>(pixels.data())%alignof(Color)) return;
        Canvas canvas{reinterpret_cast<Color*>(pixels.data()),depth.data(),
                      int(surface->width()),int(surface->height()),int(stride/sizeof(Color))};
        const auto start=std::chrono::steady_clock::now();
        draw(canvas);
        overlays(canvas,layout,game,menu,input(),lock_notice>0,view_mode);
        const bool submitted=bool(frame->present(next_frame));
        if (submitted) { ++next_frame; dirty=false; }
        const auto spent=std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()-start).count();
        if (submitted) {
            const auto interval=last_present && tick.timestamp_us>=last_present ? tick.timestamp_us-last_present : 0;
            last_present=tick.timestamp_us;
            adjust_detail(spent,interval);
        }
    }

private:
    pxa::Context* context=nullptr;
    pxa::WindowMetrics window{};
    pxa::ui::DisplayMetrics display{};
    World world;
    Game game{world};
    CloudField clouds;
    std::array<std::uint16_t,Canvas::max_pixels> depth{};
    std::array<Color,Canvas::max_pixels/4+1024> low_pixels{};
    std::optional<pxa::Surface> surface;
    pxa::input::Controller controller;
    Layout layout{{}};
    struct Contact { pxa::input::PointerCapture capture; Action action=Action::none; Point point; };
    std::array<Contact,3> contacts{};
    Menu menu=Menu::none, menu_back=Menu::none;
    View view_mode=View::chase;
    float display_time=0,lock_notice=0;
    int display_w=0,display_h=0,scale=1,offset_x=0,offset_y=0;
    std::uint64_t next_frame=1,retry_at=0,last_present=0;
    std::optional<std::uint32_t> cloud_seed;
    enum class RenderScale { full, half, two_fifths, third };
    RenderScale scene_scale=RenderScale::half;
    int slow_frames=0,fast_frames=0;
    bool opening=false,dirty=true,roll_mode=false;

    void draw(Canvas& canvas) {
        const View view=active_view(game.phase,view_mode);
        const auto scale_factor=view==View::cockpit ? std::max(RenderScale::half,scene_scale) : scene_scale;
        if (scale_factor==RenderScale::full) {
            render_scene(canvas,game,clouds,display_time,layout.compact(),view);
            return;
        }
        const int divisor=3+int(scale_factor);
        const int width=(canvas.width*2+divisor-1)/divisor;
        const int height=(canvas.height*2+divisor-1)/divisor;
        const auto bank=view==View::cockpit ? CanopyBank::project(game.player.bank,width,height) : CanopyBank{};
        const bool rolled=bank.cosine!=1 || bank.sine!=0;
        const int rows=view==View::cockpit ? cockpit_deck_y(layout,canvas.height)+6 : canvas.height;
        int source_rows=(rows*2+divisor-1)/divisor;
        if (rolled) {
            const float focus=canvas.height*CanopyBank::eye_line;
            const float far_y=bank.cosine>=0 ? rows-focus : -focus;
            const float highest=std::abs(bank.sine)*canvas.width*.5f+bank.cosine*far_y;
            source_rows=std::clamp(int(std::ceil(height*(CanopyBank::eye_line+
                highest/(canvas.height*bank.extent))))+1,1,height);
        }
        Canvas reduced{low_pixels.data(),depth.data(),width,source_rows,width};
        render_scene(reduced,game,clouds,display_time,layout.compact(),view,height);
        if (rolled) banked_view(canvas,reduced,bank,rows,height);
        else if (scale_factor==RenderScale::half) upscale<2>(canvas,reduced,rows);
        else if (scale_factor==RenderScale::third) upscale<3>(canvas,reduced,rows);
        else upscale_two_fifths(canvas,reduced,rows);
    }
    static void banked_view(Canvas& full,const Canvas& reduced,const CanopyBank& bank,int rows,int height) {
        constexpr float fixed=65536.f;
        const float horizontal=fixed*reduced.width/(full.width*bank.extent);
        const float vertical=fixed*height/(full.height*bank.extent);
        const int step_x=int(horizontal*bank.cosine),step_y=int(-vertical*bank.sine);
        const int row_x=int(horizontal*bank.sine),row_y=int(vertical*bank.cosine);
        constexpr float focus=CanopyBank::eye_line;
        int start_x=int(reduced.width*fixed*.5f+(.5f-full.width*.5f)*step_x+(.5f-full.height*focus)*row_x);
        int start_y=int(height*fixed*focus+(.5f-full.width*.5f)*step_y+(.5f-full.height*focus)*row_y);
        for (int y=0;y<rows;++y,start_x+=row_x,start_y+=row_y) {
            auto* dest=full.pixels+y*full.stride;
            int sx=start_x,sy=start_y;
            for (int x=0;x<full.width;++x,sx+=step_x,sy+=step_y) {
                const int col=std::clamp(sx>>16,0,reduced.width-1);
                const int row=std::clamp(sy>>16,0,reduced.height-1);
                dest[x]=reduced.pixels[row*reduced.stride+col];
            }
        }
    }
    static void upscale_two_fifths(Canvas& full,const Canvas& reduced,int rows) {
        int old_source=-1;
        for (int y=0;y<rows;++y) {
            auto* row=full.pixels+y*full.stride;
            const int source_y=y*2/5;
            if (source_y==old_source) {
                std::copy_n(row-full.stride,full.width,row);
                continue;
            }
            const auto* source=reduced.pixels+source_y*reduced.stride;
            for (int x=0;x<full.width;++x) row[x]=source[x*2/5];
            old_source=source_y;
        }
    }
    template<int factor>
    static void upscale(Canvas& full,const Canvas& reduced,int rows) {
        const int complete=full.width/factor;
        for (int y=0;y<rows;y+=factor) {
            auto* row=full.pixels+y*full.stride;
            const auto* source=reduced.pixels+(y/factor)*reduced.stride;
            for (int x=0;x<complete;++x) {
                const auto color=source[x];
                for (int offset=0;offset<factor;++offset) row[x*factor+offset]=color;
            }
            if (full.width%factor) std::fill_n(row+complete*factor,full.width%factor,source[complete]);
            for (int repeat=1;repeat<factor && y+repeat<rows;++repeat)
                std::copy_n(row,full.width,row+repeat*full.stride);
        }
    }
    void adjust_detail(std::int64_t spent,std::uint64_t interval) {
        if (menu!=Menu::none) return;
        const int down=scene_scale==RenderScale::full ? 36000 : scene_scale==RenderScale::half ? 47000 : 63000;
        const int up=scene_scale==RenderScale::half ? 9000 : scene_scale==RenderScale::two_fifths ? 23000 : 24500;
        const bool slow_present=interval>74000 && interval<210000 && spent>23000 &&
                                scene_scale==RenderScale::half;
        slow_frames=spent>down || slow_present ? slow_frames+1 : 0;
        fast_frames=spent>0 && spent<up && interval<63000 ? fast_frames+1 : 0;
        if (scene_scale<RenderScale::third && slow_frames>=5) {
            scene_scale=RenderScale(int(scene_scale)+1); slow_frames=fast_frames=0;
        }
        if (scene_scale>RenderScale::full && fast_frames>=70) {
            scene_scale=RenderScale(int(scene_scale)-1); slow_frames=fast_frames=0;
        }
    }

    pxa::Task<void> open_surface() {
        if (!window.pixel_width || !window.pixel_height) {
            const auto value=co_await context->window().snapshot();
            if (!value) { opening=false; co_return std::unexpected(value.error()); }
            on_window_changed(*value);
        }
        const int w=int(window.pixel_width),h=int(window.pixel_height);
        int divisor=1;
        while (w/divisor>Canvas::max_width || (w/divisor)*(h/divisor)>Canvas::max_pixels) ++divisor;
        const auto rw=std::uint16_t(w/divisor),rh=std::uint16_t(h/divisor);
        if (rw<190 || rh<160) {
            opening=false;
            report("Display is too small for flight controls");
            co_return std::unexpected(pxa::Error::unsupported);
        }
        surface.reset();
        auto created=co_await context->surface().create_mapped({
            .width=rw,.height=rh,.buffers=2,.max_buffer_bytes=448*1024});
        if (!created) {
            opening=false;
            report("Flight surface allocation failed");
            co_return std::unexpected(created.error());
        }
        auto result=co_await created->configure({.width=rw,.height=rh});
        opening=false;
        if (!result) {
            report("Flight surface configuration failed");
            co_return result;
        }
        display_w=w; display_h=h; scale=divisor;
        offset_x=(w-rw*scale)/2;
        offset_y=(h-rh*scale)/2;
        surface.emplace(std::move(*created));
        last_present=0;
        dirty=true;
        clear_input();
        update_layout();
        report("Flight display ready");
        co_return result;
    }

    void update_layout() {
        if (!surface) return;
        const auto in=window.safe_insets;
        const auto safe=pxa::ui::safe_rectangle(display);
        const int w=surface->width(),h=surface->height();
        const auto padding=[&](int n,int outline,int offset) {
            return std::max(9,std::max(0,std::max(n,outline)-offset+scale-1)/scale+4);
        };
        const bool matching=display.width==window.pixel_width && display.height==window.pixel_height;
        const int l=padding(in.left,matching ? safe.x : 0,offset_x);
        const int t=padding(in.top,matching ? safe.y : 0,offset_y);
        const int r=padding(in.right,matching ? display_w-safe.x-safe.width : 0,display_w-w*scale-offset_x);
        const int b=padding(in.bottom,matching ? display_h-safe.y-safe.height : 0,display_h-h*scale-offset_y);
        layout=Layout{{l,t,w-l-r,h-t-b}};
    }

    void report(std::string_view text) {
        (void)context->log().write(pxa::LogLevel::info,text);
    }
    void report_deck() {
        switch (game.fleet.deck.phase) {
        case DeckPhase::free: report("Carrier controls released"); break;
        case DeckPhase::approach: report("Carrier approach"); break;
        case DeckPhase::arrest: report("Carrier touchdown"); break;
        case DeckPhase::parked: report("Carrier parked"); break;
        case DeckPhase::launch: report("Carrier launch"); break;
        }
    }
    void report_phase() {
        switch (game.phase) {
        case Phase::title: break;
        case Phase::flight:
            report(game.sortie==Sortie::fleet ? "Fleet strike started" :
                   game.sortie==Sortie::escort ? "Escort started" : "Sortie started");
            break;
        case Phase::ejection: report("Pilot ejected"); break;
        case Phase::parachute: report("Parachute deployed"); break;
        case Phase::pickup: report("Rescue aircraft caught pilot"); break;
        case Phase::rescued:
            if (game.airlift) report("Pilot recovered in flight");
            else if (game.wet)
                report(game.world.surface_at(game.pilot.x,game.pilot.z).inland()
                           ? "Pilot recovered on inland water" : "Pilot recovered at sea");
            else report("Pilot landed safely");
            break;
        case Phase::lost: report("Sortie lost"); break;
        case Phase::escort_lost: report(game.sortie==Sortie::fleet ? "Friendly carrier lost" : "Convoy lost"); break;
        case Phase::victory:
            report(game.sortie==Sortie::fleet ? "Fleet strike complete" :
                   game.sortie==Sortie::escort ? "Convoy arrived" : "Airspace clear");
            break;
        }
    }
    void deploy() {
        game.open_chute();
        if (game.phase==Phase::parachute) report_phase();
    }
    void activate(Action action) {
        dirty=true;
        switch (action) {
        case Action::start:
        case Action::escort:
        case Action::fleet:
            clear_contacts(); menu=Menu::none; lock_notice=0;
            roll_mode=false;
            game.reset(Phase::flight,action==Action::fleet ? Sortie::fleet :
                       action==Action::escort ? Sortie::escort : game.sortie);
            report_phase();
            break;
        case Action::pause:
            clear_contacts(); menu=menu==Menu::paused ? Menu::none : Menu::paused;
            report(menu==Menu::paused ? "Paused" : "Resumed"); break;
        case Action::home:
            clear_contacts(); menu=Menu::none;
            view_mode=View::chase; game.reset(Phase::title); break;
        case Action::help:
            clear_contacts();
            if (menu==Menu::help) menu=menu_back;
            else { menu_back=menu; menu=Menu::help; }
            break;
        case Action::missile:
            if (game.launch_missile()) report("Missile launched");
            else if (game.rockets && game.missile_delay<=0) lock_notice=1.1f;
            break;
        case Action::eject:
            if (game.phase==Phase::flight) { game.eject(); clear_contacts(); report_phase(); }
            break;
        case Action::camera:
            view_mode=next_view(game.phase,view_mode);
            report(view_mode==View::cockpit ? "Cockpit view" :
                   view_mode==View::overhead ? "Overhead view" : "Chase view");
            break;
        case Action::aircraft:
            game.player.type=Airframe((unsigned(game.player.type)+1)%unsigned(Airframe::count));
            report(airframe(game.player.type).code);
            break;
        case Action::roll:
            roll_mode=!roll_mode;
            report(roll_mode ? "Manual roll enabled" : "Coordinated turns enabled");
            break;
        case Action::deck:
            if (game.deck_control()) {
                clear_contacts();
                report_deck();
            }
            break;
        default: break;
        }
    }

    void clear_contacts() {
        for (auto& c:contacts) c.capture.reset();
    }
    void clear_input() {
        clear_contacts();
        controller.reset();
    }
    Input input() const {
        using namespace pxa::input;
        Input in{float(controller.horizontal()),float(-controller.vertical()),
                 controller.down(controller_a),controller.down(controller_x),
                 roll_mode && game.phase==Phase::flight};
        for (const auto& c:contacts) if (c.capture.active()) {
            if (c.action==Action::cannon) in.fire=true;
            if (c.action==Action::boost) in.boost=true;
            if (c.action==Action::steer) {
                const float extent=layout.radius*.72f;
                const float turn=(c.point.x-layout.stick.x)/extent;
                const float climb=(layout.stick.y-c.point.y)/extent;
                const float length=std::max(1.f,std::hypot(turn,climb));
                in.turn=std::abs(turn)>.13f ? turn/length : 0.f;
                in.climb=std::abs(climb)>.13f ? climb/length : 0.f;
            }
        }
        return in;
    }
    void pointer(const pxa::ui::CanvasPointer& e) {
        using pxa::input::PointerPhase;
        if (!surface) return;
        const int px=pxa::ui::canvas_to_surface_coordinate(e.x,display)-offset_x;
        const int py=pxa::ui::canvas_to_surface_coordinate(e.y,display)-offset_y;
        const Point p{px>=0 ? px/scale : -1,py>=0 ? py/scale : -1};
        if (e.action()==PointerPhase::down) {
            if (p.x<0 || p.x>=surface->width() || p.y<0 || p.y>=surface->height()) return;
            const auto action=layout.action(game.phase,menu,p.x,p.y,game.sortie==Sortie::fleet);
            if (action==Action::none) return;
            for (auto& c:contacts) {
                if (!c.capture.try_capture(e)) continue;
                c.action=action; c.point=p;
                if (action==Action::cannon && game.phase==Phase::flight) game.shoot();
                return;
            }
        } else for (auto& c:contacts) {
            if (!c.capture.owns(e)) continue;
            c.point=p;
            if (e.ended()) {
                c.capture.release(e);
                if (e.action()==PointerPhase::up &&
                    c.action==layout.action(game.phase,menu,p.x,p.y,game.sortie==Sortie::fleet)) {
                    if (c.action==Action::cannon && game.phase==Phase::ejection) deploy();
                    else activate(c.action);
                }
            }
            return;
        }
    }
};

} // namespace sky

PXA_GAME(sky::App)
