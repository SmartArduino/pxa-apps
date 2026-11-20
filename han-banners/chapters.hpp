#pragma once
#include "world.hpp"

namespace han {

struct Objective {
    int a,b;
    const char* name;
    Gear reward;
};
struct Chapter {
    const char* name;
    const char* moment;
    const char* description;
    int year, season;
    int base[3];
    Objective objectives[3];
};
inline constexpr Chapter chapters[] = {
    {"赤壁前夕","建安十三年 秋","曹军南下，江汉诸军临江相望。",208,2,{10,4,9},
        {{7,15,"荆南立足",manual},{10,9,"扼守江汉",sword},{10,7,"长江屏障",armor}}},
    {"益州攻守","建安十九年 春","刘备入蜀，江东与曹军争合肥。",214,0,{30,4,8},
        {{2,13,"经略巴蜀",armor},{23,8,"饮马下江",saddle},{23,22,"进取淮南",sword}}},
    {"襄樊相持","建安二十四年 秋","汉中易手，关羽北上，江陵空虚。",219,2,{2,4,8},
        {{6,11,"北向宛洛",manual},{7,10,"荆州反攻",sword},{7,17,"江汉西进",armor}}},
};
constexpr int chapterCount=int(std::size(chapters));

struct OpeningChange {
    int city,owner,troops,fort;
};
inline constexpr OpeningChange yizhouOpening[] = {
    {32,0,475,2},{7,0,1450,2},{10,2,925,2},{15,0,890,2},{16,0,650,1},{17,0,550,1},{18,0,560,1},
    {30,0,1850,2},{2,4,1340,3},{13,4,780,2},{14,4,720,2},
    {0,1,950,3},{27,1,710,2},{23,1,1180,3},{8,2,1510,3},
};
inline constexpr OpeningChange xiangfanOpening[] = {
    {32,0,530,2},{47,0,610,2},{2,0,1575,3},{3,0,1880,3},{13,0,1080,2},{14,0,760,2},{30,0,790,2},
    {7,0,1610,3},{15,2,780,2},{16,0,660,2},{17,0,770,2},{18,2,730,2},{10,2,880,2},
    {6,1,1490,3},{11,1,980,2},{0,1,1060,3},{27,1,810,2},{8,2,1710,3},{9,2,1170,2},
};

constexpr const OpeningChange* opening(int chapter,int city) {
    if (chapter==1) for (auto& change:yizhouOpening) if (change.city==city) return &change;
    if (chapter==2) for (auto& change:xiangfanOpening) if (change.city==city) return &change;
    return nullptr;
}

}
