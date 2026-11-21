#pragma once

#include "game.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace tide {

enum TownFacility { TownMarket, TownShipyard, TownGuild, TownArchive, TownTavern, TownDock, TownFacilityCount };
inline constexpr TownFacility PortStoryDoors[]{TownShipyard,TownMarket,TownTavern,TownGuild};
static_assert(std::size(PortStoryDoors)==PortStoryCount);
inline constexpr Text TownNames[]{"市场","船坞","商会","档案室","酒馆","码头"};
inline constexpr int TownExtent=1000;
enum TownWalkTarget { TownStoryTarget=TownFacilityCount, CityGateTarget, OutsideGateTarget,
                      RiverTarget, TimberShedTarget, PostRoadTarget };
inline constexpr Point LisbonGate{934,620};
inline constexpr int OutskirtsQuay=606;
inline constexpr std::array<Point,4> OutskirtsSites{{{82,546},{495,755},{802,527},{650,535}}};

struct TownLayout {
    int port, quay;
    std::array<Point, TownFacilityCount> doors;
};

inline constexpr TownLayout Towns[]{
    {lisbon, 620, {{{244,543},{107,603},{627,501},{416,500},{852,529},{506,768}}}},
    {madeira, 610, {{{241,494},{113,587},{633,480},{422,468},{861,507},{500,769}}}},
    {canaries, 610, {{{228,523},{102,579},{626,445},{394,432},{861,526},{501,767}}}},
    {cadiz, 612, {{{225,490},{100,588},{628,476},{416,467},{848,524},{496,777}}}},
};

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
