// SPDX-License-Identifier: MIT
#pragma once
#include "items.hpp"

namespace starsoil {
enum EnemyType : uint8_t {
  NoEnemy, DewSlime, DuskBat, Mossling, GemCrawler, Warden,
  FrostSlime, DuneBeetle, LanternMoth, ThornHornet, EnemyCount
};
struct EnemyDef {
  uint8_t width, height;
  uint16_t health;
  uint8_t damage, drop;
  const char *en, *zh, *habitat_en, *habitat_zh, *behavior_en, *behavior_zh;
};
inline constexpr EnemyDef enemies[EnemyCount] = {
    {},
    {14,12,24,8,Gel,"DEWDROP SLIME","叶露史莱姆",
     "FOREST, MEADOW","草甸与森林","SHORT HOPS","短距离跳跃"},
    {24,24,24,8,Silk,"DUSK BAT","暮翼蝠",
     "CAVES","地下洞穴","SWOOPS AT YOU","追逐与俯冲"},
    {10,24,40,14,StarDust,"MOSSLING","苔木精",
     "NIGHT SURFACE","夜间地表","OPENS DOORS","会推开木门"},
    {10,24,55,8,Crystal,"GEM CRAWLER","晶背爬兽",
     "DEEP CAVES","深层洞穴","ARMOURED HOPS","甲壳跳跃"},
    {30,32,280,22,StarDust,"STARHEART WARDEN","星核守卫",
     "STAR ALTAR","星核祭坛","HEAVY LEAPS","强力跳跃"},
    {16,14,32,10,Gel,"FROST SLIME","霜露史莱姆",
     "SNOW SURFACE","雪原地表","SLOW HIGH HOPS","缓慢高跳"},
    {20,14,38,11,SandBlock,"DUNE BEETLE","沙甲虫",
     "DESERT","沙漠地表","BRACE / RUSH","蓄力后冲刺"},
    {22,18,30,10,Silk,"LANTERN MOTH","灯蛾",
     "NIGHT SURFACE","夜间地表","DASH / FLEE","冲刺与晨退"},
    {22,16,36,9,Vines,"THORN HORNET","雨林刺蜂",
     "JUNGLE","雨林与洞穴","AIMS / SHOOTS","瞄准后射刺"},
};
constexpr bool flying_enemy(int type) {
  return type == DuskBat || type == LanternMoth || type == ThornHornet;
}
} // namespace starsoil
