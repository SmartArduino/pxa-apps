// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "raster.hpp"
namespace iron {
struct Battlefield {
 float camx=0,camy=0,zoom=1;int width=480,height=320,top=43,bottom=224;float clock=0;
 Pt project(Vec p,float z=0)const{return {width*.5f+(p.x-p.y)*20*zoom-camx,top+(bottom-top)*.5f+(p.x+p.y)*10*zoom-camy-z*zoom};}
 Vec unproject(int x,int y)const{float dx=(x-width*.5f+camx)/(20*zoom),dy=(y-top-(bottom-top)*.5f+camy)/(10*zoom);return {(dy+dx)*.5f,(dy-dx)*.5f};}
 void focus(Vec p){camx=(p.x-p.y)*20*zoom;camy=(p.x+p.y)*10*zoom;}
 bool onscreen(Pt p,int r=60)const{return p.x>=-r&&p.x<width+r&&p.y>=top-r&&p.y<bottom+r;}
 void quad(Raster& r,Vec p,float sx,float sy,float z,uint16_t c){r.poly({project({p.x-sx,p.y-sy},z),project({p.x+sx,p.y-sy},z),project({p.x+sx,p.y+sy},z),project({p.x-sx,p.y+sy},z)},c);}
 [[gnu::noinline]] void cube(Raster& r,Vec p,float sx,float sy,float z,uint16_t roof,uint16_t left,uint16_t right,float base=0){auto a=project({p.x-sx,p.y-sy},z+base),b=project({p.x+sx,p.y-sy},z+base),c=project({p.x+sx,p.y+sy},z+base),d=project({p.x-sx,p.y+sy},z+base);r.poly({d,c,project({p.x+sx,p.y+sy},base),project({p.x-sx,p.y+sy},base)},left);r.poly({b,c,project({p.x+sx,p.y+sy},base),project({p.x+sx,p.y-sy},base)},right);r.poly({a,b,c,d},roof);r.line(a.x,a.y,b.x,b.y,mix(roof,cream,4));r.line(a.x,a.y,d.x,d.y,mix(roof,cream,3));}
 void wire(Raster& r,Vec a,Vec b,float z,uint16_t c,int thick=1){auto aa=project(a,z),bb=project(b,z);r.line(aa.x,aa.y,bb.x,bb.y,c,thick);}
 [[gnu::noinline]] void building_art(Raster& r,const Entity& a){Vec p=a.p;bool enemy=a.team==1;auto team=enemy?red:cyan;uint16_t roof=rgb(96,113,107),wall=rgb(57,73,72),side=rgb(41,57,59);if(a.flash>0)roof=rgb(188,186,143);float size=a.kind==turret?.52f:a.kind==power?.75f:1.f;quad(r,{p.x+.15f,p.y+.25f},size+.18f,size+.18f,0,rgb(24,38,35));cube(r,p,size+.12f,size+.12f,3,rgb(61,70,61),rgb(39,47,43),rgb(35,43,41));
  if(a.progress>0){cube(r,p,size,size,7,wall,side,navy);for(int i=-1;i<=1;++i){wire(r,{p.x+i*.6f,p.y-size},{p.x+i*.6f,p.y+size},19,gold);wire(r,{p.x-size,p.y+i*.6f},{p.x+size,p.y+i*.6f},19,gold);}auto t=project(p,28);r.text(t.x,t.y,"施工中",gold,11,1);return;}
  if(a.kind==hq){cube(r,p,1,1,19,roof,wall,side);cube(r,{p.x-.2f,p.y-.2f},.65f,.55f,12,rgb(128,139,126),wall,side,19);quad(r,{p.x-.2f,p.y-.2f},.45f,.34f,32,team);wire(r,{p.x-.6f,p.y+.99f},{p.x+.7f,p.y+.99f},11,team,2);auto m=project({p.x+.55f,p.y-.45f},35);r.line(m.x,m.y,m.x,m.y-21,cream);r.line(m.x,m.y-21,m.x+12,m.y-16,team,2);for(int i=0;i<4;++i)cube(r,{p.x-.7f+i*.4f,p.y+.5f},.09f,.18f,3,rgb(40,48,46),navy,side,20);}
  else if(a.kind==power){cube(r,p,.7f,.7f,12,roof,wall,side);for(int i=0;i<2;++i){Vec v={p.x-.28f+i*.55f,p.y};cube(r,v,.2f,.35f,20,rgb(153,159,138),wall,side,12);quad(r,v,.22f,.36f,33,team);auto s=project(v,36);r.ellipse(s.x,s.y,3*zoom,2*zoom,cream);for(int j=0;j<3;++j){int rise=int(clock*6+j*9)%28;r.ellipse(s.x+std::sin(clock+j)*4+rise*.15f,s.y-rise-5,(2+rise/8)*zoom,(2+rise/10)*zoom,rgb(133-rise*2,143-rise*2,131-rise*2));}}}
  else if(a.kind==refinery){cube(r,{p.x-.3f,p.y-.3f},.7f,.7f,22,roof,wall,side);cube(r,{p.x+.7f,p.y+.4f},.25f,.6f,13,gold,wall,side);for(int i=0;i<3;++i)wire(r,{p.x-.8f,p.y-.5f+i*.4f},{p.x+.2f,p.y-.5f+i*.4f},23,navy,2);quad(r,{p.x,p.y+1.5f},.8f,.5f,0,rgb(100,108,87));for(int i=0;i<4;++i)wire(r,{p.x-.8f+i*.4f,p.y+1.3f},{p.x-.6f+i*.4f,p.y+1.7f},1,gold,2);}
  else if(a.kind==barracks){cube(r,p,.9f,.85f,15,roof,wall,side);cube(r,p,.6f,.65f,7,rgb(109,125,107),wall,side,15);for(int i=0;i<4;++i)wire(r,{p.x-.5f+i*.34f,p.y-.7f},{p.x-.5f+i*.34f,p.y+.7f},23,edge);wire(r,{p.x-.55f,p.y+.86f},{p.x+.45f,p.y+.86f},11,team,3);cube(r,{p.x+.8f,p.y-.8f},.18f,.18f,24,team,wall,side);}
  else if(a.kind==factory){cube(r,p,1,1,19,roof,wall,side);for(int i=0;i<5;++i)wire(r,{p.x-.8f+i*.36f,p.y-.9f},{p.x-.8f+i*.36f,p.y+.9f},20,wall);cube(r,{p.x+.65f,p.y-.7f},.23f,.23f,10,gold,wall,side,20);wire(r,{p.x+.65f,p.y-.7f},{p.x-1.2f,p.y-.7f},31,gold,3);auto b=project({p.x-1.2f,p.y-.7f},31);r.line(b.x,b.y,b.x,b.y+14,gold);wire(r,{p.x+.99f,p.y-.55f},{p.x+.99f,p.y+.6f},11,navy,7);wire(r,{p.x+.99f,p.y-.55f},{p.x+.99f,p.y+.6f},17,team,2);}
  else if(a.kind==relay){cube(r,p,.7f,.7f,15,roof,wall,side);auto s=project(p,15),t=project(p,50);r.line(s.x,s.y,t.x,t.y,wall,4);r.line(s.x-8,s.y,t.x,t.y,roof);r.line(s.x+8,s.y,t.x,t.y,roof);r.ellipse(t.x,t.y-3,13*zoom,5*zoom,a.team==2?gold:cyan);r.ellipse(t.x,t.y-5,10*zoom,3*zoom,cream);r.line(t.x,t.y-5,t.x+7,t.y-13,wall,2);r.dot(t.x+7,t.y-14,int(clock*2)%2?red:gold);}
  else if(a.kind==turret){cube(r,p,.5f,.5f,11,roof,wall,side);turret_art(r,p,a.dir,19,team,true);}
  // Windows, vents, hazard lights and a lit team identifier.
  if(a.kind!=turret&&a.kind!=relay){for(int i=0;i<3;++i){Vec v={p.x-.5f+i*.4f,p.y+size+.015f};wire(r,v,{v.x+.14f,v.y},7,i==1?team:gold,2);}auto lamp=project({p.x+size,p.y},17);r.dot(lamp.x,lamp.y,int(clock*2)%2?team:cream);}
 }
 [[gnu::noinline]] void turret_art(Raster& r,Vec p,float angle,float z,uint16_t team,bool cannon=false){float dx=std::cos(angle),dy=std::sin(angle);cube(r,p,.28f,.3f,5,rgb(131,146,123),rgb(61,78,68),rgb(45,62,57),z-4);auto a=project(p,z+3),b=project({p.x+dx*(cannon?.95f:.8f),p.y+dy*(cannon?.95f:.8f)},z+3);r.line(a.x,a.y,b.x,b.y,rgb(24,34,33),5*zoom);r.line(a.x,a.y,b.x,b.y,rgb(155,163,140),2*zoom);r.rect(a.x-2,a.y-3,4,2,team);}
 [[gnu::noinline]] void unit_art(Raster& r,const Entity& a){Vec p=a.p;auto q=project(p);auto team=a.team==1?red:cyan;bool infantry=a.kind<=rocket;
  if(a.selected){r.ring(q.x,q.y+1,infantry?9*zoom:17*zoom,infantry?4*zoom:8*zoom,cyan);if(a.moving){auto g=project(a.goal);if(onscreen(g))r.ring(g.x,g.y,7*zoom,3*zoom,mix(cyan,navy,6));}}
  r.ellipse(q.x+3,q.y+2,infantry?5*zoom:15*zoom,infantry?3*zoom:7*zoom,rgb(29,41,33));
  if(infantry){int walk=a.moving?int(std::sin(clock*11)*2*zoom):0;auto t=project(p,6);r.line(t.x-2,t.y+3,t.x-2-walk,t.y+8,rgb(42,49,43),2*zoom);r.line(t.x+2,t.y+3,t.x+2+walk,t.y+8,rgb(42,49,43),2*zoom);r.rect(t.x-3*zoom,t.y-3*zoom,6*zoom,7*zoom,a.flash>0?cream:rgb(109,125,92));r.rect(t.x-3*zoom,t.y-2*zoom,2*zoom,3*zoom,team);r.ellipse(t.x,t.y-5*zoom,3*zoom,2*zoom,rgb(147,154,115));r.rect(t.x,t.y-4*zoom,2*zoom,2*zoom,rgb(195,162,115));auto gun=project({p.x+std::cos(a.dir)*.4f,p.y+std::sin(a.dir)*.4f},7);r.line(t.x,t.y,gun.x,gun.y,a.kind==rocket?gold:navy,(a.kind==rocket?3:2)*zoom);}
  else {float dx=std::cos(a.dir),dy=std::sin(a.dir);auto corner=[&](float x,float y,float z){return project({p.x+dx*x-dy*y,p.y+dy*x+dx*y},z);};auto shape=[&](float x1,float y1,float x2,float y2,float z,uint16_t c){r.poly({corner(x1,y1,z),corner(x2,y1,z),corner(x2,y2,z),corner(x1,y2,z)},c);};
   for(int s=-1;s<=1;s+=2){shape(-.58f,s*.33f,.58f,s*.57f,3,rgb(29,37,34));for(int i=0;i<5;++i){auto v=corner(-.48f+i*.22f,s*.47f,5);r.rect(v.x,v.y,3*zoom,2*zoom,rgb(83,91,76));}}
   shape(-.52f,-.34f,.52f,.34f,5,rgb(58,77,63));shape(-.48f,-.32f,.48f,.32f,9,a.flash>0?cream:rgb(131,146,109));shape(-.4f,-.32f,-.25f,.32f,10,team);
   if(a.kind==harvester){shape(-.3f,-.3f,.3f,.3f,16,rgb(177,147,79));shape(.32f,-.28f,.48f,.28f,13,cyan);if(a.cargo){auto t=project(p,17);r.ellipse(t.x,t.y,6*zoom,3*zoom,gold);}}
   else if(a.kind==artillery){turret_art(r,p,a.dir,14,team,true);auto a1=project(p,16),b=project({p.x+dx*1.2f,p.y+dy*1.2f},26);r.line(a1.x,a1.y,b.x,b.y,navy,5*zoom);r.line(a1.x,a1.y,b.x,b.y,rgb(174,182,147),2*zoom);}
   else turret_art(r,p,a.dir,13,team);
  }
  if(a.selected||a.hp<specs[a.kind].hp){int ww=(a.kind>=hq)?34:infantry?15:24;int yy=q.y-((a.kind>=hq)?40:infantry?20:25)*zoom;r.rect(q.x-ww/2-1,yy-1,ww+2,4,navy);r.rect(q.x-ww/2,yy,ww*a.hp/specs[a.kind].hp,2,a.hp>specs[a.kind].hp*.35f?cyan:red);}
 }
 [[gnu::noinline]] void draw(Raster& r,const World& w){r.clip={0,top,width,bottom-top};r.rect(0,top,width,bottom-top,rgb(21,36,38));
  for(int d=0;d<N*2-1;++d)for(int x=0;x<N;++x){int y=d-x;if(y<0||y>=N)continue;int c=y*N+x;Vec p={x+.5f,y+.5f};auto q=project(p);if(!onscreen(q,42))continue;uint32_t hash=x*8749+y*6329+x*y*17;
   uint16_t col=rgb(67+(hash%6),79+(hash%8),58+(hash%7));if(w.chapter==1)col=rgb(67+hash%8,81+hash%9,75+hash%5);if(w.chapter==2)col=rgb(78+hash%7,76+hash%7,58+hash%6);
   if(w.terrain[c]==2)col=rgb(30+hash%4,57+hash%5,62+hash%5);if(!w.seen[c])col=rgb(20+hash%3,32+hash%3,34+hash%3);else if(!w.fog[c])col=mix(col,navy,9);quad(r,p,.51f,.51f,0,col);
   if(w.seen[c]){if(w.terrain[c]==2){r.line(q.x-6,q.y+int(clock*2+x)%4,q.x+4,q.y+int(clock*2+x)%4,mix(col,cyan,2));continue;}
    if((x>=2&&x<=25&&y>=9&&y<=10)||(y>=3&&y<=25&&x>=10&&x<=11)){quad(r,p,.5f,.5f,.1f,mix(col,rgb(118,116,88),6));wire(r,{p.x-.48f,p.y-.2f},{p.x+.48f,p.y-.2f},.2f,mix(col,navy,4));}
    for(int i=0;i<3;++i){int ox=(hash>>(i*4))%21-10,oy=(hash>>(i*3+2))%9-4;r.dot(q.x+ox*zoom,q.y+oy*zoom,mix(col,i%2?cream:navy,3));}
    if(w.ore[c])for(int j=0;j<4;++j){Vec v={p.x+float(int((hash>>(j*2))%7)-3)*.09f,p.y+(j-1.5f)*.16f};auto t=project(v,2+j%2*2);auto color=w.fog[c]?rgb(213+j*8,159+j*10,66+j*6):rgb(117,99,54);r.poly({Pt{t.x-3*zoom,t.y},Pt{t.x,t.y-5*zoom},Pt{t.x+3*zoom,t.y},Pt{t.x,t.y+2*zoom}},color);r.dot(t.x,t.y-3*zoom,cream);}
   }
  }
  // Painter sort props and entities by their ground plane depth.
  std::array<int,MAX> order{};int num=0;for(int i=0;i<MAX;++i)if(w.e[i].hp>0&&(w.e[i].team==0||w.fog[cell(w.e[i].p)]))order[num++]=i;std::sort(order.begin(),order.begin()+num,[&](int a,int b){return w.e[a].p.x+w.e[a].p.y<w.e[b].p.x+w.e[b].p.y;});
  for(int d=0,ii=0;d<N*2;++d){for(int x=1;x<N-1;++x){int y=d-x;if(y<1||y>=N-1)continue;int c=y*N+x;if(!w.fog[c]||w.ore[c]||w.terrain[c]==2)continue;Vec p={x+.5f,y+.5f};auto q=project(p);if(!onscreen(q,30))continue;uint32_t hash=x*8749+y*6329+x*y*17;bool occupied=false;for(auto& a:w.e)if(a.hp>0&&dist(a.p,p)<1.8f)occupied=true;if(occupied)continue;
    if(hash%23==0&&x>12){r.ellipse(q.x+3,q.y+2,8*zoom,3*zoom,rgb(31,44,34));r.line(q.x,q.y,q.x,q.y-18*zoom,rgb(76,65,43),3*zoom);for(int j=0;j<3;++j){int yy=q.y-(10+j*6)*zoom,s=(8-j*2)*zoom;r.poly({Pt{q.x-s,float(yy)},Pt{q.x,float(yy-10*zoom)},Pt{q.x+s,float(yy)},Pt{q.x,float(yy+3*zoom)}},j==2?rgb(94,116,76):rgb(44+j*12,72+j*12,52+j*7));}}
    else if(hash%19==0){cube(r,p,.22f,.26f,4+hash%4,rgb(119,122,100),rgb(77,88,77),rgb(56,70,65));}
    else if(hash%31==0){wire(r,{p.x-.5f,p.y},{p.x+.5f,p.y},4,rgb(138,137,109));auto v=project(p);r.line(v.x,v.y,v.x,v.y-7,edge);}
   }
   while(ii<num&&int(w.e[order[ii]].p.x+w.e[order[ii]].p.y)<=d){auto& a=w.e[order[ii++]];if(!onscreen(project(a.p)))continue;if(a.kind>=hq)building_art(r,a);else unit_art(r,a);if(a.kind>=hq&&(a.selected||a.hp<specs[a.kind].hp)){auto p=project(a.p,48);r.rect(p.x-18,p.y,36,3,navy);r.rect(p.x-17,p.y+1,34*a.hp/specs[a.kind].hp,1,a.team==1?red:cyan);}}
  }
  for(auto& s:w.shots)if(s.alive&&w.fog[cell(s.p)]){float z=s.kind==artillery?std::sin(s.t/s.duration*3.14159f)*50:8;auto a=project(s.p,z),b=project({s.p.x-(s.dest.x-s.from.x)*.04f,s.p.y-(s.dest.y-s.from.y)*.04f},z);r.line(a.x,a.y,b.x,b.y,gold,s.kind==rifle?1:2);r.dot(a.x,a.y,cream);}
  for(auto& b:w.blasts)if(b.alive&&w.fog[cell(b.p)]){auto p=project(b.p,b.age*10);float life=b.type==2?1.6f:.65f;float f=b.age/life;if(b.type==3){r.ring(p.x,p.y,14*zoom*f,7*zoom*f,cyan);continue;}if(f<.45f){int rad=(b.type==2?19:b.type==1?12:5)*zoom*(.5f+f);r.ellipse(p.x,p.y,rad,rad*.7f,red);r.ellipse(p.x-2,p.y-2,rad*.7f,rad*.5f,gold);r.ellipse(p.x-2,p.y-2,rad*.35f,rad*.3f,cream);}else {int rad=(b.type==2?15:8)*zoom*f;r.ellipse(p.x,p.y,rad,rad,rgb(62+int(f*13),67+int(f*13),61+int(f*10)));}for(int j=0;j<5&&f<.6f;++j){float a=j*1.256f;r.dot(p.x+std::cos(a)*f*35,p.y+std::sin(a)*f*25,gold);}}
  r.clip={0,0,r.w,r.h};
 }
};
}
