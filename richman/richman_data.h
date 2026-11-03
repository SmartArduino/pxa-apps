#pragma once

// 原创棋盘数据（致敬大富翁类玩法，地名、数值均为原创，未使用任何原版素材）
namespace rich {

enum TileType {
    T_START = 0,   // 起点：经过/停留 +2000
    T_PROP = 1,    // 地产：可购买、可升级
    T_CHANCE = 2,  // 机会卡
    T_FATE = 3,    // 命运卡
    T_JAIL = 4,    // 监狱：交1000或停2轮
    T_LOTTERY = 5, // 彩票：花500，50%得5000
};

struct Tile {
    const char* name;
    int type;
    int price; // 仅地产有效
};

inline constexpr Tile kTiles[30] = {
    {"起点",   T_START,   0},
    {"桃花村", T_PROP,    800},
    {"机会",   T_CHANCE,  0},
    {"青石镇", T_PROP,    1000},
    {"命运",   T_FATE,    0},
    {"翠竹林", T_PROP,    1200},
    {"监狱",   T_JAIL,    0},
    {"荷花池", T_PROP,    1500},
    {"机会",   T_CHANCE,  0},
    {"白马寺", T_PROP,    1800},
    {"命运",   T_FATE,    0},
    {"望江楼", T_PROP,    2000},
    {"彩票",   T_LOTTERY, 0},
    {"紫金山", T_PROP,    2400},
    {"机会",   T_CHANCE,  0},
    {"观星台", T_PROP,    2800},
    {"命运",   T_FATE,    0},
    {"龙门客栈", T_PROP,  3200},
    {"监狱",   T_JAIL,    0},
    {"碧水湖", T_PROP,    3600},
    {"机会",   T_CHANCE,  0},
    {"凤凰城", T_PROP,    4000},
    {"命运",   T_FATE,    0},
    {"天机阁", T_PROP,    4500},
    {"彩票",   T_LOTTERY, 0},
    {"云梦泽", T_PROP,    5000},
    {"机会",   T_CHANCE,  0},
    {"长安街", T_PROP,    5500},
    {"命运",   T_FATE,    0},
    {"杏花村", T_PROP,    6000},
};

inline constexpr const char* kLevelName[4] = {"空地", "小屋", "洋房", "大厦"};

// 卡片效果：0 金钱(+/-), 1 前进N格(不触发), 2 后退N格(不触发),
//           3 进监狱, 4 其他每位玩家给你N, 5 按百分比扣钱
struct Card {
    const char* text;
    int kind;
    int amount;
};

inline constexpr Card kChance[8] = {
    {"银行分红，+1500",      0, 1500},
    {"彩票中奖，+2500",      0, 2500},
    {"好运连连，前进3格",    1, 3},
    {"生日快乐，每人给你500", 4, 500},
    {"投资成功，+2000",      0, 2000},
    {"贵人相助，+1200",      0, 1200},
    {"免费体检，+1000",      0, 1000},
    {"做慈善，-800",         0, -800},
};

inline constexpr Card kFate[8] = {
    {"违章罚款，-1200",      0, -1200},
    {"汽车抛锚，-1500",      0, -1500},
    {"牢狱之灾，停2轮",      3, 0},
    {"遭遇小偷，-800",       0, -800},
    {"缴纳所得税，-10%",     5, 10},
    {"生病住院，-1000",      0, -1000},
    {"迷路了，后退2格",      2, 2},
    {"天上掉馅饼，+1800",    0, 1800},
};

} // namespace rich
