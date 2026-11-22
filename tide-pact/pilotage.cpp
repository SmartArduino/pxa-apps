#include "game.hpp"

#include <cmath>
#include <limits>

namespace tide {
namespace {

constexpr int width=PilotWidth*PilotSubcell, height=PilotHeight*PilotSubcell;
constexpr float radians=0.0174532925f;
constexpr float world_x=ChartLongitudeScale*16, world_y=ChartLatitudeScale*16;

float north_south_miles(const PilotRegion& region) {
    return (region.north-region.south)*60.f/PilotHeight;
}

float miles_between(const PilotRegion& region,Point a,Point b) {
    const float x=(a.x-b.x)*pilot_cell_miles(region)/PilotSubcell;
    const float y=(a.y-b.y)*north_south_miles(region)/PilotSubcell;
    return x*x+y*y;
}

Point nearby_water(PilotWater map,Point at) {
    if (map.navigable(at)) return at;
    const int x=at.x/PilotSubcell, y=at.y/PilotSubcell;
    constexpr int reach=16;
    Point best{-1,-1};
    int shortest=reach*reach+1;
    for (int j=std::max(0,y-reach);j<=std::min(PilotHeight-1,y+reach);++j)
        for (int i=std::max(0,x-reach);i<=std::min(PilotWidth-1,x+reach);++i) {
            const int radius=(i-x)*(i-x)+(j-y)*(j-y);
            if (radius<shortest && map.at({i,j})<4) {
                shortest=radius;
                best={i*PilotSubcell+8,j*PilotSubcell+8};
            }
        }
    return best;
}

bool clear_approach(Point from,Point to,PilotWater map) {
    const int dx=to.x-from.x,dy=to.y-from.y;
    const int count=1+std::max(std::abs(dx),std::abs(dy))/4;
    for (int i=1;i<=count;++i)
        if (!map.navigable({from.x+dx*i/count,from.y+dy*i/count})) return false;
    return true;
}

} // namespace

Point pilot_from_world(const PilotRegion& region,Point fixed) {
    const float lon=ChartWest+fixed.x/world_x;
    const float lat=ChartNorth-fixed.y/world_y;
    return {int(std::lround((lon-region.west)*width/(region.east-region.west))),
            int(std::lround((region.north-lat)*height/(region.north-region.south)))};
}

Point pilot_to_world(const PilotRegion& region,Point local) {
    const float lon=region.west+(region.east-region.west)*local.x/width;
    const float lat=region.north-(region.north-region.south)*local.y/height;
    return {int(std::lround((lon-ChartWest)*world_x)),
            int(std::lround((ChartNorth-lat)*world_y))};
}

float pilot_cell_miles(const PilotRegion& region) {
    return (region.east-region.west)*60.f*std::cos((region.north+region.south)*.5f*radians)/PilotWidth;
}

int pilot_bearing(Point offset,float aspect) {
    int result=0;
    float best=-std::numeric_limits<float>::infinity();
    for (int i=0;i<16;++i) {
        const auto v=PilotDirections[i];
        const float dot=offset.x*aspect*v.x+float(offset.y)*v.y;
        if (dot>best) { best=dot; result=i; }
    }
    return result;
}

int Game::pilot_region() const {
    if (s.pilot.region) return s.pilot.region-1;
    if (!s.helm || encounter()) return -1;
    const Point world{s.helm_x,s.helm_y};
    const auto home=Anchor[s.origin];
    const bool at_departure=s.wake_count<=2 &&
        std::abs(world.x-home.x*16)+std::abs(world.y-home.y*16)<=16;
    for (int i=0;i<int(std::size(PilotRegions));++i) {
        const auto& region=PilotRegions[i];
        const auto point=pilot_from_world(region,world);
        if ((at_departure && region.port==s.origin) ||
            (point.x>=0 && point.x<width && point.y>=0 && point.y<height)) return i;
    }
    return -1;
}

bool Game::begin_pilotage(int index,PilotWater map) {
    if (index<0 || index!=pilot_region() || !s.helm || s.pilot.region || !map.bytes) return false;
    const auto& region=PilotRegions[index];
    const auto home=Anchor[s.origin];
    const bool at_departure=s.origin==region.port && s.wake_count<=2 &&
        std::abs(s.helm_x-home.x*16)+std::abs(s.helm_y-home.y*16)<=16;
    const auto point=at_departure?PilotSites[region.first].berth:
        nearby_water(map,pilot_from_world(region,{s.helm_x,s.helm_y}));
    if (!map.navigable(point)) {
        notice="请从领航图的水域驶入";
        return false;
    }
    s.pilot.x=static_cast<std::uint16_t>(point.x);
    s.pilot.y=static_cast<std::uint16_t>(point.y);
    s.pilot.region=index+1;
    s.pilot.heading=s.heading*2;
    s.pilot.moored=0;
    const auto world=pilot_to_world(region,point);
    s.helm_x=world.x;
    s.helm_y=world.y;
    s.wake_head=s.wake_count=0;
    record_wake();
    paused=true;
    notice="已展开近岸领航图";
    return true;
}

bool Game::leave_pilotage(WaterTest ocean) {
    if (!s.pilot.region || !ocean) return false;
    if (s.pilot.moored) { notice="先解缆再切换远航"; return false; }
    const auto& area=PilotRegions[s.pilot.region-1];
    const auto here=position();
    const Point local{s.pilot.x,s.pilot.y};
    if (miles_between(area,local,PilotSites[area.first].berth)<.85f*.85f) {
        s.helm_x=Anchor[area.port].x*16;
        s.helm_y=Anchor[area.port].y*16;
    } else if (!ocean(here)) {
        int nearest=5;
        Point target=here;
        for (int y=-2;y<=2;++y)
            for (int x=-2;x<=2;++x) {
                const int distance=x*x+y*y;
                if (distance>=nearest) continue;
                const Point candidate{here.x+x,here.y+y};
                if (ocean(candidate)) { nearest=distance; target=candidate; }
            }
        if (nearest==5) { notice="沿水道驶向外海再切换海图"; return false; }
        s.helm_x=target.x*16;
        s.helm_y=target.y*16;
    }
    const auto minutes=s.pilot.minutes;
    s.pilot={.minutes=minutes};
    record_wake();
    paused=true;
    notice="回到远航海图";
    return true;
}

void Game::set_pilot_heading(int heading) {
    if (!s.pilot.region || s.pilot.moored) return;
    s.pilot.heading=static_cast<std::uint8_t>(heading&15);
    s.heading=static_cast<std::uint8_t>(((s.pilot.heading+1)/2)&7);
}

int Game::pilot_speed(PilotWater map) const {
    if (!s.pilot.region || s.pilot.moored) return 0;
    const auto depth=map.at({s.pilot.x/PilotSubcell,s.pilot.y/PilotSubcell});
    const int speed=speed_in_tenths_of_a_knot();
    return depth>=3?speed/2:speed;
}

void Game::pilot_tick(PilotWater map) {
    if (!s.helm || !s.pilot.region || s.pilot.moored || paused || encounter() || !map.bytes) return;
    const auto& area=PilotRegions[s.pilot.region-1];
    const Point start{s.pilot.x,s.pilot.y};
    const auto bearing=PilotDirections[s.pilot.heading];
    const float miles=pilot_speed(map)*PilotTickMinutes/600.f;
    const int dx=int(std::lround(miles*bearing.x*PilotSubcell/(100.f*pilot_cell_miles(area))));
    const int dy=int(std::lround(miles*bearing.y*PilotSubcell/(100.f*north_south_miles(area))));
    const int pieces=1+std::max(std::abs(dx),std::abs(dy))/4;
    for (int i=1;i<=pieces;++i) {
        const Point next{start.x+dx*i/pieces,start.y+dy*i/pieces};
        if (next.x<0 || next.x>=width || next.y<0 || next.y>=height) {
            paused=true;
            notice="已到图边，点远航可继续";
            break;
        }
        if (!map.navigable(next)) {
            s.hull-=s.reefed?1:3;
            if (s.hull<=0) { rescue(); return; }
            paused=true;
            notice="碰到岸边，转舵后再开船";
            break;
        }
        s.pilot.x=static_cast<std::uint16_t>(next.x);
        s.pilot.y=static_cast<std::uint16_t>(next.y);
    }
    const auto world=pilot_to_world(area,{s.pilot.x,s.pilot.y});
    s.helm_x=world.x;
    s.helm_y=world.y;
    record_wake();
    s.pilot.minutes+=PilotTickMinutes;
    if (s.pilot.minutes>=60) {
        s.pilot.minutes-=60;
        s.helm_pulse=(s.helm_pulse+1)%HelmPulsesPerWatch;
        if (!s.helm_pulse && advance_watch()) consume_day(conditions());
    }
}

int Game::pilot_berth(PilotWater map) const {
    if (!s.pilot.region || !map.bytes) return -1;
    if (s.pilot.moored) return s.pilot.moored-1;
    const auto& area=PilotRegions[s.pilot.region-1];
    const Point here{s.pilot.x,s.pilot.y};
    int berth=-1;
    float best=.85f*.85f;
    for (int i=area.first;i<area.first+area.count;++i) {
        const auto at=PilotSites[i].berth;
        if (at.x<0) continue;
        const float distance=miles_between(area,here,at);
        if (distance<best && clear_approach(here,at,map)) { berth=i; best=distance; }
    }
    return berth;
}

bool Game::moor_pilotage(PilotWater map) {
    const int site=pilot_berth(map);
    if (site<0 || s.pilot.moored) return false;
    const auto& area=PilotRegions[s.pilot.region-1];
    const auto berth=PilotSites[site].berth;
    s.pilot.x=static_cast<std::uint16_t>(berth.x);
    s.pilot.y=static_cast<std::uint16_t>(berth.y);
    s.pilot_visits|=1U<<site;
    paused=true;
    if (site==area.first) {
        s.chart_marks|=place_bit(area.port);
        s.destination=area.port;
        arrive();
    } else {
        s.pilot.moored=site+1;
        const auto world=pilot_to_world(area,berth);
        s.helm_x=world.x;
        s.helm_y=world.y;
        notice="已靠上泊地";
    }
    return true;
}

void Game::cast_off_pilotage() {
    if (!s.pilot.moored) return;
    s.pilot.moored=0;
    paused=true;
    notice="已解缆，调整航向后开船";
}

bool Game::pilot_supplies(bool water) {
    if (!s.pilot.moored) return false;
    const int amount=water?std::min(8,water_capacity()-s.water):std::min(6,food_capacity()-s.stores);
    const int cost=water?12:18;
    if (amount<=0 || s.gold<cost) return false;
    earn(-cost);
    if (water) s.water+=amount;
    else s.stores+=amount;
    notice=water?"水桶已经装满这一批":"已把这批口粮搬上船";
    return true;
}

} // namespace tide
