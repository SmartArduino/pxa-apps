#pragma once
#include "scene.hpp"

namespace sky {

enum class Action { none, steer, cannon, missile, boost, eject, camera, pause, help, start, escort, fleet, home, aircraft, roll, deck };
enum class Menu { none, paused, help };
struct Layout {
    Rect safe, fire, missile, boost, eject, pause, camera, primary, secondary, home, aircraft, escort, fleet, roll, deck;
    Point stick;
    int radius=29;
    bool compact() const { return safe.w<158; }
    Layout(Rect bounds) : safe(bounds) {
        const int right=safe.x+safe.w,bottom=safe.y+safe.h;
        const int bw=std::clamp(safe.w/5,39,58), bh=compact() ? 23 : 28, gap=5;
        fire={right-bw,bottom-bh,bw,bh};
        missile={right-bw,bottom-2*bh-gap,bw,bh};
        boost={right-bw*2-gap,bottom-bh,bw,bh};
        eject={right-bw*2-gap,bottom-2*bh-gap,bw,bh};
        pause={right-38,safe.y,38,24};
        camera=compact() ? Rect{pause.x,pause.y+27,38,24} : Rect{pause.x-43,pause.y,38,24};
        radius=std::clamp(safe.w/8,22,30);
        stick={safe.x+radius,bottom-radius};
        const int center=safe.x+safe.w/2;
        const int row=std::min(safe.w,166), first=row*3/5;
        primary={center-row/2,bottom-38,first,34};
        secondary={primary.x+first+6,bottom-38,row-first-6,34};
        home={center-39,bottom-76,78,29};
        aircraft={primary.x,home.y,primary.w,home.h};
        escort={secondary.x,home.y,secondary.w,home.h};
        fleet={center-48,home.y-33,96,28};
        roll={safe.x,stick.y-radius-30,44,23};
        deck={safe.x,roll.y-28,53,23};
    }

    Action action(Phase phase,Menu menu,int x,int y,bool fleet_mission=false) const {
        if (menu==Menu::help) return primary.contains(x,y) ? Action::help : Action::none;
        if (menu==Menu::paused) {
            if (primary.contains(x,y)) return Action::pause;
            if (secondary.contains(x,y)) return Action::help;
            return home.contains(x,y) ? Action::home : Action::none;
        }
        if (phase==Phase::title || phase==Phase::rescued || phase==Phase::lost ||
            phase==Phase::escort_lost || phase==Phase::victory) {
            if (primary.contains(x,y)) return Action::start;
            if (phase==Phase::title && aircraft.contains(x,y)) return Action::aircraft;
            if (phase==Phase::title && escort.contains(x,y)) return Action::escort;
            if (phase==Phase::title && fleet.contains(x,y)) return Action::fleet;
            return secondary.contains(x,y) ? (phase==Phase::title ? Action::help : Action::home) : Action::none;
        }
        if (pause.contains(x,y)) return Action::pause;
        if (camera.contains(x,y)) return Action::camera;
        if (fire.contains(x,y) && (phase==Phase::flight || phase==Phase::ejection)) return Action::cannon;
        if (phase==Phase::flight) {
            if (fleet_mission && deck.contains(x,y)) return Action::deck;
            if (missile.contains(x,y)) return Action::missile;
            if (boost.contains(x,y)) return Action::boost;
            if (eject.contains(x,y)) return Action::eject;
            if (roll.contains(x,y)) return Action::roll;
        }
        if ((phase==Phase::flight || phase==Phase::parachute) &&
            x<stick.x+radius+13 && y>=stick.y-radius-12)
            return Action::steer;
        return Action::none;
    }
};

int cockpit_deck_y(const Layout& layout,int height);
void overlays(Canvas& d,const Layout& l,const Game& g,Menu menu,Input input,bool failed,View view);

} // namespace sky
