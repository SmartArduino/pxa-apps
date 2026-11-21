// SPDX-License-Identifier: MIT
#pragma once
#include <array>
namespace ash {
struct Character {
    const char* id;const char* name;const char* roman;const char* style;const char* motive;
    std::array<const char*,4> arts;
    float walk,back,jump,air,projectile,rush;
    unsigned effect;
};
inline constexpr std::array<Character,4> roster={{
    {"yan","陆炎","YAN","迅掌连段","为一封旧信重返雨港",{"烬流掌","破空拳","踏潮进","燎原潮"},1.25f,.9f,5.25f,1.35f,2.9f,2.f,0},
    {"lan","霁岚","LAN","长脚控场","为守住港灯站上夜擂",{"潮刃掌","鹤升拳","断浪步","月满潮"},1.1f,.9f,5.25f,1.35f,2.5f,2.f,1},
    {"zhen","韩震","ZHEN","重拳投技","替码头工友讨回公道",{"震潮掌","撼山拳","铁肩撞","崩港震"},.94f,.72f,4.85f,1.1f,2.15f,1.8f,0},
    {"ling","唐绫","LING","疾行腿法","循失踪师姐留下的雷痕",{"惊雷掌","掠云拳","逐电步","紫电破"},1.46f,1.05f,5.65f,1.6f,3.2f,2.5f,1}
}};
inline constexpr int characterCount=roster.size();
// Palm pixels in the original 96x128, right-facing release image (frame 25).
// These are source-image coordinates, before silhouette cropping and mirroring.
inline constexpr int fighterHalf=48,fighterBaseline=120,fireReleaseFrame=25;
struct PalmPixel {int x,y;};
inline constexpr std::array<PalmPixel,characterCount> firePalms={{{86,52},{82,52},{86,53},{82,52}}};
inline int tourOpponent(int player,int first,int stage){
    int candidate=first==player?(player+1)%characterCount:first;
    for(int i=0;i<stage;++i){candidate=(candidate+1)%characterCount;if(candidate==player)candidate=(candidate+1)%characterCount;}
    return candidate;
}
}
