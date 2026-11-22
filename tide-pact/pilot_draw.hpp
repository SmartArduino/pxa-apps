#pragma once

#include "chart_style.hpp"

namespace tide {

inline void draw_pilot_water(Draw& d,PilotWater water,Rect target,Point world_origin) {
    const auto r=d.clipped(target);
    constexpr std::array<unsigned,16> colors{
        0x0e4057,0x15576b,0x227986,0x419997,0xc5b37d,0xa49a63,0x82925c,0x6e8350,
        0x687749,0x7b774b,0x8a7f57,0xa18d64,0x9d8365,0xad9377,0xc1ab8e,0xd2c6ac
    };
    constexpr auto palette=[colors] {
        std::array<std::array<Color,16>,16> result{};
        for (int terrain=0;terrain<16;++terrain)
            for (int shade=0;shade<16;++shade)
                result[terrain][shade]=chart_style::tint(colors[terrain],(shade-8)*(terrain<4?1:3));
        return result;
    }();
    for (int y=r.y;y<r.bottom();++y) {
        auto pixels=*d.pixels.row(y);
        const int v=(y-target.y)*PilotHeight*4/target.h;
        const int latitude=std::clamp((world_origin.y+v/2)/16,0,ChartHeight-1);
        for (int x=r.x;x<r.right();++x) {
            const int u=(x-target.x)*PilotWidth*4/target.w;
            const Point cell{u/4,v/4};
            int terrain=water.at(cell);
            if (water.world) {
                if (terrain) {
                    const int longitude=std::clamp((world_origin.x+cell.x*2)/16,0,ChartWidth-1);
                    const auto level=Draw::relief_level(longitude,latitude);
                    terrain=5+level*10/15;
                } else if (water.at({cell.x-1,cell.y})==8 || water.at({cell.x+1,cell.y})==8 ||
                           water.at({cell.x,cell.y-1})==8 || water.at({cell.x,cell.y+1})==8) {
                    terrain=2;
                }
            }
            const int material=terrain==0?0:terrain<5?1:terrain<=9?2:3;
            pixels[x]=palette[terrain][chart_style::texture(material,u,v)];
        }
    }
}

} // namespace tide
