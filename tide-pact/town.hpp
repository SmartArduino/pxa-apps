#pragma once

#include "game.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace tide {

enum TownFacility { TownMarket, TownShipyard, TownGuild, TownArchive, TownTavern, TownDock, TownFacilityCount };
inline constexpr TownFacility PortStoryDoors[]{TownShipyard,TownMarket,TownTavern,TownGuild,
                                              TownMarket,TownGuild,TownGuild,TownShipyard};
static_assert(std::size(PortStoryDoors)==PortStoryCount);
inline constexpr Text TownNames[]{"市场","船坞","商会","档案室","酒馆","码头"};
inline constexpr int TownExtent=1000;
enum TownWalkTarget { TownStoryTarget=TownFacilityCount, CityGateTarget, OutsideGateTarget,
                      RiverTarget, TimberShedTarget, PostRoadTarget, RegionalTrailTarget };
inline constexpr Point LisbonGate{934,620};
inline constexpr int OutskirtsQuay=606;
inline constexpr std::array<Point,4> OutskirtsSites{{{82,546},{495,755},{802,527},{650,535}}};

struct TownLayout {
    int port, quay;
    std::array<Point, TownFacilityCount> doors;
    Point field_gate{};
};

inline constexpr TownLayout Towns[]{
    {lisbon, 620, {{{244,543},{107,603},{627,501},{416,500},{852,529},{506,768}}}},
    {madeira, 610, {{{241,494},{113,587},{633,480},{422,468},{861,507},{500,769}}}, {920,610}},
    {canaries, 610, {{{228,523},{102,579},{626,445},{394,432},{861,526},{501,767}}}},
    {cadiz, 612, {{{225,490},{100,588},{628,476},{416,467},{848,524},{496,777}}}, {916,612}},
    {calicut, 594, {{{254,552},{106,552},{619,552},{405,552},{861,552},{507,735}}}, {922,594}},
    {venice, 601, {{{219,550},{855,609},{700,486},{470,516},{105,490},{476,774}}}},
    {kilwa, 600, {{{612,549},{879,598},{437,464},{322,520},{170,514},{491,782}}}},
    {ragusa, 630, {{{300,597},{128,626},{554,596},{662,582},{812,594},{540,796}}}},
    {candia, 622, {{{111,588},{852,602},{520,580},{635,577},{331,586},{527,771}}}},
    {diu, 602, {{{112,555},{679,604},{323,551},{568,562},{890,550},{490,761}}}},
    {gresik, 612, {{{324,562},{112,613},{653,574},{745,569},{827,583},{483,792}}}},
    {quanzhou, 551, {{{197,516},{906,545},{606,509},{670,519},{373,510},{511,775}}}},
    {hakata, 613, {{{253,569},{897,606},{589,571},{804,572},{101,574},{498,815}}}},
    {kochi, 590, {{{256,565},{926,587},{793,560},{650,565},{437,565},{501,796}}}},
    {busanpo, 598, {{{193,552},{898,580},{363,551},{602,550},{741,543},{500,797}}}},
    {malacca, 612, {{{556,569},{863,557},{307,569},{421,568},{112,586},{486,775}}}},
    {naha, 577, {{{130,553},{861,550},{631,532},{735,531},{353,538},{483,785}}}},
    {ningbo, 597, {{{155,571},{348,553},{628,551},{779,566},{927,557},{502,769}}}},
    {sakai, 602, {{{344,555},{136,563},{645,552},{739,551},{881,552},{508,767}}}},
};

struct LocalVessel {
    int port;
    Text name, note;
};

inline constexpr LocalVessel LocalVessels[]{
    {quanzhou,"中式硬帆船","帆上横着一根根帆桁。收帆时，桁条带着帆面逐层落下。泉州的近海货船把瓷器和日用货装进船舱，也靠小艇接驳。"},
    {hakata,"博多沿岸商船","横桁支着一幅宽方帆，木舱可遮雨。博多船工把待交的货捆好，沿九州岸近行，借顺风渡往对马。"},
    {kochi,"马拉巴尔三角帆船","斜桁挑起三角帆，船员可依风向调帆。装胡椒的大船多泊在口外，细长的小船把货从回水河道运来。"},
    {busanpo,"朝鲜沿岸货船","宽阔的木船可载鱼筐和布包，近岸往返用短桅方帆。进釜山浦后，船夫等岸上的书吏办完查验才卸货。"},
};
inline constexpr int PortBoatWidth=96, PortBoatHeight=72;
inline int local_vessel(int port) {
    for (int i=0;i<int(std::size(LocalVessels));++i)
        if (LocalVessels[i].port==port) return i;
    return -1;
}

inline int town_index(int port) {
    for (int i=0;i<int(std::size(Towns));++i)
        if (Towns[i].port==port) return i;
    return -1;
}

inline bool same_town_point(Point a, Point b) { return a.x==b.x && a.y==b.y; }

inline std::optional<Point> town_street(int quay, std::span<const Point> doors, Point p) {
    Point chosen{};
    int distance=65*65;
    const auto consider=[&](Point road) {
        const int dx=p.x-road.x, dy=p.y-road.y;
        const int score=dx*dx+dy*dy;
        if (score<distance) { distance=score; chosen=road; }
    };
    consider({std::clamp(p.x,65,935),quay});
    for (const auto door : doors)
        consider({door.x,std::clamp(p.y,std::min(door.y,quay),std::max(door.y,quay))});
    if (distance==65*65) return std::nullopt;
    return chosen;
}
inline std::optional<Point> town_street(const TownLayout& town, Point p) {
    return town_street(town.quay,town.doors,p);
}

struct TownWalk {
    Point at{};
    std::array<Point,3> steps{};
    unsigned started=0;
    int leg=0, count=0, facing=0, entrance=-1;

    bool moving() const { return leg<count; }
    void cancel() { leg=count=0; entrance=-1; }

    void go(Point target, int quay, int open, unsigned now) {
        cancel();
        entrance=open;
        started=now;
        const auto add=[&](Point p) {
            if (!same_town_point(count?steps[count-1]:at,p)) steps[count++]=p;
        };
        if (at.x!=target.x) {
            add({at.x,quay});
            add({target.x,quay});
        }
        add(target);
    }

    int update(unsigned now) {
        while (moving()) {
            const Point target=steps[leg];
            const int dx=target.x-at.x, dy=target.y-at.y;
            const unsigned duration=std::max(1,(std::abs(dx)*13+std::abs(dy)*6)/10);
            facing=dx?dx>0?2:3:dy>0?0:1;
            if (now-started<duration) return -1;
            at=target;
            ++leg;
            started+=duration;
        }
        const int result=entrance;
        entrance=-1;
        return result;
    }

    Point position(unsigned now) const {
        if (!moving()) return at;
        const auto p=steps[leg];
        const int dx=p.x-at.x, dy=p.y-at.y;
        const unsigned duration=std::max(1,(std::abs(dx)*13+std::abs(dy)*6)/10);
        const int elapsed=std::min(now-started,duration);
        return {at.x+dx*elapsed/int(duration),at.y+dy*elapsed/int(duration)};
    }
};

} // namespace tide
