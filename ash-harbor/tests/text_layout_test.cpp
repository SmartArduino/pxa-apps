// SPDX-License-Identifier: MIT
#include "../text_layout.hpp"
#include <cassert>
#include <cstdio>
using namespace ash;
int main(){int cases=0;
 for(const char* value:{"街机","训练","招式","开始训练","等待对手继续","等待对手再战","重置位置","设置","1","9","<","192.168.100.200","YAN","gyp"}){
  PixelText run(value);assert(run.valid);auto ink=run.ink(0,run.count);
  for(int align=0;align<3;++align)for(int h:{12,14,16,18,20,24,32}){
   UiRect box{8,3,float(ink.w+16),float(h)};auto plan=planText(run,box);
   assert(!plan.elided&&plan.count==1);
   auto origin=textOrigin(box,ink,align);float x=origin.x+ink.x,y=origin.y+ink.y;
   assert(x>=box.x&&x+ink.w<=box.x+box.w&&y>=box.y&&y+ink.h<=box.y+box.h);
   assert(std::abs(y+ink.h*.5f-(box.y+box.h*.5f))<=.5f);
   if(align==1)assert(std::abs(x+ink.w*.5f-(box.x+box.w*.5f))<=.5f);
   ++cases;
  }
 }
 {PixelText mixed("拳+脚：投技／挣脱");assert(mixed.label&&mixed.ink(0,mixed.count).w<int(mixed.count*12));
  auto plan=planText(mixed,{0,0,36,40},3);assert(!plan.elided&&plan.count==3);unsigned seen=0;
  for(unsigned i=0;i<plan.count;++i){auto line=plan.lines[i];assert(line.first==seen&&line.ink.w<=36);seen+=line.count;}
  assert(seen==mixed.count);++cases;}
 {PixelText large("等待对手继续");assert(planText(large,{0,0,20,12}).elided);assert(planText(large,{0,0,100,5}).elided);++cases;}
 for(int lift=6;lift<=84;++lift){
  auto keys=addressLayout(lift);for(int i=0;i<12;++i){auto box=keys.key(i);assert(box.w>=54&&box.h>=16);assert(box.y>=keys.inputY+12&&box.y+box.h<=210-lift-4);}
  for(int c=0;c<14;++c)for(auto d:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1}}){int next=addressNavigate(c,d.first,d.second,lift);assert(next>=0&&next<14);}
  for(bool opponent:{false,true}){auto grid=cardGrid(lift,opponent);assert(grid.height>=36);assert(grid.row(2)+grid.height<=(opponent?192:210)-lift-6);}
  auto home=homeLayout(std::max(0,lift-12));assert(home.toolbar+20<home.character&&home.character+26<home.actions);assert(home.logoHeight+4<=home.toolbar-12);
  auto rooms=roomListLayout(lift,4,3);assert(rooms.capacity>=1&&rooms.first+rooms.capacity==4&&rooms.step-2>=18);assert(65+rooms.step*rooms.capacity<=210-lift-6);
  ++cases;
 }
 for(int width:{296,264,232,208})for(int i=0;i<4;++i){auto box=homeControl(i,(320-width)/2,(320+width)/2,106);assert(box.w>=38);if(i)assert(homeControl(i-1,(320-width)/2,(320+width)/2,106).x+homeControl(i-1,(320-width)/2,(320+width)/2,106).w<=box.x);++cases;}
 std::printf("%d ink alignment, full-glyph wrapping, compact keyboard/card and reserved-bottom layout cases passed\n",cases);
}
