#pragma once

namespace tide {

inline constexpr int PilotWidth=256, PilotHeight=128, PilotSubcell=16;
inline constexpr int PilotMapBytes=4+PilotWidth*PilotHeight/2;
inline constexpr int PilotTickMinutes=6;

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

struct PilotageState {
    std::uint16_t x=0,y=0;
    std::uint8_t region=0, heading=0, moored=0, minutes=0;
};

struct PilotWater {
    const unsigned char* bytes=nullptr;

    int at(Point pixel) const {
        if (!bytes || pixel.x<0 || pixel.x>=PilotWidth || pixel.y<0 || pixel.y>=PilotHeight) return 15;
        const int index=pixel.y*PilotWidth+pixel.x;
        return (bytes[4+index/2]>>(4*(index&1)))&15;
    }

    bool navigable(Point fixed) const {
        return fixed.x>=0 && fixed.y>=0 && at({fixed.x/PilotSubcell,fixed.y/PilotSubcell})<4;
    }
};

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

} // namespace tide
