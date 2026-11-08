// SPDX-License-Identifier: MIT
#include "living.hpp"
#include "livestock.hpp"
#include "mines.hpp"
#include <algorithm>
namespace farm {
Weather weather_on(uint16_t day) {
  unsigned h=unsigned(day)*747796405u+2891336453u;
  h=((h>>((h>>28)+4))^h)*277803737u; h=(h>>22)^h;
  int season=season_of(day);
  if(season_day(day)==1) return Weather::Sun;
  if(season==3) return h%5<3?Weather::Snow:Weather::Sun;
  if(season==1 && h%9==0) return Weather::Storm;
  if(h%5==0) return Weather::Rain;
  return h%4==0?Weather::Wind:Weather::Sun;
}
const char *weather_name(Weather w,bool en) {
  const char *cn[]={"晴朗","下雨","雷雨","飘雪","有风"};
  const char *eng[]={"Sunny","Rain","Thunderstorm","Snow","Windy"};
  return (en?eng:cn)[unsigned(w)<5?unsigned(w):0];
}
bool wet_weather(Weather w) { return w==Weather::Rain||w==Weather::Storm; }
bool shop_open(const State&s) { return (s.day-1)%7!=2 && s.minute>=540 && s.minute<1020; }
bool building_open(const State&s,Region b) {
  return b==Region::SeedShop?shop_open(s):b==Region::Lodge?s.minute>=480&&s.minute<1200:true;
}
bool crop_in_season(uint16_t day,int crop) {
  constexpr unsigned masks[]={1,5,2,4};
  return crop>=0&&crop<4&&(masks[crop]&(1u<<season_of(day)));
}
int food_energy(int item) {
  constexpr int crops[]={10,12,14,0}, wild[]={14,18,20,22,0};
  if(item==43||item==44)return item==43?20:28;
  if(item>=29&&item<=36){constexpr int energy[]={8,10,18,16,0,0,8,10};return energy[item-29];}
  return item>=12&&item<=15?crops[item-12]:item>=19&&item<=23?Foods[item-19].energy:
    item>=24&&item<=28?wild[item-24]:0;
}
int item_price(int item) {
  if(item>=38&&item<=45){constexpr int price[]={12,18,32,55,100,30,45,10};return price[item-38];}
  if(item>=29&&item<=36){constexpr int prices[]={20,28,65,55,80,120,28,36};return prices[item-29];}
  constexpr int wild[]={18,25,30,35,28};
  return item>=8&&item<=11?std::max(1,int(Crops[item-8].price)/2):
    item>=12&&item<=15?Crops[item-12].sell:item==16?24:item==17||item==18?2:
    item>=19&&item<=23?Foods[item-19].value:item>=24&&item<=28?wild[item-24]:0;
}
const char *seasonal_name(int item,bool en) {
  const char *cn[]={"春野菜","夏香草","秋榛果","冬根菜","湖贝壳"};
  const char *eng[]={"Spring greens","Summer herb","Hazelnut","Winter root","Lake shell"};
  return (en?eng:cn)[std::clamp(item-24,0,4)];
}
bool spend_energy(State&s,int amount) {
  if(amount<=0)return true;
  if(s.overwork>=10)return false;
  int paid=std::min(int(s.energy),amount);s.energy-=paid;
  if(!s.energy)s.exhausted=true;
  s.overwork=std::min(10,int(s.overwork)+amount-paid);
  return true;
}
int movement_speed(const State &s) {
  return !s.energy ? 32 : s.auto_run ? 112 : 64;
}
Lighting ambient_light(const State &s) {
  if (indoors(s.region)) return {};
  struct Key { int minute, r, g, b, amount; };
  constexpr int sunsets[] = {1080, 1140, 1050, 990};
  int sunset = sunsets[season_of(s.day)];
  const Key keys[] = {
      {0, 22, 30, 59, 160}, {300, 39, 48, 81, 115},
      {360, 209, 164, 131, 45}, {420, 236, 197, 143, 24},
      {480, 240, 208, 164, 0}, {sunset-120, 240, 208, 164, 0},
      {sunset-60, 240, 174, 112, 24}, {sunset, 218, 124, 88, 64},
      {sunset+50, 100, 73, 120, 102},
      {sunset+120, 28, 38, 73, 145}, {1440, 22, 30, 59, 160},
      {1560, 22, 30, 59, 160}};
  // One cached palette per ten game minutes (five real seconds). Daylight
  // stays an identity palette; walking never recalculates the LUT.
  int minute = std::clamp(int(s.minute), 0, 1560) / 10 * 10;
  int i = 1;
  while (i < 11 && minute > keys[i].minute) ++i;
  const auto &a = keys[i-1], &b = keys[i];
  int span = b.minute-a.minute, t = minute-a.minute;
  auto mix = [&](int first, int last) { return first+(last-first)*t/span; };
  int r = mix(a.r,b.r), g = mix(a.g,b.g), blue = mix(a.b,b.b);
  int amount = mix(a.amount,b.amount);
  int overcast = s.weather==unsigned(Weather::Storm) ? 64 :
                 s.weather==unsigned(Weather::Rain) ? 32 :
                 s.weather==unsigned(Weather::Snow) ? 16 : 0;
  if (overcast) {
    int retained = amount*(256-overcast)/256;
    amount = retained+overcast;
    r = (r*retained+94*overcast)/amount;
    g = (g*retained+113*overcast)/amount;
    blue = (blue*retained+139*overcast)/amount;
  }
  return {uint16_t(((r&248)<<8)|((g&252)<<3)|(blue>>3)),uint8_t(amount)};
}
uint16_t apply_light(uint16_t color, Lighting light) {
  unsigned a=light.amount, keep=256-a;
  unsigned r=(((color>>11)*keep+(light.color>>11)*a+128)>>8);
  unsigned g=((((color>>5)&63)*keep+((light.color>>5)&63)*a+128)>>8);
  unsigned b=(((color&31)*keep+(light.color&31)*a+128)>>8);
  return uint16_t((r<<11)|(g<<5)|b);
}
const char *calendar_event(int date,bool en) {
  int day=season_day(date),season=season_of(date);
  if(day==7&&season==0)return en?"Xiaohe's birthday":"小禾的生日";
  if(day==12&&season==1)return en?"Alan's birthday":"阿岚的生日";
  if(day==19&&season==2)return en?"Cheng's birthday":"澄的生日";
  if(day==25&&season==3)return en?"Lin's birthday":"林婆婆的生日";
  if(day==28)return en?"Season's last day":"本季最后一天";
  return en?"A quiet valley day":"晴露谷的日常";
}
}
