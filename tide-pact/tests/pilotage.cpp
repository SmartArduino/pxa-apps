#include "game.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

using namespace tide;

int main() {
    const auto ocean=[](Point) { return true; };
    for (int index=0;index<int(std::size(PilotRegions));++index) {
        const auto& area=PilotRegions[index];
        std::array<unsigned char,PilotMapBytes> bytes{};
        auto file=std::ifstream(std::string(area.asset),std::ios::binary);
        assert(file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()));
        assert(file.peek()==EOF && bytes[0]=='P' && bytes[1]=='C' && bytes[2]=='H' && bytes[3]=='1');
        const PilotWater water{bytes.data()};
        for (int site=area.first;site<area.first+area.count;++site) {
            const auto& mark=PilotSites[site];
            if (mark.berth.x>=0) assert(water.navigable(mark.berth));
        }
        Game ship;
        ship.s.origin=area.port;
        assert(ship.start_helm(-1));
        assert(ship.pilot_region()==index && ship.begin_pilotage(index,water));
        assert(ship.paused && ship.pilot_berth(water)==area.first && !ship.dock_helm(ocean));
        const int heading=ship.s.pilot.heading, x=ship.s.pilot.x, y=ship.s.pilot.y;
        ship.set_pilot_heading(heading+1);
        assert(ship.s.pilot.heading==(heading+1)%16);
        const auto advanced=ship;
        State loaded;
        assert(decode(loaded,encode(advanced.s)) && loaded.pilot.x==x &&
               loaded.pilot.y==y && loaded.pilot.region==index+1);
        const auto fixed=pilot_to_world(area,{x,y});
        assert(fixed.x==ship.s.helm_x && fixed.y==ship.s.helm_y);

        bool moved=false;
        for (int bearing=0;bearing<16 && !moved;++bearing) {
            auto course=ship;
            course.set_pilot_heading(bearing);
            course.paused=false;
            course.pilot_tick(water);
            moved=(course.s.pilot.x!=x || course.s.pilot.y!=y) && !course.paused;
            if (moved) assert(course.s.pilot.minutes==PilotTickMinutes);
        }
        assert(moved);
        Game harbour=ship;
        const auto village=PilotSites[area.first+1].berth;
        assert(water.navigable(village));
        harbour.s.pilot.x=village.x;
        harbour.s.pilot.y=village.y;
        assert(harbour.moor_pilotage(water) && harbour.s.pilot.moored==area.first+2);
        const int gold=harbour.s.gold;
        harbour.s.water=12;
        harbour.s.stores=12;
        assert(harbour.pilot_supplies(true) && harbour.s.water==20 && harbour.s.gold==gold-12);
        assert(decode(loaded,encode(harbour.s)) && loaded.pilot.moored==area.first+2);
        harbour.cast_off_pilotage();
        assert(!harbour.pilot_supplies(true) && harbour.leave_pilotage(ocean) && !harbour.s.pilot.region);

        assert(ship.moor_pilotage(water) && ship.s.origin==area.port && !ship.sea());
        assert(ship.s.pilot_visits&(1U<<area.first));
    }
    std::puts("Pilotage charts, bearings, departure, mooring, supplies and saves passed.");
}
