#pragma once
#include "world.hpp"

namespace sky {

enum class SurfaceRole { ally_carrier, enemy_carrier, gunboat, battery };
enum class DeckPhase { free, approach, arrest, parked, launch };

struct CarrierDeck {
    static constexpr float half_width=40.f, half_length=130.f, height=14.f;
    static constexpr float runway_x=-14.f, touchdown=-81.f, launch_edge=111.f, rest=height+3.f;
    static constexpr std::array<Vec,2> parking{{{19,rest,-94},{19,rest,-59}}};
};

struct DeckOperation {
    DeckPhase phase=DeckPhase::free;
    Vec start;
    float yaw=0, pitch=0, bank=0, elapsed=0, duration=0;
};

struct SurfaceUnit {
    SurfaceRole role{};
    Vec pos;
    float yaw=0;
    int hull=0;
    float cooldown=0, flash=0, smoke=0, wreck_age=0;

    bool friendly() const { return role==SurfaceRole::ally_carrier; }
    bool carrier() const { return friendly() || role==SurfaceRole::enemy_carrier; }
    bool afloat() const { return role!=SurfaceRole::battery; }
    Vec aim() const { return pos+Vec{0,carrier() ? CarrierDeck::height*.76f : 4.1f,0}; }
    float contact(Vec from,Vec to) const;
};

struct Fleet {
    std::array<SurfaceUnit,4> units{};
    unsigned launched=0;
    bool supplied=false;
    float supply_flash=0, clear_time=0;
    DeckOperation deck;

    int remaining() const {
        return int(std::count_if(units.begin()+1,units.end(),[](const auto& unit){return unit.hull>0;}));
    }
};

struct Scene;
void draw_surface_unit(Scene& scene,const SurfaceUnit& unit,float time,int shade);

} // namespace sky
