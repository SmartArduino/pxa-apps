#include "game.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace tide;

namespace {

void check_shoreline(PilotWater water, Point berth) {
    assert(water.navigable(berth));
    bool shore=false;
    for (const Point step : {Point{-PilotSubcell,0},Point{PilotSubcell,0},
                             Point{0,-PilotSubcell},Point{0,PilotSubcell}}) {
        const Point near{berth.x+step.x,berth.y+step.y};
        if (near.x>=0 && near.x<PilotWidth*PilotSubcell && near.y>=0 && near.y<PilotHeight*PilotSubcell)
            shore |= !water.navigable(near);
    }
    assert(shore);
}

struct WorldAtlas {
    std::ifstream file{"assets/sea/pilot-world.bin",std::ios::binary};
    std::vector<unsigned char> data{std::istreambuf_iterator<char>(file),{}};
    std::array<unsigned char,PilotTileBytes> pixels{};

    PilotWater read(int patch) {
        const auto offset=[this](int index) {
            const auto* b=data.data()+4*index;
            return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|
                   (std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
        };
        assert(!data.empty() && patch>=0 && patch<PilotTileCount);
        const auto start=offset(patch), end=offset(patch+1);
        assert(start<end && end<=data.size());
        assert(decode_pilot_tile(std::span(data).subspan(start,end-start),pixels));
        return {pixels.data(),true};
    }
};

template<std::size_t Bytes>
void check_legacy(const State& state,std::uint8_t version) {
    auto source=state;
    if (source.pilot.region==WorldPilotIndex+1) {
        source.pilot.region=LegacyPilotRegionCount+1;
        constexpr int previous_columns=(PreviousChartWidth+29)/30;
        source.pilot.patch=(source.pilot.patch/PilotTilesX)*previous_columns+source.pilot.patch%PilotTilesX;
        if (source.pilot.moored>CoastalPilotSite) source.pilot.moored-=PlaceCount-PreviousPlaceCount;
    }
    const auto current=encode(source);
    std::array<std::byte,Bytes> old{};
    std::copy_n(current.begin(),old.size()-4,old.begin());
    old[3]=std::byte{version};
    std::uint32_t checksum=2166136261U;
    for (std::size_t i=0;i<old.size()-4;++i)
        checksum=(checksum^std::to_integer<unsigned>(old[i]))*16777619U;
    for (int i=0;i<4;++i) old[old.size()-4+i]=std::byte(checksum>>(i*8));
    State loaded;
    assert(decode(loaded,old) && loaded.pilot.region==state.pilot.region &&
           loaded.pilot.moored==state.pilot.moored && loaded.pilot_visits==state.pilot_visits &&
           loaded.pilot.patch==state.pilot.patch && !loaded.coast_visits);
    for (const auto stage : loaded.pilot_parcels) assert(stage==PilotParcelStage::Waiting);
    for (int i=0;i<WorldPilotIndex;++i)
        for (int y=0;y<PilotSurveyRows;++y)
            assert(loaded.pilot_chart[i][y]==
                   (Bytes==FirstSurveySaveBytes && i<LegacyPilotRegionCount?state.pilot_chart[i][y]:0));
}

} // namespace

int main() {
    const auto ocean=[](Point) { return true; };
    for (int index=0;index<int(std::size(PilotRegions));++index) {
        const auto& area=PilotRegions[index];
        std::array<unsigned char,PilotMapBytes> bytes{};
        auto file=std::ifstream(std::string(area.asset),std::ios::binary);
        assert(file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()));
        assert(file.peek()==EOF && bytes[0]=='P' && bytes[1]=='C' && bytes[2]=='H' && bytes[3]=='1');
        const PilotWater water{bytes.data(),false,area.roadstead};
        for (int site=area.first;site<area.first+area.count;++site) {
            const auto& mark=PilotSites[site];
            if (mark.berth.x>=0 && !area.roadstead) check_shoreline(water,mark.berth);
        }
        Game ship;
        ship.s.origin=area.port;
        const auto main_berth=PilotSites[area.first].berth;
        const auto unexplored=Point{main_berth.x<2048?4095:0,main_berth.y<1024?2047:0};
        assert(!ship.pilot_charted(index,main_berth));
        assert(!ship.pilot_site_visible(area.first+1));
        assert(ship.start_helm(-1));
        assert(ship.pilot_region()==index && ship.begin_pilotage(index,water));
        assert(ship.pilot_charted(index,main_berth) && !ship.pilot_charted(index,unexplored));
        assert(!ship.reveal_pilot_chart());
        assert(ship.paused && ship.pilot_berth(water)==area.first && !ship.dock_helm(ocean));
        if (area.port==lisbon) {
            const Point restelo=PilotSites[area.first+1].berth;
            assert(restelo.y<952 && water.at({restelo.x/PilotSubcell,restelo.y/PilotSubcell})==3);
            Game flood=ship;
            flood.s.pilot.x=2350;
            flood.s.pilot.y=958;
            flood.s.helm_pulse=2;
            flood.set_pilot_heading(4);
            flood.paused=false;
            assert(water.navigable({flood.s.pilot.x,flood.s.pilot.y}));
            auto ebb=flood;
            ebb.s.sail_watch=1;
            const auto incoming=flood.pilot_current(), outgoing=ebb.pilot_current();
            assert(incoming.in_channel && incoming.flood && incoming.tenths>0);
            assert(outgoing.in_channel && !outgoing.flood && outgoing.tenths>incoming.tenths &&
                   outgoing.bearing==((incoming.bearing+8)&15));
            flood.pilot_tick(water);
            ebb.pilot_tick(water);
            assert(!flood.paused && !ebb.paused && flood.s.pilot.x>ebb.s.pilot.x);
            flood.s.pilot.x=880;
            assert(!flood.pilot_current().in_channel);
        }
        const int heading=ship.s.pilot.heading, x=ship.s.pilot.x, y=ship.s.pilot.y;
        ship.set_pilot_heading(heading+1);
        assert(ship.s.pilot.heading==(heading+1)%16);
        const auto advanced=ship;
        State loaded;
        assert(decode(loaded,encode(advanced.s)) && loaded.pilot.x==x &&
               loaded.pilot.y==y && loaded.pilot.region==index+1 && loaded.pilot_chart==ship.s.pilot_chart);
        if (index<LegacyPilotRegionCount) {
            check_legacy<PilotSaveBytes>(advanced.s,0x4a);
            check_legacy<CargoSaveBytes>(advanced.s,0x4f);
            check_legacy<FirstSurveySaveBytes>(advanced.s,0x50);
        }
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
        if (area.roadstead) {
            const auto fort=PilotSites[area.first].location, lagoon=PilotSites[area.first+1].location;
            assert(water.at({fort.x/PilotSubcell,fort.y/PilotSubcell})>=4);
            assert(water.at({lagoon.x/PilotSubcell,lagoon.y/PilotSubcell})==3);
            assert(!water.navigable(lagoon) && water.navigable(main_berth));
            assert(ship.pilot_site_visible(area.first+1) && ship.pilot_parcel_destination()<0);
            const auto water_point=ship.pilot_atlas_site(area.first,true);
            const auto general=ship.pilot_atlas_site(WorldPilotSite+elmina,true);
            assert(water_point.x==general.x && water_point.y==general.y);
            assert(ship.begin_pilot_docking(water,area.first) && ship.s.origin==elmina && !ship.sea());
            continue;
        }
        Game harbour=ship;
        const auto& parcel=PilotParcels[index];
        const int sender=area.first+parcel.sender, recipient=area.first+parcel.recipient;
        const auto village=PilotSites[sender].berth;
        assert(water.navigable(village) && !harbour.handle_pilot_parcel());
        bool has_channel=false;
        for (int dy=-72;dy<=72 && !has_channel;dy+=6)
            for (int dx=-72;dx<=72 && !has_channel;dx+=6) {
                const Point at{village.x+dx,village.y+dy};
                if (!water.navigable(at)) continue;
                harbour.s.pilot.x=at.x;
                harbour.s.pilot.y=at.y;
                const auto approach=harbour.pilot_approach(sender,water);
                has_channel=approach.clear && approach.tenths>=6 && approach.tenths<=22;
            }
        harbour.reveal_pilot_chart();
        assert(has_channel && harbour.begin_pilot_docking(water,sender) && harbour.pilot_docking()==sender);
        const int hull=harbour.s.hull;
        for (int tick=0;tick<100 && !harbour.paused;++tick) harbour.pilot_tick(water);
        assert(harbour.s.pilot.moored==sender+1 && harbour.paused && harbour.s.hull==hull);
        assert(harbour.pilot_slow_water(water));
        const int gold=harbour.s.gold;
        harbour.s.water=12;
        harbour.s.stores=12;
        assert(harbour.pilot_supplies(true) && harbour.s.water==20 && harbour.s.gold==gold-12);
        harbour.s.water=harbour.water_capacity()-1;
        assert(harbour.pilot_supply_offer(true).cost==2 && harbour.pilot_supplies(true));
        assert(harbour.pilot_parcel_at(sender) && harbour.handle_pilot_parcel());
        assert(harbour.pilot_parcel_destination()==recipient && !harbour.handle_pilot_parcel());
        assert(decode(loaded,encode(harbour.s)) && loaded.pilot.moored==sender+1 &&
               loaded.pilot_parcels[index]==PilotParcelStage::Aboard);
        harbour.cast_off_pilotage();
        assert(!harbour.handle_pilot_parcel());
        const auto dock=PilotSites[recipient].berth;
        harbour.s.pilot.x=dock.x;
        harbour.s.pilot.y=dock.y;
        assert(harbour.begin_pilot_docking(water,recipient) && harbour.s.pilot.moored==recipient+1);
        const int before=harbour.s.gold;
        assert(harbour.handle_pilot_parcel() && harbour.s.gold==before+parcel.reward);
        assert(!harbour.handle_pilot_parcel() && harbour.pilot_parcel_destination()<0);
        harbour.cast_off_pilotage();
        assert(!harbour.pilot_supplies(true) && harbour.leave_pilotage(ocean) && !harbour.s.pilot.region);

        assert(ship.begin_pilot_docking(water,area.first) && ship.s.origin==area.port && !ship.sea());
        assert(ship.s.pilot_visits&(1U<<area.first));
    }
    WorldAtlas atlas;
    for (int i=0;i<PilotTileCount;++i) atlas.read(i);
    const auto check_world_berth=[&](Point at) {
        const int patch=pilot_patch_at({at.x/8,at.y/8});
        const auto origin=pilot_patch_origin(patch);
        check_shoreline(atlas.read(patch),{at.x-origin.x*8,at.y-origin.y*8});
    };
    for (int index=0;index<PortCount;++index) {
        const int port=port_place(index);
        Game ship;
        if (port!=elmina) check_world_berth(ship.pilot_atlas_site(port+WorldPilotSite,true));
        ship.s.origin=port;
        assert(ship.start_helm(-1));
        if (ship.pilot_region()!=WorldPilotIndex) continue;
        const auto water=atlas.read(ship.starting_pilot_patch());
        assert(ship.begin_pilotage(WorldPilotIndex,water));
        const auto berth=ship.pilot_site(port+WorldPilotSite).berth;
        assert(water.navigable(berth) && ship.pilot_site_visible(port+WorldPilotSite));
        State loaded;
        assert(decode(loaded,encode(ship.s)) && loaded.pilot.patch==ship.s.pilot.patch);
        if (port==bristol) {
            State old=ship.s;
            old.pilot_chart[0][0]=0x10;
            check_legacy<FirstSurveySaveBytes>(old,0x50);
        }
        assert(ship.begin_pilot_docking(water,port+WorldPilotSite) && ship.s.origin==port && !ship.sea());
    }
    Game crossing;
    assert(crossing.start_helm(-1));
    crossing.s.helm_x=30*24*16-2;
    crossing.s.helm_y=14*28*16+55;
    auto water=atlas.read(crossing.starting_pilot_patch());
    assert(crossing.begin_pilotage(WorldPilotIndex,water));
    const int first=crossing.s.pilot.patch;
    crossing.set_pilot_heading(4);
    crossing.paused=false;
    for (int i=0;i<60 && crossing.s.pilot.patch==first && !crossing.paused;++i)
        crossing.pilot_tick(water);
    assert(crossing.s.pilot.patch==first+1 && !crossing.paused);
    water=atlas.read(crossing.s.pilot.patch);
    assert(water.navigable({crossing.s.pilot.x,crossing.s.pilot.y}));
    check_legacy<WorldPilotSaveBytes>(crossing.s,0x4b);
    check_legacy<CoastalSaveBytes>(crossing.s,0x4c);

    const auto& tagus=PilotRegions[0];
    std::array<unsigned char,PilotMapBytes> tagus_bytes{};
    assert(std::ifstream(std::string(tagus.asset),std::ios::binary)
           .read(reinterpret_cast<char*>(tagus_bytes.data()),tagus_bytes.size()));
    const PilotWater tagus_water{tagus_bytes.data()};
    Game downstream;
    assert(downstream.start_helm(-1) && downstream.begin_pilotage(0,tagus_water));
    downstream.s.pilot.x=24;
    downstream.s.pilot.y=1120;
    downstream.set_pilot_heading(12);
    downstream.paused=false;
    for (int i=0;i<8 && !downstream.pilot_exiting();++i) downstream.pilot_tick(tagus_water);
    assert(downstream.pilot_exiting() && !downstream.paused);
    const auto edge=downstream.pilot_exit_position();
    const auto sea=atlas.read(pilot_patch_at({edge.x/8,edge.y/8}));
    assert(downstream.connect_pilotage(sea) && !downstream.paused && downstream.s.pilot.heading==12);
    assert(downstream.pilot_atlas_position().x==edge.x && downstream.pilot_atlas_position().y==edge.y);
    downstream.set_pilot_heading(4);
    for (int i=0;i<40 && downstream.s.pilot.region>WorldPilotIndex;++i) {
        const auto patch=atlas.read(downstream.s.pilot.patch);
        downstream.pilot_tick(patch);
        downstream.refine_pilotage(0,tagus_water);
    }
    assert(downstream.s.pilot.region==1 && !downstream.paused && downstream.s.pilot.heading==4);

    for (int id=CoastalPilotSite;id<PilotSiteEnd;++id) {
        const auto& site=PilotCoastalSites[id-CoastalPilotSite];
        if (site.berth.x<0) continue;
        check_world_berth(site.berth);
        Game visitor;
        assert(visitor.start_helm(-1));
        visitor.s.helm_x=site.berth.x/8;
        visitor.s.helm_y=site.berth.y/8;
        const auto shore=atlas.read(visitor.starting_pilot_patch());
        assert(visitor.begin_pilotage(WorldPilotIndex,shore));
        assert(visitor.pilot_site_visible(id) && visitor.begin_pilot_docking(shore,id));
        assert(visitor.s.pilot.moored==id+1 && visitor.pilot_site_visited(id));
        State loaded;
        assert(decode(loaded,encode(visitor.s)) && loaded.pilot.moored==id+1 &&
               loaded.coast_visits==visitor.s.coast_visits);
        // The previous world save used the same map geometry and fewer named places.
        std::array<std::byte,FieldSaveBytes> previous{};
        auto source=visitor.s;
        source.pilot.moored-=PlaceCount-FirstNewPort;
        source.chart_marks&=place_bit(FirstNewPort)-1;
        const auto current=encode(source);
        std::copy_n(current.begin(),previous.size()-4,previous.begin());
        previous[3]=std::byte{0x52};
        std::uint32_t sum=2166136261U;
        for (std::size_t i=0;i<previous.size()-4;++i)
            sum=(sum^std::to_integer<unsigned>(previous[i]))*16777619U;
        for (int i=0;i<4;++i) previous[previous.size()-4+i]=std::byte(sum>>(i*8));
        assert(decode(loaded,previous) && loaded.pilot.moored==id+1 &&
               loaded.coast_visits==visitor.s.coast_visits && loaded.stock_delta==visitor.s.stock_delta);
        Game restored;
        restored.s=loaded;
        restored.s.pilot.x^=1;
        restored.restore();
        const auto anchored=restored.pilot_atlas_position();
        assert(anchored.x==site.berth.x && anchored.y==site.berth.y && restored.paused);
        const int next=visitor.pilot_coastal_hint();
        if (next>=0) {
            assert(visitor.record_pilot_coast(next) && visitor.pilot_site_visible(next));
            assert(!visitor.record_pilot_coast(next));
        }
    }

    Game automatic;
    assert(automatic.depart(madeira));
    const auto from_route=automatic.position();
    assert(automatic.take_over_helm() && automatic.paused && automatic.s.destination==madeira);
    assert(from_route.x==automatic.position().x && from_route.y==automatic.position().y);
    std::puts("Continuous regional transitions, coastal harbors, chart edges, errands and old saves passed.");
}
