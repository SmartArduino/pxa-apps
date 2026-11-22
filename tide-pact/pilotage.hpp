#pragma once

namespace tide {

inline constexpr int PilotWidth=256, PilotHeight=128, PilotSubcell=16;
inline constexpr int PilotMapBytes=4+PilotWidth*PilotHeight/2;
inline constexpr int PilotTickMinutes=3;
inline constexpr unsigned PilotTickMilliseconds=275;

enum PilotSiteKind { PilotCity, PilotVillage, PilotPoint };

struct PilotRegion {
    Text name, asset;
    int port, first, count;
    float west,north,east,south;
};

struct PilotSite {
    Text name, note;
    Point location, berth;
    PilotSiteKind kind;
};

#include "pilotage_data.hpp"
#include "pilot_world_data.hpp"

inline constexpr int WorldPilotIndex=std::size(PilotRegions);
inline constexpr int WorldPilotSite=32;
inline constexpr int CoastalPilotSite=WorldPilotSite+PlaceCount;
inline constexpr int PilotSiteEnd=CoastalPilotSite+std::size(PilotCoastalSites);
inline constexpr int PilotTilesX=(ChartWidth+29)/30, PilotTilesY=(ChartHeight+13)/14;
inline constexpr int PilotTileCount=PilotTilesX*PilotTilesY;
inline constexpr int PilotTileBytes=PilotWidth*PilotHeight/8;
inline constexpr int PilotAtlasUnit=128;
inline constexpr int PilotAtlasWidth=ChartWidth*PilotAtlasUnit, PilotAtlasHeight=ChartHeight*PilotAtlasUnit;
inline constexpr int PilotPatchWidth=30*PilotAtlasUnit, PilotPatchHeight=14*PilotAtlasUnit;
static_assert(std::size(PilotSites)<WorldPilotSite && std::size(PilotWorldBerths)==PlaceCount);
static_assert(std::size(PilotCoastalSites)<64 && PilotSiteEnd<=255);

enum class PilotParcelStage : std::uint8_t { Waiting, Aboard, Delivered };

struct PilotParcel {
    int sender, recipient, reward; // Settlement positions within the regional chart
    Text title, offer, delivery;
};

inline constexpr PilotParcel PilotParcels[]{
    {1,2,24,"补好的渔网","雷斯特卢的补网匠把网捆好了。她请你带去卡斯凯什，给等着出海的弟弟。",
            "渔夫接过网，摸了摸打好的结：这下赶得上明早出海。他按约给了船钱。"},
    {1,2,30,"渡船的船桨","罗塔的木匠托你捎一支新桨。圣玛丽亚港的渡船缺了它，只好在岸边等。",
            "渡船夫把桨放进桨架试了试，正合手。他把船钱数给你，回头喊乘客上船。"},
    {1,2,28,"给谷里的盐","湾里的渔户装了一篮盐鱼，想捎给里贝拉布拉瓦的亲戚。靠岸前先找准谷口。",
            "收篮子的姑娘掀开布一看，笑说今晚不用再熬一锅清汤了。她拿出预备好的船钱。"},
    {2,3,26,"外海归来的信","加尔达尔的引水人想请你把家书带去阿加埃特。信纸怕水，他用布裹了两层。",
            "老妇人认出信上的笔迹，托你下次见到写信的人，告诉他家里一切安好。"},
    {1,2,42,"海峡两岸的灯","塔里法的船工修好一盏提灯，请你带到休达。白天渡海，到了南岸再沿湾找泊地。",
            "码头看守拧了一下灯柄，提灯稳稳亮起来。他付清船钱，挂到小艇的船头。"}
};

constexpr bool valid_pilot_parcels() {
    if (std::size(PilotParcels)!=std::size(PilotRegions)) return false;
    for (int i=0;i<int(std::size(PilotParcels));++i) {
        const auto& region=PilotRegions[i];
        const auto& parcel=PilotParcels[i];
        for (int place : {parcel.sender,parcel.recipient})
            if (place<=0 || place>=region.count || PilotSites[region.first+place].berth.x<0) return false;
        if (parcel.sender==parcel.recipient) return false;
    }
    return true;
}
static_assert(valid_pilot_parcels());

struct PilotApproach {
    int bearing=0, tenths=0;
    bool clear=false, known=false;
};

struct PilotSupplies {
    int amount=0, cost=0;
};

struct PilotageState {
    std::uint16_t x=0,y=0;
    std::uint8_t region=0, heading=0, moored=0, minutes=0;
    std::uint16_t patch=0;
};

struct PilotWater {
    const unsigned char* bytes=nullptr;
    bool world=false;

    int at(Point pixel) const {
        if (!bytes || pixel.x<0 || pixel.x>=PilotWidth || pixel.y<0 || pixel.y>=PilotHeight) return 15;
        const int index=pixel.y*PilotWidth+pixel.x;
        if (world) return (bytes[index/8]&(128>>(index&7)))?8:0;
        return (bytes[4+index/2]>>(4*(index&1)))&15;
    }

    bool navigable(Point fixed) const {
        return fixed.x>=0 && fixed.y>=0 && at({fixed.x/PilotSubcell,fixed.y/PilotSubcell})<4;
    }
};

inline bool decode_pilot_tile(std::span<const unsigned char> packed,std::span<unsigned char> tile) {
    if (tile.size()!=PilotTileBytes || packed.empty()) return false;
    if (packed.size()==1) {
        std::fill(tile.begin(),tile.end(),packed.front());
        return true;
    }
    if (packed.size()==tile.size()) {
        std::copy(packed.begin(),packed.end(),tile.begin());
        return true;
    }
    if (packed.size()%2) return false;
    std::size_t offset=0;
    for (std::size_t i=0;i<packed.size();i+=2) {
        const auto count=packed[i];
        if (!count || count>tile.size()-offset) return false;
        std::fill_n(tile.begin()+offset,count,packed[i+1]);
        offset+=count;
    }
    return offset==tile.size();
}

inline constexpr Point PilotDirections[]{
    {0,-100},{38,-92},{71,-71},{92,-38},{100,0},{92,38},{71,71},{38,92},
    {0,100},{-38,92},{-71,71},{-92,38},{-100,0},{-92,-38},{-71,-71},{-38,-92}
};
inline constexpr Text PilotBearings[]{
    "北","北偏东","东北","东偏北","东","东偏南","东南","南偏东",
    "南","南偏西","西南","西偏南","西","西偏北","西北","北偏西"
};

int pilot_bearing(Point offset,float aspect=1.f);
Point pilot_from_world(const PilotRegion& region,Point fixed);
Point pilot_to_world(const PilotRegion& region,Point local);
float pilot_cell_miles(const PilotRegion& region);
Point pilot_patch_origin(int patch);
int pilot_patch_at(Point world);
Point pilot_from_atlas(const PilotRegion& region,Point atlas);
Point pilot_to_atlas(const PilotRegion& region,Point local);
int pilot_detail_at(Point atlas,int padding=0);

} // namespace tide
