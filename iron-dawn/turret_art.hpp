// SPDX-License-Identifier: MIT
#pragma once
#include "sprites.hpp"
namespace iron {
struct TurretArt {
 static constexpr unsigned base=art::buildings_first+5;
 static unsigned frame(unsigned facing,bool recoil){return art::turret_heads_first+(facing&7)+(recoil?8:0);}
 static void draw(Raster& r,int x,int y,float scale,unsigned facing,bool recoil,unsigned variant=0){Sprites::anchor(r,base,x,y,scale,variant);Sprites::anchor(r,frame(facing,recoil),x,y-int(std::lround(art::turret_mount_height*scale)),scale,variant);}
 static Pt muzzle(unsigned facing,bool recoil){unsigned n=(facing&7)+(recoil?8:0);const auto& f=art::frames[art::turret_heads_first+n];auto p=art::turret_muzzles[n];return {float(p.x-f.ox),float(p.y-f.oy-art::turret_mount_height)};}
 static Rect bounds(unsigned facing=0,bool recoil=false){auto& a=art::frames[base];auto& b=art::frames[frame(facing,recoil)];int left=std::min(int(a.bx)-a.ox,int(b.bx)-b.ox),top=std::min(int(a.by)-a.oy,int(b.by)-b.oy-art::turret_mount_height),right=std::max(int(a.bx+a.bw)-a.ox,int(b.bx+b.bw)-b.ox),bottom=std::max(int(a.by+a.bh)-a.oy,int(b.by+b.bh)-b.oy-art::turret_mount_height);return {left,top,right-left,bottom-top};}
 static void fit(Raster& r,Rect area,unsigned variant=0){if(area.w<=0||area.h<=0)return;auto b=bounds();float scale=std::min(float(area.w)/b.w,float(area.h)/b.h);int x=area.x+int((area.w-b.w*scale)/2-b.x*scale),y=area.y+int((area.h-b.h*scale)/2-b.y*scale);auto clip=r.clip;int left=std::max(clip.x,area.x),top=std::max(clip.y,area.y),right=std::min(clip.x+clip.w,area.x+area.w),bottom=std::min(clip.y+clip.h,area.y+area.h);r.clip={left,top,std::max(0,right-left),std::max(0,bottom-top)};draw(r,x,y,scale,0,false,variant);r.clip=clip;}
};
}
