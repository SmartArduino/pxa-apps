#pragma once
#include "draw.hpp"
#include "game.hpp"

namespace sky {

struct CloudField;

enum class View { chase, overhead, cockpit };
inline View active_view(Phase phase,View requested) {
    return requested==View::cockpit && phase!=Phase::flight ? View::chase : requested;
}
inline View next_view(Phase phase,View requested) {
    switch (active_view(phase,requested)) {
    case View::chase: return View::overhead;
    case View::overhead: return phase==Phase::flight ? View::cockpit : View::chase;
    case View::cockpit: return View::chase;
    }
    return View::chase;
}

struct CanopyBank {
    static constexpr float eye_line=.43f;
    float cosine=1,sine=0,extent=1;
    static CanopyBank project(float bank,int width,int height);
    Vec to_screen(Vec p,float cx,float cy) const {
        const float x=(p.x-cx)*extent,y=(p.y-cy)*extent;
        return {cx+x*cosine-y*sine,cy+x*sine+y*cosine,p.z};
    }
};

Camera flight_camera(const Game& game,int w,int h,float orbit,bool compact,View view);
void render_scene(Canvas& canvas,const Game& game,const CloudField& sky,float display_time,bool compact,
                  View view,int viewport_height=0);

} // namespace sky
