#include "game.hpp"

#include <cmath>
#include <limits>

namespace tide {
namespace {

constexpr int width=PilotWidth*PilotSubcell, height=PilotHeight*PilotSubcell;
constexpr float radians=0.0174532925f;
constexpr float world_x=ChartLongitudeScale*16, world_y=ChartLatitudeScale*16;
constexpr float berth_radius=.48f;

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
    const int count=std::max(std::abs(dx),std::abs(dy));
    for (int i=1;i<count;++i) {
        const Point at{from.x+dx*i/count,from.y+dy*i/count};
        if (!map.navigable(at)) return false;
        // Leave room for movement rounded to the ship's fixed-point position.
        if (i>2 && i<count-2 &&
            (!map.navigable({at.x-2,at.y}) || !map.navigable({at.x+2,at.y}) ||
             !map.navigable({at.x,at.y-2}) || !map.navigable({at.x,at.y+2}))) return false;
    }
    if (!map.navigable(to)) return false;
    return true;
}

bool departing(const State& state) {
    const auto home=Anchor[state.origin];
    return state.wake_count<=2 &&
        std::abs(state.helm_x-home.x*16)+std::abs(state.helm_y-home.y*16)<=16;
}

Point world_entry(const State& state) {
    return departing(state)?PilotWorldBerths[state.origin]:Point{state.helm_x,state.helm_y};
}

bool within_patch(Point p) {
    return p.x>=0 && p.x<width && p.y>=0 && p.y<height;
}

} // namespace

Point pilot_patch_origin(int patch) {
    return {(patch%PilotTilesX*30-1)*16,(patch/PilotTilesX*14-1)*16};
}

Point pilot_from_atlas(const PilotRegion& region,Point atlas) {
    const float lon=ChartWest+atlas.x/(world_x*8);
    const float lat=ChartNorth-atlas.y/(world_y*8);
    return {int(std::lround((lon-region.west)*width/(region.east-region.west))),
            int(std::lround((region.north-lat)*height/(region.north-region.south)))};
}

Point pilot_to_atlas(const PilotRegion& region,Point local) {
    const float lon=region.west+(region.east-region.west)*local.x/width;
    const float lat=region.north-(region.north-region.south)*local.y/height;
    return {int(std::lround((lon-ChartWest)*world_x*8)),
            int(std::lround((ChartNorth-lat)*world_y*8))};
}

int pilot_detail_at(Point atlas,int padding) {
    for (int i=0;i<WorldPilotIndex;++i) {
        const auto p=pilot_from_atlas(PilotRegions[i],atlas);
        if (p.x>=-padding && p.x<width+padding && p.y>=-padding && p.y<height+padding) return i;
    }
    return -1;
}

int pilot_patch_at(Point world) {
    const int x=std::clamp(world.x/(30*16),0,PilotTilesX-1);
    const int y=std::clamp(world.y/(14*16),0,PilotTilesY-1);
    return y*PilotTilesX+x;
}

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
    const bool at_departure=departing(s);
    for (int i=0;i<int(std::size(PilotRegions));++i) {
        const auto& region=PilotRegions[i];
        const auto point=pilot_from_world(region,world);
        if ((at_departure && region.port==s.origin) ||
            within_patch(point)) return i;
    }
    return WorldPilotIndex;
}

int Game::starting_pilot_patch() const {
    return pilot_patch_at(world_entry(s));
}

PilotRegion Game::pilot_area() const {
    const int index=s.pilot.region-1;
    if (index<0) return {};
    if (index!=WorldPilotIndex) return PilotRegions[index];
    const auto origin=pilot_patch_origin(s.pilot.patch);
    const float west=ChartWest+origin.x/world_x, north=ChartNorth-origin.y/world_y;
    return {"精航海图",{},-1,WorldPilotSite,PilotSiteEnd-WorldPilotSite,west,north,
            west+32.f/ChartLongitudeScale,north-16.f/ChartLatitudeScale};
}

Point Game::pilot_atlas_position() const {
    if (!s.pilot.region) return {s.helm_x*8,s.helm_y*8};
    if (s.pilot.region<=WorldPilotIndex)
        return pilot_to_atlas(pilot_area(),{s.pilot.x,s.pilot.y});
    const auto origin=pilot_patch_origin(s.pilot.patch);
    return {origin.x*8+s.pilot.x,origin.y*8+s.pilot.y};
}

Point Game::pilot_atlas_site(int site,bool berth) const {
    if (site<WorldPilotSite) {
        for (const auto& region : PilotRegions)
            if (site>=region.first && site<region.first+region.count) {
                const auto& mark=PilotSites[site];
                if (berth && mark.berth.x<0) return {-1,-1};
                return pilot_to_atlas(region,berth?mark.berth:mark.location);
            }
        return {-1,-1};
    }
    if (site>=CoastalPilotSite) {
        const auto& place=PilotCoastalSites[site-CoastalPilotSite];
        return berth?place.berth:place.location;
    }
    const int place=site-WorldPilotSite;
    if (berth) return {PilotWorldBerths[place].x*8,PilotWorldBerths[place].y*8};
    if (place<PortCount) return PilotWorldPortMarks[place];
    const auto& info=Places[place];
    return {int(std::lround((info.longitude*.01f-ChartWest)*world_x*8)),
            int(std::lround((ChartNorth-info.latitude*.01f)*world_y*8))};
}

PilotSite Game::pilot_site(int site) const {
    if (site<WorldPilotSite) return PilotSites[site];
    const auto origin=pilot_patch_origin(s.pilot.patch);
    const auto local=[origin](Point atlas) {
        return atlas.x<0?atlas:Point{atlas.x-origin.x*8,atlas.y-origin.y*8};
    };
    const auto location=local(pilot_atlas_site(site,false)), berth=local(pilot_atlas_site(site,true));
    if (site>=CoastalPilotSite) {
        const auto& place=PilotCoastalSites[site-CoastalPilotSite];
        return {place.name,place.note,location,berth,place.kind,place.transfer};
    }
    const int place=site-WorldPilotSite;
    const auto& info=Places[place];
    const bool transfer=std::find(std::begin(PilotTransferPorts),std::end(PilotTransferPorts),place)!=
                        std::end(PilotTransferPorts);
    return {info.name,info.note,location,berth,place<PortCount?PilotCity:PilotPoint,transfer};
}

bool Game::pilot_site_visible(int site) const {
    if (site<WorldPilotSite) return true;
    if (site>=CoastalPilotSite) {
        if (pilot_site_visited(site)) return true;
        const auto& place=PilotCoastalSites[site-CoastalPilotSite];
        const auto found=[this](Point point) {
            return point.x>=0 && chart_cell(point.x/(PilotAtlasUnit*ChartCell),point.y/(PilotAtlasUnit*ChartCell));
        };
        return found(place.location) || found(place.berth);
    }
    return charted(site-WorldPilotSite);
}

bool Game::pilot_site_visited(int site) const {
    if (site<WorldPilotSite) return s.pilot_visits&(1U<<site);
    if (site>=CoastalPilotSite) return s.coast_visits&(std::uint64_t{1}<<(site-CoastalPilotSite));
    return (s.visited|s.surveyed)&place_bit(site-WorldPilotSite);
}

int Game::pilot_coastal_hint() const {
    if (s.pilot.moored<=CoastalPilotSite) return -1;
    int choice=-1, distance=1250;
    for (int id=CoastalPilotSite;id<PilotSiteEnd;++id) {
        if (pilot_site_visible(id) || PilotCoastalSites[id-CoastalPilotSite].berth.x<0) continue;
        const auto approach=pilot_approach(id,{});
        if (approach.tenths<distance) {
            distance=approach.tenths;
            choice=id;
        }
    }
    return choice;
}

bool Game::record_pilot_coast(int site) {
    if (site<CoastalPilotSite || site!=pilot_coastal_hint()) return false;
    for (const bool berth : {false,true}) {
        const auto point=pilot_atlas_site(site,berth);
        reveal_chart({point.x/PilotAtlasUnit,point.y/PilotAtlasUnit},ChartCell);
    }
    notice="船工指出下一处泊地，已记在海图";
    return true;
}

bool Game::begin_pilotage(int index,PilotWater map) {
    if (index<0 || index!=pilot_region() || !s.helm || s.pilot.region || !map.bytes ||
        (index==WorldPilotIndex)!=map.world) return false;
    s.pilot.region=index+1;
    s.pilot.patch=index==WorldPilotIndex?starting_pilot_patch():0;
    const auto region=pilot_area();
    const bool at_departure=departing(s);
    const Point entry=index==WorldPilotIndex?world_entry(s):Point{s.helm_x,s.helm_y};
    const auto point=at_departure && region.port==s.origin?PilotSites[region.first].berth:
        nearby_water(map,pilot_from_world(region,entry));
    if (!map.navigable(point)) {
        s.pilot={.minutes=s.pilot.minutes};
        notice="请从领航图的水域驶入";
        return false;
    }
    s.pilot.x=static_cast<std::uint16_t>(point.x);
    s.pilot.y=static_cast<std::uint16_t>(point.y);
    s.pilot.heading=s.heading*2;
    s.pilot.moored=0;
    pilot_remainder={};
    pilot_exit={-1,-1};
    cancel_pilot_docking();
    const auto world=pilot_to_world(region,point);
    s.helm_x=world.x;
    s.helm_y=world.y;
    s.wake_head=s.wake_count=0;
    record_wake();
    sighting(position());
    paused=true;
    notice="已展开近岸领航图";
    return true;
}

bool Game::connect_pilotage(PilotWater map) {
    if (!pilot_exiting() || !map.bytes || !map.world) return false;
    const auto world=pilot_exit;
    pilot_exit={-1,-1};
    const int patch=pilot_patch_at({world.x/8,world.y/8});
    const auto origin=pilot_patch_origin(patch);
    const Point point{world.x-origin.x*8,world.y-origin.y*8};
    if (!map.navigable(point)) {
        paused=true;
        notice="前方有岸，沿水道转向外海";
        return false;
    }
    s.pilot.region=WorldPilotIndex+1;
    s.pilot.patch=patch;
    s.pilot.x=point.x;
    s.pilot.y=point.y;
    s.helm_x=(world.x+4)/8;
    s.helm_y=(world.y+4)/8;
    cancel_pilot_docking();
    pilot_remainder={};
    return true;
}

bool Game::refine_pilotage(int index,PilotWater map) {
    if (s.pilot.region!=WorldPilotIndex+1 || s.pilot.moored || pilot_dock_target>=0 ||
        index<0 || index>=WorldPilotIndex || !map.bytes || map.world) return false;
    const auto local=pilot_from_atlas(PilotRegions[index],pilot_atlas_position());
    constexpr int margin=2*PilotSubcell;
    if (local.x<margin || local.x>=width-margin || local.y<margin || local.y>=height-margin ||
        !map.navigable(local)) return false;
    s.pilot.region=index+1;
    s.pilot.patch=0;
    s.pilot.x=local.x;
    s.pilot.y=local.y;
    pilot_remainder={};
    return true;
}

bool Game::leave_pilotage(WaterTest ocean) {
    if (!s.pilot.region || !ocean) return false;
    if (s.pilot.moored) { notice="先解缆再切换远航"; return false; }
    const auto area=pilot_area();
    const auto here=position();
    const Point local{s.pilot.x,s.pilot.y};
    if (area.port>=0 && miles_between(area,local,PilotSites[area.first].berth)<.85f*.85f) {
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
    pilot_exit={-1,-1};
    cancel_pilot_docking();
    record_wake();
    paused=true;
    notice="回到远航海图";
    return true;
}

void Game::set_pilot_heading(int heading) {
    if (!s.pilot.region || s.pilot.moored) return;
    cancel_pilot_docking();
    pilot_exit={-1,-1};
    s.pilot.heading=static_cast<std::uint8_t>(heading&15);
    s.heading=static_cast<std::uint8_t>(((s.pilot.heading+1)/2)&7);
}

bool Game::pilot_slow_water(PilotWater map) const {
    if (s.pilot.moored || pilot_dock_target>=0) return true;
    const auto& state=s.pilot;
    return map.bytes && (map.at({state.x/PilotSubcell,state.y/PilotSubcell})==3 || pilot_berth(map)>=0);
}

int Game::pilot_speed(PilotWater map,int near_berth) const {
    if (!s.pilot.region || s.pilot.moored) return 0;
    const auto depth=map.at({s.pilot.x/PilotSubcell,s.pilot.y/PilotSubcell});
    const int speed=speed_in_tenths_of_a_knot();
    const int safe=depth==3?speed/2:speed;
    if (pilot_dock_target>=0) return std::clamp(safe,12,30);
    return near_berth>=0?std::min(safe,32):safe;
}

void Game::pilot_tick(PilotWater map) {
    if (!s.helm || !s.pilot.region || s.pilot.moored || paused || encounter() || !map.bytes || pilot_exiting()) return;
    const auto area=pilot_area();
    const bool world_mode=s.pilot.region==WorldPilotIndex+1;
    const auto last=position();
    const Point start{s.pilot.x,s.pilot.y};
    const float x_scale=pilot_cell_miles(area)/PilotSubcell;
    const float y_scale=north_south_miles(area)/PilotSubcell;
    const int near=pilot_dock_target>=0?pilot_dock_target:pilot_berth(map);
    const float miles=pilot_speed(map,near)*PilotTickMinutes/600.f;
    const auto advance=[&](float cells,int& remainder) {
        const int more=remainder+int(std::lround(cells*256.f));
        remainder=more%256;
        return more/256;
    };
    Point movement;
    if (pilot_dock_target>=0) {
        const auto aim=pilot_site(pilot_dock_target).berth;
        if (!map.navigable(aim)) {
            cancel_pilot_docking();
            paused=true;
            notice="进港路线受阻，请沿水道调整";
            return;
        }
        const Point delta{aim.x-pilot_dock_from.x,aim.y-pilot_dock_from.y};
        const float distance=std::sqrt(miles_between(area,pilot_dock_from,aim));
        if (distance-pilot_dock_travel<=berth_radius) { finish_pilot_docking(pilot_dock_target); return; }
        pilot_dock_travel+=miles;
        const float part=std::min(1.f,pilot_dock_travel/distance);
        s.pilot.heading=pilot_bearing(delta,x_scale/y_scale);
        s.heading=((s.pilot.heading+1)/2)&7;
        movement={pilot_dock_from.x+int(delta.x*part)-start.x,
                  pilot_dock_from.y+int(delta.y*part)-start.y};
    } else {
        const auto bearing=PilotDirections[s.pilot.heading];
        movement={advance(miles*bearing.x/(100.f*x_scale),pilot_remainder.x),
                  advance(miles*bearing.y/(100.f*y_scale),pilot_remainder.y)};
    }
    const auto [dx,dy]=movement;
    const int pieces=1+std::max(std::abs(dx),std::abs(dy))/(pilot_dock_target>=0?1:4);
    const auto origin=world_mode?pilot_patch_origin(s.pilot.patch):Point{};
    for (int i=1;i<=pieces;++i) {
        const Point next{start.x+dx*i/pieces,start.y+dy*i/pieces};
        if (world_mode && (origin.x*8+next.x<0 || origin.x*8+next.x>=PilotAtlasWidth ||
                           origin.y*8+next.y<0 || origin.y*8+next.y>=PilotAtlasHeight)) {
            paused=true;
            notice="已到世界海图边界";
            break;
        }
        if (!within_patch(next)) {
            if (world_mode) {
                paused=true;
                notice="已到世界海图边界";
            } else pilot_exit=pilot_to_atlas(area,next);
            break;
        }
        if (!map.navigable(next)) {
            if (pilot_dock_target<0) s.hull-=s.reefed?1:3;
            if (s.hull<=0) { rescue(); return; }
            paused=true;
            notice=pilot_dock_target>=0?"进港路线受阻，请沿水道调整":"碰到岸边，转舵后再开船";
            break;
        }
        s.pilot.x=static_cast<std::uint16_t>(next.x);
        s.pilot.y=static_cast<std::uint16_t>(next.y);
    }
    const auto world=pilot_to_world(area,{s.pilot.x,s.pilot.y});
    s.helm_x=std::clamp(world.x,0,ChartWidth*16-1);
    s.helm_y=std::clamp(world.y,0,ChartHeight*16-1);
    record_wake();
    if (s.pilot.region==WorldPilotIndex+1) {
        const int patch=pilot_patch_at({s.helm_x,s.helm_y});
        if (patch!=s.pilot.patch) {
            const auto old=pilot_patch_origin(s.pilot.patch), fresh=pilot_patch_origin(patch);
            const Point shift{(old.x-fresh.x)*8,(old.y-fresh.y)*8};
            s.pilot.x+=shift.x;
            s.pilot.y+=shift.y;
            pilot_dock_from.x+=shift.x;
            pilot_dock_from.y+=shift.y;
            s.pilot.patch=patch;
        }
        const auto here=position();
        if ((here.x!=last.x || here.y!=last.y) && sighting(here)>=0)
            notice="发现新航点，已记入泊地";
    }
    if (paused) { pilot_remainder={}; cancel_pilot_docking(); }
    s.pilot.minutes+=PilotTickMinutes;
    if (s.pilot.minutes>=60) {
        s.pilot.minutes-=60;
        s.helm_pulse=(s.helm_pulse+1)%HelmPulsesPerWatch;
        if (!s.helm_pulse && advance_watch()) consume_day(conditions());
    }
}

int Game::pilot_berth(PilotWater map,int preferred) const {
    if (!s.pilot.region || !map.bytes) return -1;
    if (s.pilot.moored) return s.pilot.moored-1;
    const auto area=pilot_area();
    const Point here{s.pilot.x,s.pilot.y};
    int berth=-1;
    const float range=map.world?4.f:2.5f;
    float best=range*range;
    if (preferred>=area.first && preferred<area.first+area.count && pilot_site_visible(preferred)) {
        const auto at=pilot_site(preferred).berth;
        if (within_patch(at) && miles_between(area,here,at)<best && clear_approach(here,at,map)) return preferred;
    }
    for (int i=area.first;i<area.first+area.count;++i) {
        if (!pilot_site_visible(i)) continue;
        const auto at=pilot_site(i).berth;
        if (!within_patch(at)) continue;
        const float distance=miles_between(area,here,at);
        if (distance<best && clear_approach(here,at,map)) { berth=i; best=distance; }
    }
    return berth;
}

PilotApproach Game::pilot_approach(int site,PilotWater map) const {
    if (!s.pilot.region || site<0 || site>=PilotSiteEnd) return {};
    const auto area=pilot_area();
    const bool local=site>=area.first && site<area.first+area.count;
    const auto location=pilot_atlas_site(site,false), berth=pilot_atlas_site(site,true);
    const auto mark=local?pilot_site(site):PilotSite{};
    const auto aim=local?(berth.x>=0?mark.berth:mark.location):
                         pilot_from_atlas(area,berth.x>=0?berth:location);
    const Point here{s.pilot.x,s.pilot.y};
    const Point delta{aim.x-here.x,aim.y-here.y};
    const int bearing=pilot_bearing(delta,pilot_cell_miles(area)/north_south_miles(area));
    const bool known=local && within_patch(mark.berth) && map.bytes;
    return {bearing,int(std::lround(std::sqrt(miles_between(area,here,aim))*10)),
            known && clear_approach(here,aim,map),known};
}

bool Game::begin_pilot_docking(PilotWater map,int preferred) {
    const int site=pilot_berth(map,preferred);
    if (site<0 || s.pilot.moored) return false;
    const Point here{s.pilot.x,s.pilot.y};
    if (miles_between(pilot_area(),here,pilot_site(site).berth)<=berth_radius*berth_radius)
        return finish_pilot_docking(site);
    pilot_dock_target=site;
    pilot_dock_from=here;
    pilot_dock_travel=0;
    pilot_remainder={};
    paused=false;
    notice="正在收帆进港，完成后停船";
    return true;
}

bool Game::finish_pilot_docking(int site) {
    const auto area=pilot_area();
    const auto berth=pilot_site(site).berth;
    s.pilot.x=static_cast<std::uint16_t>(berth.x);
    s.pilot.y=static_cast<std::uint16_t>(berth.y);
    if (site<WorldPilotSite) s.pilot_visits|=1U<<site;
    if (site>=CoastalPilotSite) s.coast_visits|=std::uint64_t{1}<<(site-CoastalPilotSite);
    paused=true;
    cancel_pilot_docking();
    if ((site>=WorldPilotSite && site<CoastalPilotSite) || (area.port>=0 && site==area.first)) {
        const int destination=site>=WorldPilotSite?site-WorldPilotSite:area.port;
        s.chart_marks|=place_bit(destination);
        s.destination=destination;
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

PilotSupplies Game::pilot_supply_offer(bool water) const {
    if (!s.pilot.moored) return {};
    const int amount=water?std::clamp(water_capacity()-s.water,0,8):
                           std::clamp(food_capacity()-s.stores,0,6);
    return {amount,water?(amount*3+1)/2:amount*3};
}

bool Game::pilot_supplies(bool water) {
    const auto offer=pilot_supply_offer(water);
    if (!offer.amount || s.gold<offer.cost) return false;
    earn(-offer.cost);
    if (water) s.water+=offer.amount;
    else s.stores+=offer.amount;
    notice=water?"淡水已装上船":"口粮已搬上船";
    return true;
}

int Game::pilot_parcel_destination() const {
    if (!s.pilot.region || s.pilot.region>WorldPilotIndex) return -1;
    const int index=s.pilot.region-1;
    if (s.pilot_parcels[index]!=PilotParcelStage::Aboard) return -1;
    return PilotRegions[index].first+PilotParcels[index].recipient;
}

bool Game::pilot_parcel_at(int site) const {
    if (!s.pilot.region || s.pilot.region>WorldPilotIndex || site<0 || s.pilot.moored!=site+1) return false;
    const int index=s.pilot.region-1;
    const auto& stage=s.pilot_parcels[index];
    const auto& parcel=PilotParcels[index];
    const int place=site-PilotRegions[index].first;
    return (place==parcel.sender && stage==PilotParcelStage::Waiting) ||
           (place==parcel.recipient && stage!=PilotParcelStage::Waiting);
}

bool Game::handle_pilot_parcel() {
    if (!pilot_parcel_at(int(s.pilot.moored)-1)) return false;
    const int index=s.pilot.region-1;
    auto& stage=s.pilot_parcels[index];
    if (stage==PilotParcelStage::Delivered) return false;
    if (stage==PilotParcelStage::Waiting) {
        stage=PilotParcelStage::Aboard;
        notice="差事已接，收件泊地记在图上";
    } else {
        stage=PilotParcelStage::Delivered;
        earn(PilotParcels[index].reward);
        s.renown=std::min(999999,s.renown+1);
        notice="货已送到，船钱已入账";
    }
    return true;
}

} // namespace tide
