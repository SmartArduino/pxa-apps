// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "fx.hpp"
#include "sprites.hpp"
#include "ground.hpp"
namespace iron {
struct Battlefield {
 float camx=0,camy=0,zoom=1;int width=480,height=320,top=43,bottom=224;float clock=0;GroundCache ground;bool cache_ground=true;
 Pt project(Vec p,float z=0)const{return {width*.5f+(p.x-p.y)*20*zoom-camx,top+(bottom-top)*.5f+(p.x+p.y)*10*zoom-camy-z*zoom};}
 Vec unproject(int x,int y)const{float dx=(x-width*.5f+camx)/(20*zoom),dy=(y-top-(bottom-top)*.5f+camy)/(10*zoom);return {(dy+dx)*.5f,(dy-dx)*.5f};}
 void focus(Vec p){camx=(p.x-p.y)*20*zoom;camy=(p.x+p.y)*10*zoom;}
 bool onscreen(Pt p,int r=60)const{return p.x>=-r&&p.x<width+r&&p.y>=top-r&&p.y<bottom+r;}
 void quad(Raster& r,Vec p,float sx,float sy,float z,uint16_t c){r.poly({project({p.x-sx,p.y-sy},z),project({p.x+sx,p.y-sy},z),project({p.x+sx,p.y+sy},z),project({p.x-sx,p.y+sy},z)},c);}
 [[gnu::noinline]] void cube(Raster& r,Vec p,float sx,float sy,float z,uint16_t roof,uint16_t left,uint16_t right,float base=0){auto a=project({p.x-sx,p.y-sy},z+base),b=project({p.x+sx,p.y-sy},z+base),c=project({p.x+sx,p.y+sy},z+base),d=project({p.x-sx,p.y+sy},z+base);r.poly({d,c,project({p.x+sx,p.y+sy},base),project({p.x-sx,p.y+sy},base)},left);r.poly({b,c,project({p.x+sx,p.y+sy},base),project({p.x+sx,p.y-sy},base)},right);r.poly({a,b,c,d},roof);r.line(a.x,a.y,b.x,b.y,mix(roof,cream,4));r.line(a.x,a.y,d.x,d.y,mix(roof,cream,3));}
 void wire(Raster& r,Vec a,Vec b,float z,uint16_t c,int thick=1){auto aa=project(a,z),bb=project(b,z);r.line(aa.x,aa.y,bb.x,bb.y,c,thick);}
 [[gnu::noinline]] void building_art(Raster& r,const Entity& a){
  Vec p=a.p;bool enemy=a.team==1;auto team=enemy?red:cyan;float size=a.kind==turret?.52f:a.kind==power?.75f:1.f;
  quad(r,{p.x+.22f,p.y+.32f},size+.18f,size+.18f,0,rgb(22,33,32));
  if(a.progress>0&&a.kind!=supply){cube(r,p,size,size,7,rgb(57,73,72),rgb(41,57,59),navy);for(int i=-1;i<=1;++i){wire(r,{p.x+i*.6f,p.y-size},{p.x+i*.6f,p.y+size},19,gold);wire(r,{p.x-size,p.y+i*.6f},{p.x+size,p.y+i*.6f},19,gold);}auto t=project(p,28);r.text(t.x,t.y,"施工中",gold,11,1);return;}
  unsigned id=art::buildings_first+std::clamp(int(a.kind)-int(hq),0,7);auto q=project(p);unsigned variant=(enemy?1:0)+(a.flash>0?2:0);Sprites::anchor(r,id,std::lround(q.x),std::lround(q.y),zoom,variant);
  if(a.kind==turret)turret_art(r,p,a.dir,21,team,.95f);
  if(a.kind==power){for(int i=0;i<2;++i){auto vent=project({p.x-.28f+i*.55f,p.y},37);float age=std::fmod(clock*.6f+i*.5f,1.f);r.ellipse(vent.x+age*5*zoom,vent.y-age*18*zoom,(1+age*3)*zoom,(1+age*2)*zoom,mix(rgb(109,129,119),navy,int(age*9)));}}
  if(a.kind==supply){auto& frame=art::frames[id];int yy=q.y-int((frame.oy-frame.by)*zoom);r.label({int(q.x)-46,yy-20,92,18},"补给 +280",cyan,11);if(a.progress>0){r.rect(q.x-18,yy+2,36,4,navy);r.rect(q.x-17,yy+3,34*std::min(1.f,a.progress/5),2,cyan);}return;}
  if(a.kind==relay){auto& frame=art::frames[id];r.dot(q.x,q.y-(frame.oy-frame.by)*zoom,a.team==2?gold:cyan);}
  if(a.hp<specs[a.kind].hp*.35f){auto smoke=project(p,25);for(int i=0;i<3;++i){float age=std::fmod(clock*.65f+i*.34f,1.f);r.ellipse(smoke.x+age*9*zoom,smoke.y-age*25*zoom,(2+age*5)*zoom,(3+age*4)*zoom,mix(rgb(82,82,70),navy,int(age*9)));}}
 }
 [[gnu::noinline]] void turret_art(Raster& r,Vec p,float angle,float z,uint16_t team,float length=.8f,float elevation=0){float dx=std::cos(angle),dy=std::sin(angle);cube(r,p,.28f,.3f,5,rgb(131,146,123),rgb(61,78,68),rgb(45,62,57),z-4);auto a=project(p,z+3),b=project({p.x+dx*length,p.y+dy*length},z+3+elevation);r.line(a.x,a.y,b.x,b.y,rgb(24,34,33),5*zoom);r.line(a.x,a.y,b.x,b.y,rgb(155,163,140),2*zoom);r.rect(a.x-2,a.y-3,4,2,team);}
 [[gnu::noinline]] void unit_art(Raster& r,const Entity& a){
  auto p=project(a.p);int x=std::lround(p.x),y=std::lround(p.y);bool infantry=a.kind<=rocket;auto team=a.team==1?red:cyan;
  if(a.selected){
   int rx=std::max(6,int((infantry?8:18)*zoom)),ry=std::max(3,int((infantry?4:8)*zoom)),arm=std::max(2,int(4*zoom));
   for(int side:{-1,1})for(int end:{-1,1}){int xx=x+side*rx,yy=y+end*ry;r.line(xx,yy,xx-side*arm,yy,team);r.line(xx,yy,xx,yy-end*std::max(2,arm/2),team);}
   if(a.moving){auto g=project(a.goal);if(onscreen(g)){int d=std::max(2,int(3*zoom));r.line(g.x-d,g.y,g.x+d,g.y,mix(team,navy,5));r.line(g.x,g.y-d,g.x,g.y+d,mix(team,navy,5));}}
  }
  if(a.moving&&a.kind>=tank)for(int j=0;j<2;++j){float age=std::fmod(clock*1.5f+j*.5f,1.f);Vec dust={a.p.x-std::cos(a.dir)*(.5f+age*.8f),a.p.y-std::sin(a.dir)*(.5f+age*.8f)};auto d=project(dust,2+age*2);r.ellipse(d.x,d.y,(2+age*4)*zoom,(1+age*2)*zoom,mix(rgb(124,119,84),rgb(64,77,57),int(age*13)));}
  unsigned pose=a.kind==harvester?(a.cargo>0):(a.moving?(unsigned(clock*8)&1):0);unsigned id=Sprites::unit_frame(a.kind,a.dir,pose);unsigned variant=(a.team==1?1:0)+(a.flash>0?2:0);
  r.ellipse(x+3*zoom,y+3*zoom,(infantry?6:20)*zoom,(infantry?3:8)*zoom,rgb(26,38,33));Sprites::anchor(r,id,x,y,zoom,variant);
  unsigned rank=veteran_rank(a);if(rank){int yy=y-(infantry?46:53)*zoom;for(unsigned j=0;j<rank;++j){int xx=x+(int(j)*6-int(rank-1)*3);r.line(xx-2,yy+3,xx,yy,gold);r.line(xx,yy,xx+2,yy+3,gold);}}
  if(a.hp<specs[a.kind].hp*.3f&&!infantry){auto smoke=project(a.p,15);r.ellipse(smoke.x+3*zoom,smoke.y-(5+int(clock*8)%9)*zoom,3*zoom,4*zoom,edge);}
  if(a.selected||a.hp<specs[a.kind].hp){int ww=infantry?16:27,yy=y-(infantry?40:47)*zoom;r.rect(x-ww/2-1,yy-1,ww+2,4,navy);r.rect(x-ww/2,yy,ww*std::clamp(a.hp/specs[a.kind].hp,0.f,1.f),2,a.hp>specs[a.kind].hp*.35f?team:red);}
 }
 [[gnu::noinline]] void ground_details(Raster& r,const World& w,Vec p,unsigned hash,uint16_t col){
  int x=int(p.x),y=int(p.y),c=y*N+x;auto q=project(p);
  if(w.terrain[c]==2){
   uint16_t foam=mix(col,rgb(155,179,156),6);
   if(x>0&&w.terrain[c-1]!=2)wire(r,{p.x-.48f,p.y-.5f},{p.x-.48f,p.y+.5f},1,foam);
   if(x<N-1&&w.terrain[c+1]!=2)wire(r,{p.x+.48f,p.y-.5f},{p.x+.48f,p.y+.5f},1,foam);
   if(y>0&&w.terrain[c-N]!=2)wire(r,{p.x-.5f,p.y-.48f},{p.x+.5f,p.y-.48f},1,foam);
   if(y<N-1&&w.terrain[c+N]!=2)wire(r,{p.x-.5f,p.y+.48f},{p.x+.5f,p.y+.48f},1,foam);
   return;
  }
  if(w.terrain[c]>=3){
   r.ellipse(q.x,q.y,13*zoom,6*zoom,mix(col,navy,10));r.ring(q.x,q.y,12*zoom,5*zoom,mix(col,cream,3));
   for(int j=0;j<4;++j){int dx=(int((hash>>(j*3))%19)-9)*zoom,dy=(int((hash>>(j*2))%9)-4)*zoom;r.rect(q.x+dx,q.y+dy,(2+j%2)*zoom,2*zoom,rgb(78+j*8,84+j*7,73+j*4));}
   if(w.terrain[c]==5)quad(r,p,.35f,.35f,1,rgb(53,65,61));
  }else if(!w.ore[c]&&hash%4==0){
   int dx=(int(hash%13)-6)*zoom,dy=(int(hash%7)-3)*zoom;
   r.line(q.x+dx,q.y+dy,q.x+dx-2*zoom,q.y+dy-3*zoom,mix(col,rgb(149,153,99),5));r.line(q.x+dx,q.y+dy,q.x+dx+2*zoom,q.y+dy-4*zoom,mix(col,rgb(113,142,83),7));
  }
  if(w.chapter==1&&x>=13&&x<=15&&(y==5||y==8||y==16||y==19||y==23||y==25)){
   quad(r,p,.5f,.5f,1,rgb(102,115,105));wire(r,{p.x-.5f,p.y-.45f},{p.x+.5f,p.y-.45f},4,rgb(153,164,139));wire(r,{p.x-.5f,p.y+.45f},{p.x+.5f,p.y+.45f},4,rgb(80,101,93));
  }
 }
 void rallies(Raster& r,const World& w){for(auto& a:w.e)if(a.hp>0&&a.team==0&&a.selected&&a.moving&&(a.kind==factory||a.kind==barracks)){
  for(int i=0;i<10;++i){float t=i/10.f;Vec p={a.p.x+(a.goal.x-a.p.x)*t,a.p.y+(a.goal.y-a.p.y)*t};auto q=project(p,1);r.rect(q.x,q.y,2,1,cyan);}
  auto flag=project(a.goal,2);r.ring(flag.x,flag.y,9*zoom,4*zoom,cyan);r.line(flag.x,flag.y,flag.x,flag.y-18*zoom,cream);r.poly({Pt{flag.x,flag.y-18*zoom},Pt{flag.x+12*zoom,flag.y-15*zoom},Pt{flag.x,flag.y-10*zoom},Pt{flag.x,flag.y-18*zoom}},gold);
 }}
 [[gnu::noinline]] void draw_ground(Raster& r,const World& w){
  for(int y=top;y<bottom;y+=4)r.rect(0,y,width,std::min(4,bottom-y),mix(rgb(17,31,36),rgb(34,47,40),(y-top)*6/std::max(1,bottom-top)));
  for(int d=0;d<N*2-1;++d)for(int x=0;x<N;++x){int y=d-x;if(y<0||y>=N)continue;int c=y*N+x;Vec p={x+.5f,y+.5f};auto q=project(p);if(!onscreen(q,42))continue;unsigned hash=x*8749+y*6329+x*y*17;
   uint16_t color=!w.seen[c]?rgb(20+hash%3,32+hash%3,34+hash%3):rgb(62,76,57);quad(r,p,.51f,.51f,0,color);if(!w.seen[c])continue;
   unsigned tile=hash%5==0?1:0;if(w.terrain[c]==2)tile=4+(int(clock*2)&1);else if(w.ore[c])tile=7;else if(w.terrain[c]>=3)tile=6;else if(x>=2&&x<=25&&y>=9&&y<=10)tile=2;else if(y>=3&&y<=25&&x>=10&&x<=11)tile=3;
   Sprites::blit(r,art::terrain_first+tile,std::lround(q.x-20*zoom),std::lround(q.y-10*zoom),zoom,zoom,w.fog[c]?0:4);
   if(w.chapter==1&&x>=13&&x<=15&&(y==5||y==8||y==16||y==19||y==23||y==25))ground_details(r,w,p,hash,color);
  }
 }
 [[gnu::noinline]] void draw(Raster& r,const World& w){
  r.clip={0,top,width,bottom-top};
  if(cache_ground)ground.draw(r,w,width,top,bottom,camx,camy,zoom,int(clock*2)%4,[&](Raster& backing){draw_ground(backing,w);});else draw_ground(r,w);
  rallies(r,w);
  // Painter sort props and entities by their ground plane depth.
  std::array<int,MAX> order{};int num=0;for(int i=0;i<MAX;++i)if(w.e[i].hp>0&&(w.e[i].team==0||w.fog[cell(w.e[i].p)]))order[num++]=i;std::sort(order.begin(),order.begin()+num,[&](int a,int b){return w.e[a].p.x+w.e[a].p.y<w.e[b].p.x+w.e[b].p.y;});
  for(int d=0,ii=0;d<N*2;++d){for(int x=1;x<N-1;++x){int y=d-x;if(y<1||y>=N-1)continue;int c=y*N+x;if(!w.fog[c]||w.ore[c]||w.terrain[c]==2)continue;Vec p={x+.5f,y+.5f};auto q=project(p);if(!onscreen(q,30))continue;uint32_t hash=x*8749+y*6329+x*y*17;if(!((hash%23==0&&x>12)||hash%37==0||hash%19==0||hash%31==0))continue;bool occupied=false;for(auto& a:w.e)if(a.hp>0&&dist(a.p,p)<1.8f){occupied=true;break;}if(occupied)continue;
    if(hash%23==0&&x>12){r.ellipse(q.x+3,q.y+2,8*zoom,3*zoom,rgb(31,44,34));r.line(q.x,q.y,q.x,q.y-18*zoom,rgb(76,65,43),3*zoom);for(int j=0;j<3;++j){int yy=q.y-(10+j*6)*zoom,s=(8-j*2)*zoom;r.poly({Pt{q.x-s,float(yy)},Pt{q.x,float(yy-10*zoom)},Pt{q.x+s,float(yy)},Pt{q.x,float(yy+3*zoom)}},j==2?rgb(94,116,76):rgb(44+j*12,72+j*12,52+j*7));}}
    else if(hash%37==0){cube(r,p,.18f,.24f,7,rgb(149,134,86),rgb(92,91,63),rgb(57,73,61));wire(r,{p.x-.18f,p.y+.24f},{p.x+.18f,p.y+.24f},5,gold);}
    else if(hash%19==0){cube(r,p,.22f,.26f,4+hash%4,rgb(119,122,100),rgb(77,88,77),rgb(56,70,65));}
    else if(hash%31==0){wire(r,{p.x-.5f,p.y},{p.x+.5f,p.y},4,rgb(138,137,109));auto v=project(p);r.line(v.x,v.y,v.x,v.y-7,edge);}
   }
   while(ii<num&&int(w.e[order[ii]].p.x+w.e[order[ii]].p.y)<=d){auto& a=w.e[order[ii++]];if(!onscreen(project(a.p),110))continue;if(a.kind>=hq)building_art(r,a);else unit_art(r,a);if(a.kind>=hq&&(a.selected||a.hp<specs[a.kind].hp)){auto p=project(a.p,48);r.rect(p.x-18,p.y,36,3,navy);r.rect(p.x-17,p.y+1,34*std::clamp(a.hp/specs[a.kind].hp,0.f,1.f),1,a.team==1?red:cyan);}}
  }
  for(auto& s:w.shots)if(s.alive&&w.fog[cell(s.p)]){
   float z=s.kind==artillery?std::sin(s.t/s.duration*3.14159f)*50:8;auto a=project(s.p,z),shadow=project(s.p);
   if(s.kind==artillery)r.ellipse(shadow.x,shadow.y,3*zoom,2*zoom,navy);
   for(int i=3;i>=0;--i){float t=std::max(0.f,s.t/s.duration-i*.065f);Vec trail={s.from.x+(s.dest.x-s.from.x)*t,s.from.y+(s.dest.y-s.from.y)*t};auto b=project(trail,z);r.line(a.x,a.y,b.x,b.y,i?mix(gold,red,i*3):cream,s.kind==rifle?1:2);if(s.kind==rocket&&i)r.ellipse(b.x,b.y,2*zoom,2*zoom,edge);}
   r.dot(a.x,a.y,cream);
  }
  for(auto& b:w.blasts)if(b.alive&&w.fog[cell(b.p)]){
   auto p=project(b.p,b.type==5?10:b.age*10);
   if(b.type==5){int d=(2+int((.16f-b.age)*20))*zoom;r.line(p.x-d,p.y,p.x+d,p.y,gold);r.line(p.x,p.y-d,p.x,p.y+d,cream);continue;}
   auto origin=project(b.p);
   if(b.type==3){draw_fx(r,3,origin.x,origin.y,b.age/.7f,zoom*.9f);continue;}
   if(b.type==4||b.type==6){float f=b.age/(b.type==6?1.2f:1.f);draw_fx(r,4,origin.x,origin.y,f,zoom);if(b.type==6)r.text(p.x,p.y-20,"+280",gold,16,1);else {r.line(p.x-6,p.y-10,p.x,p.y-17,gold,2);r.line(p.x,p.y-17,p.x+6,p.y-10,gold,2);}continue;}
   float life=b.type==2?2.2f:b.type==1?1.1f:.65f;draw_fx(r,b.type==2?2:b.type==1?1:0,origin.x,origin.y,b.age/life,zoom);
  }
  r.clip={0,0,r.w,r.h};
 }
};
}
