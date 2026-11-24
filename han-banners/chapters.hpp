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

struct RegionalGoal {
    const char* name;
    int cities[3];
    int minOrder;
    int gold, grain;
    Gear gear;
};
inline constexpr RegionalGoal regionalGoals[] = {
    {"关洛通道",{44,0,1},0,220,170,armor},
    {"蜀道据点",{3,30,47},0,200,220,saddle},
    {"江汉锁钥",{7,10,43},65,250,260,manual},
    {"淮泗攻守",{45,22,24},0,240,170,sword},
    {"江东门户",{8,19,20},75,360,210,armor},
    {"闽江通商",{20,39,38},75,300,240,manual},
    {"西江粮道",{34,35,29},75,225,440,saddle},
    {"北部湾水路",{36,40,41},65,290,390,sword},
    {"汉水峡道",{3,shangyong,zigui},65,220,330,manual},
    {"濡须江防",{wancheng,ruxu,8},65,260,280,scaleArmor},
    {"金牛道",{47,fucheng,14},65,280,260,armor},
};
constexpr int regionalGoalCount=int(std::size(regionalGoals));
static_assert(regionalGoalCount<=16);

struct OpeningChange {
    int city,owner,troops,fort;
};
inline constexpr OpeningChange yizhouOpening[] = {
    {32,0,475,2},{7,0,1450,2},{10,2,925,2},{15,0,890,2},{16,0,650,1},{17,0,550,1},{18,0,560,1},
    {30,0,1850,2},{12,0,580,2},{2,4,1340,3},{13,4,780,2},{14,4,720,2},
    {0,1,950,3},{27,1,710,2},{23,1,1180,3},{8,2,1510,3},{53,0,525,2},
    {fucheng,0,520,2},{zigui,0,495,2},{qishan,1,490,2},
};
inline constexpr OpeningChange xiangfanOpening[] = {
    {32,0,530,2},{47,0,610,2},{2,0,1575,3},{3,0,1880,3},{13,0,1080,2},{14,0,760,2},{30,0,790,2},
    {7,0,1610,3},{15,2,780,2},{16,0,660,2},{17,0,770,2},{18,2,730,2},{10,2,880,2},
    {6,1,1490,3},{11,1,980,2},{0,1,1060,3},{27,1,810,2},{8,2,1710,3},{9,2,1170,2},
    {12,0,660,2},{52,0,660,2},{53,0,475,2},{55,0,710,3},
    {shangyong,0,540,2},{fucheng,0,560,2},{zigui,0,530,2},{wancheng,2,600,2},{qishan,1,510,2},
};

constexpr const OpeningChange* opening(int chapter,int city) {
    if (chapter==1) for (auto& change:yizhouOpening) if (change.city==city) return &change;
    if (chapter==2) for (auto& change:xiangfanOpening) if (change.city==city) return &change;
    return nullptr;
}

}
