// SPDX-License-Identifier: MIT
#pragma once
#include "paint.hpp"
#include <cstring>
namespace motion {
struct Controls {
 struct Item {Rect area;int id=0;bool enabled=true,grid=false;uint16_t color=mint;std::array<char,96> text{};};
 std::array<Item,24> items{};unsigned count=0;int focus=-1,pressed=-1,down_x=0,down_y=0;
 void begin(){count=0;}
 void add(Rect area,int id,std::string_view text,uint16_t color=mint,bool enabled=true,bool grid=false){if(count==items.size())return;auto &i=items[count++];i={};i.area=area;i.id=id;i.enabled=enabled;i.grid=grid;i.color=color;auto n=std::min(text.size(),i.text.size()-1);std::memcpy(i.text.data(),text.data(),n);}
 int hit(int x,int y,bool enabled_only=true)const{for(unsigned i=0;i<count;++i)if(items[i].area.contains(x,y)&&(!enabled_only||items[i].enabled))return items[i].id;return -1;}
 bool enabled(int id)const{for(unsigned i=0;i<count;++i)if(items[i].id==id)return items[i].enabled;return false;}
 void normalize(){if(enabled(focus))return;focus=-1;for(unsigned i=0;i<count;++i)if(items[i].enabled){focus=items[i].id;break;}}
 void move(int direction){normalize();if(!count)return;int index=0;for(unsigned i=0;i<count;++i)if(items[i].id==focus)index=int(i);
   for(unsigned i=0;i<count;++i){index=(index+int(count)+direction)%int(count);if(items[index].enabled){focus=items[index].id;return;}}}
 void down(int x,int y){pressed=hit(x,y);down_x=x;down_y=y;}
 int up(int x,int y,bool cancelled=false){int id=pressed;pressed=-1;if(cancelled||std::abs(x-down_x)>12||std::abs(y-down_y)>12||id!=hit(x,y))return -1;focus=id;return id;}
 void paint(Paint& p){normalize();for(unsigned i=0;i<count;++i){auto &n=items[i];if(n.id==focus)p.round({n.area.x-2,n.area.y-2,n.area.w+4,n.area.h+4},teal,9);
   p.round(n.area,n.enabled?n.color:disabled,7);Rect label{n.area.x+4,n.area.y,n.area.w-8,n.area.h};if(n.grid){label.y+=1;label.h=std::min(30,n.area.h);}
   p.label(label,n.text.data(),n.enabled?ink:muted,1,n.grid?18:0);}}
};
}
