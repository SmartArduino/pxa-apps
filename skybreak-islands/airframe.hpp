#pragma once
#include "world.hpp"

namespace sky {

enum class Airframe : unsigned { patrol, dart, maritime, count };

struct FlightTuning {
    float cruise,boost,turn,pitch,roll;
};

struct AirframeSpec {
    const char* code;
    const char* role;
    FlightTuning flight;
    float nose,tail,body,span,sweep,tip_chord,rear,fin_height;
    float nacelle,nozzle;
    bool twin_tail;
    Color top,edge,accent,glass;
};

inline constexpr AirframeSpec airframes[]{
    {"F-01","巡航", {108,184,1.31f,1.f,1.f},
     13.8f,-10,3.3f,13,-3.8f,3.6f,-6.9f,5.5f,2.5f,1.30f,true,
     rgb(0xf0dfba),rgb(0x637682),rgb(0xe4993c),rgb(0x23596c)},
    {"F-02","加速", {122,210,1.53f,1.13f,1.24f},
     17.5f,-12.7f,2.55f,10.9f,-7.1f,2.9f,-9.8f,6.4f,0,1.63f,false,
     rgb(0xa9c7e2),rgb(0x465e7b),rgb(0xe0e9e6),rgb(0x255a73)},
    {"F-03","低空", {96,162,1.10f,.86f,.80f},
     12.6f,-10.5f,3.9f,16.3f,-1.0f,4.8f,-7.6f,4.8f,3.65f,1.50f,true,
     rgb(0x9eae7c),rgb(0x4e6255),rgb(0xe4b864),rgb(0x246465)},
};

constexpr const AirframeSpec& airframe(Airframe key) { return airframes[unsigned(key)]; }

struct EnemyProfile {
    const char* code;
    float speed,turn,altitude,spacing,range,reload;
    int hull;
    Color top,edge,accent,glass;
};

inline constexpr EnemyProfile enemy_profiles[]{
    {"E1", 73, .94f, -22, 185, 375, 1.70f, 2,
     rgb(0xb87869),rgb(0x623e3e),rgb(0xfac074),rgb(0xc3ded3)},
    {"E2", 96, 1.34f, 35, 265, 475, 1.06f, 2,
     rgb(0xaf8dc6),rgb(0x514367),rgb(0xff688f),rgb(0xe9bed3)},
    {"E3", 65, .65f, 62, 355, 535, 2.40f, 3,
     rgb(0x885b55),rgb(0x47393e),rgb(0xe9a54b),rgb(0xf9ce8c)},
};

constexpr const EnemyProfile& enemy_profile(Airframe key) { return enemy_profiles[unsigned(key)]; }

} // namespace sky
