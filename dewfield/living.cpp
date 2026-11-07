// SPDX-License-Identifier: MIT
#include "living.hpp"
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
  return item>=12&&item<=15?crops[item-12]:item>=19&&item<=23?Foods[item-19].energy:
    item>=24&&item<=28?wild[item-24]:0;
}
int item_price(int item) {
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
int daylight(const State&s) {
  if(indoors(s.region))return 0;
  int level=s.minute<420?1:s.minute<1020?0:std::min(6,(int(s.minute)-1020)/60+1);
  return std::min(5,level+(s.weather==unsigned(Weather::Storm)?1:0));
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
