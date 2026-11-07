// SPDX-License-Identifier: MIT
#include "../living.hpp"
#include <cassert>
#include <cstdio>
#include <set>
using namespace farm;
static int luminance(uint16_t c) {
  return int(c>>11)*2+int((c>>5)&63)*3+int(c&31);
}
int main() {
  State s;fresh(s);s.minute=720;
  auto day=ambient_light(s);assert(!day.key());
  for(unsigned c=0;c<65536;++c)assert(apply_light(c,day)==c);
  s.minute=1080;auto dusk=ambient_light(s);
  s.minute=1320;auto night=ambient_light(s);
  s.minute=360;auto dawn=ambient_light(s);
  assert(dawn.key()!=dusk.key() && dusk.key()!=night.key());
  assert((dusk.color>>11)>(dusk.color&31));
  assert((night.color&31)>(night.color>>11));
  auto ground=uint16_t((17<<11)|(44<<5)|10);
  assert(luminance(apply_light(ground,night))<luminance(ground)*8/10);
  for(int season=0;season<4;++season) {
    s.day=season*28+1;s.weather=0;
    std::set<int> tones;
    for(int time=360;time<=1550;++time) {
      s.minute=time;auto tone=ambient_light(s);tones.insert(tone.key());
      auto bucket=s;s.minute=time/10*10;assert(ambient_light(s).key()==tone.key());s=bucket;
      if(time%10) {auto prev=s;--prev.minute;assert(ambient_light(prev).key()==tone.key());}
    }
    assert(tones.size()>30);
  }
  s.minute=1080;s.day=1;auto spring=ambient_light(s);
  s.day=29;auto summer=ambient_light(s);
  s.day=85;auto winter=ambient_light(s);
  assert(summer.amount<spring.amount && spring.amount<winter.amount);
  s.minute=720;s.weather=unsigned(Weather::Rain);auto rain=ambient_light(s);
  s.weather=unsigned(Weather::Storm);assert(ambient_light(s).amount>rain.amount);
  for(int region:{4,6,7})for(int time:{360,1080,1320,1500}) {
    s.region=region;s.minute=time;assert(!ambient_light(s).key());
  }
  puts("PASS: identity daylight, warm dawn/sunset, blue night, smooth cached ten-minute transitions, seasonal sunset, overcast weather and stable indoor lighting");
}
