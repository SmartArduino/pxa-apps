// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "fx.hpp"
#include "sprites.hpp"
#include "ground.hpp"
#include "locomotion.hpp"
#include "combat_art.hpp"
#include "shadow.hpp"
#include "turret_art.hpp"
#include "ground_texture.hpp"
namespace iron {
struct Battlefield {
 float camx=0,camy=0,zoom=1;int width=480,height=320,top=43,bottom=224;float clock=0;GroundCache ground;GroundTexture material;bool cache_ground=true,water_visible=false,cull_tiles=true;Locomotion locomotion;
 Pt project(Vec p,float z=0)const{return {width*.5f+(p.x-p.y)*20*zoom-camx,top+(bottom-top)*.5f+(p.x+p.y)*10*zoom-camy-z*zoom};}
 Vec unproject(int x,int y)const{float dx=(x-width*.5f+camx)/(20*zoom),dy=(y-top-(bottom-top)*.5f+camy)/(10*zoom);return {(dy+dx)*.5f,(dy-dx)*.5f};}
 void focus(Vec p){camx=(p.x-p.y)*20*zoom;camy=(p.x+p.y)*10*zoom;}
 bool onscreen(Pt p,int r=60)const{return p.x>=-r&&p.x<width+r&&p.y>=top-r&&p.y<bottom+r;}
 struct Tiles {int left,right,first,last;};
 Tiles visible_tiles(const World& world,int padding=110)const{if(!cull_tiles)return {0,world.side-1,0,world.side-1};Tiles v{world.side-1,0,world.side-1,0};for(int yy:{top-padding,bottom+padding})for(int xx:{-padding,width+padding}){auto p=unproject(xx,yy);v.left=std::min(v.left,int(std::floor(p.x)));v.right=std::max(v.right,int(std::ceil(p.x)));v.first=std::min(v.first,int(std::floor(p.y)));v.last=std::max(v.last,int(std::ceil(p.y)));}v.left=std::clamp(v.left,0,world.side-1);v.right=std::clamp(v.right,0,world.side-1);v.first=std::clamp(v.first,0,world.side-1);v.last=std::clamp(v.last,0,world.side-1);return v;}
 void quad(Raster& r,Vec p,float sx,float sy,float z,uint16_t c){r.poly({project({p.x-sx,p.y-sy},z),project({p.x+sx,p.y-sy},z),project({p.x+sx,p.y+sy},z),project({p.x-sx,p.y+sy},z)},c);}
 [[gnu::noinline]] void cube(Raster& r,Vec p,float sx,float sy,float z,uint16_t roof,uint16_t left,uint16_t right,float base=0){auto a=project({p.x-sx,p.y-sy},z+base),b=project({p.x+sx,p.y-sy},z+base),c=project({p.x+sx,p.y+sy},z+base),d=project({p.x-sx,p.y+sy},z+base);r.poly({d,c,project({p.x+sx,p.y+sy},base),project({p.x-sx,p.y+sy},base)},left);r.poly({b,c,project({p.x+sx,p.y+sy},base),project({p.x+sx,p.y-sy},base)},right);r.poly({a,b,c,d},roof);r.line(a.x,a.y,b.x,b.y,mix(roof,cream,4));r.line(a.x,a.y,d.x,d.y,mix(roof,cream,3));}
 void wire(Raster& r,Vec a,Vec b,float z,uint16_t c,int thick=1){auto aa=project(a,z),bb=project(b,z);r.line(aa.x,aa.y,bb.x,bb.y,c,thick);}
#include "environment.inc"
 [[gnu::noinline]] void building_art(Raster& r,const Entity& a){
  Vec p=a.p;bool enemy=a.team==1;auto team=enemy?red:cyan;float size=a.kind==turret?.52f:a.kind==power?.75f:1.f;
  if(a.progress>0&&a.kind>=power&&a.kind<=turret){unsigned stage=std::min(3u,unsigned(std::max(0.f,1-a.progress/specs[a.kind].build)*4)),id=art::construction_first+(a.kind-power)*4+stage;auto q=project(p);ShadowArt::base(r,id,std::lround(q.x),std::lround(q.y),zoom,true);Sprites::anchor(r,id,std::lround(q.x),std::lround(q.y),zoom);float cycle=std::fmod(clock*1.7f,1.f);if(stage&&cycle<.25f)draw_fx(r,0,q.x+18*zoom,q.y-14*zoom,cycle*4,.2f*zoom);int yy=q.y-70*zoom;r.rect(q.x-21,yy,42,5,navy);r.rect(q.x-20,yy+1,40*(1-a.progress/specs[a.kind].build),3,gold);return;}

  unsigned base_id=Sprites::building_frame(a.kind);auto contact=project(p);ShadowArt::base(r,base_id,std::lround(contact.x),std::lround(contact.y),zoom,true);
  unsigned id=Sprites::building_frame(a.kind);auto q=project(p);unsigned variant=(enemy?1:0)+(a.flash>0?2:0);if(a.kind==turret)TurretArt::draw(r,std::lround(q.x),std::lround(q.y),zoom,CombatArt::facing(a.dir),a.cool>specs[turret].reload-.14f,variant);else Sprites::anchor(r,id,std::lround(q.x),std::lround(q.y),zoom,variant);
  if(a.kind==power){for(int i=0;i<2;++i){auto vent=project({p.x-.28f+i*.55f,p.y},37);float age=std::fmod(clock*.6f+i*.5f,1.f);r.ellipse(vent.x+age*5*zoom,vent.y-age*18*zoom,(1+age*3)*zoom,(1+age*2)*zoom,mix(rgb(109,129,119),navy,int(age*9)));}}
  if(a.kind==supply){auto& frame=art::frames[id];int yy=q.y-int((frame.oy-frame.by)*zoom);r.label({int(q.x)-46,yy-20,92,18},"补给 +280",cyan,11);if(a.progress>0){r.rect(q.x-18,yy+2,36,4,navy);r.rect(q.x-17,yy+3,34*std::min(1.f,a.progress/5),2,cyan);}return;}
  if(a.kind==relay&&a.team==2&&a.stuck>0){r.rect(q.x-18,q.y-55*zoom,36,4,navy);r.rect(q.x-17,q.y-55*zoom+1,34*std::min(1.f,a.stuck/12),2,gold);}
  if(a.kind==relay){auto& frame=art::frames[id];r.dot(q.x,q.y-(frame.oy-frame.by)*zoom,a.team==2?gold:cyan);}
  if(a.hp<specs[a.kind].hp*.35f){auto smoke=project(p,25);for(int i=0;i<3;++i){float age=std::fmod(clock*.65f+i*.34f,1.f);r.ellipse(smoke.x+age*9*zoom,smoke.y-age*25*zoom,(2+age*5)*zoom,(3+age*4)*zoom,mix(rgb(82,82,70),navy,int(age*9)));}}
 }
 void combined_unit_art(Raster& r,const Entity& a,int identity){
  unsigned frame=Sprites::unit_frame(a.kind,a.dir);
  unsigned variant=(a.team==1?1:0)+(a.flash>0?2:0);
  auto ground=project(a.p);
  if(a.kind==rotorcraft){
   r.ellipse(ground.x-7*zoom,ground.y+3*zoom,16*zoom,7*zoom,edge);
   auto body=project(a.p,craft_altitude);
   Sprites::anchor(r,frame,std::lround(body.x),std::lround(body.y),zoom,variant);
   unsigned facing=CombatArt::facing(a.dir);
   constexpr Pt mast[]={{-1,-16},{0,-21},{1,-16},{0,-12},{0,-16},{0,-24},{1,-16},{0,-12}};
   float x=body.x+mast[facing].x*zoom,y=body.y+mast[facing].y*zoom;
   float angle=std::floor(clock*28)*.78f;
   for(float axis:{angle,angle+1.5707963f}){
    float dx=std::cos(axis)*25*zoom,dy=std::sin(axis)*11*zoom;
    r.line(x-dx,y-dy,x+dx,y+dy,rgb(122,151,143));
   }
   r.dot(x,y,cream);
  }else{
   const auto& pose=locomotion.get(identity);
   if(naval_kind(a.kind)&&pose.weight>.12f){
    auto wake=project({a.p.x-std::cos(a.dir)*.8f,a.p.y-std::sin(a.dir)*.8f});
    r.ring(wake.x,wake.y,12*zoom,4*zoom,rgb(102,167,162));
   }
   if(a.kind==commander)r.ellipse(ground.x,ground.y,7*zoom,3*zoom,edge);
   float bob=a.kind==commander&&pose.weight>.12f?(int(pose.phase*8)&1)*zoom:0;
   Sprites::anchor(r,frame,std::lround(ground.x),std::lround(ground.y-bob),zoom,variant);
  }
  auto track=project(a.p,a.kind==rotorcraft?craft_altitude+36:a.kind==commander?41:39);
  auto team=a.kind==commander?gold:a.team==1?red:cyan;
  if(a.kind==commander)r.text(track.x,track.y-15*zoom,"林岚",gold,12,1);
  if(a.selected||a.hp<specs[a.kind].hp){
   r.rect(track.x-14,track.y,28,4,navy);
   r.rect(track.x-13,track.y+1,26*std::clamp(a.hp/specs[a.kind].hp,0.f,1.f),2,team);
  }
  if(a.selected)r.ring(ground.x,ground.y,17*zoom,7*zoom,team);
 }
 [[gnu::noinline]] void unit_art(Raster& r,const Entity& a,int identity=-1,bool cover=false){
  if(naval_kind(a.kind)||a.kind==rotorcraft||a.kind==commander){combined_unit_art(r,a,identity);return;}
  auto p=project(a.p);int x=std::lround(p.x),y=std::lround(p.y);bool infantry=a.kind<=rocket;auto team=a.team==1?red:cyan;
  if(a.selected){
   int rx=std::max(6,int((infantry?8:18)*zoom)),ry=std::max(3,int((infantry?4:8)*zoom)),arm=std::max(2,int(4*zoom));
   for(int side:{-1,1})for(int end:{-1,1}){int xx=x+side*rx,yy=y+end*ry;r.line(xx,yy,xx-side*arm,yy,team);r.line(xx,yy,xx,yy-end*std::max(2,arm/2),team);}
   if(a.moving){auto g=project(a.goal);if(onscreen(g)){int d=std::max(2,int(3*zoom));r.line(g.x-d,g.y,g.x+d,g.y,mix(team,navy,5));r.line(g.x,g.y-d,g.x,g.y+d,mix(team,navy,5));}}
  }
  if(identity>=0&&locomotion.get(identity).weight>.1f&&a.kind>=tank)for(int j=0;j<2;++j){float age=std::fmod(clock*1.5f+j*.5f,1.f);Vec dust={a.p.x-std::cos(a.dir)*(.5f+age*.8f),a.p.y-std::sin(a.dir)*(.5f+age*.8f)};auto d=project(dust,2+age*2);r.ellipse(d.x,d.y,(2+age*4)*zoom,(1+age*2)*zoom,mix(rgb(124,119,84),rgb(64,77,57),int(age*13)));}
  const auto& motion=locomotion.get(identity);bool firing=a.kind<harvester&&a.cool>specs[a.kind].reload-.12f;
  unsigned pose=a.kind==harvester?(a.cargo>0):(!firing&&motion.weight>0?(unsigned(motion.phase*2)&1):0);unsigned facing=CombatArt::facing(a.dir);unsigned id=Sprites::unit_frame(a.kind,a.dir,infantry?0:pose);unsigned variant=(a.team==1?1:0)+(a.flash>0?2:0);
  if(infantry){ShadowArt::infantry(r,a.kind,facing,x,y,zoom,motion.phase,firing?0:motion.weight);CombatArt::infantry(r,a.kind,facing,x,y,zoom,motion.phase,firing?0:motion.weight,variant);}else{ShadowArt::base(r,id,x,y,zoom);Sprites::anchor(r,id,x,y,zoom,variant);}
  if(a.kind==harvester&&a.team==2){r.text(x,y-48*zoom,"车队",gold,11,1);r.ring(x,y+5*zoom,18*zoom,8*zoom,gold);}
  if(cover){int yy=y-42*zoom,xx=x+12*zoom;r.line(xx-3,yy-2,xx,yy+2,rgb(147,199,125),2);r.line(xx,yy+2,xx+3,yy-2,rgb(147,199,125),2);}
  unsigned rank=veteran_rank(a);if(rank){int yy=y-(infantry?46:53)*zoom;for(unsigned j=0;j<rank;++j){int xx=x+(int(j)*6-int(rank-1)*3);r.line(xx-2,yy+3,xx,yy,gold);r.line(xx,yy,xx+2,yy+3,gold);}}
  if(a.hp<specs[a.kind].hp*.3f&&!infantry){auto smoke=project(a.p,15);r.ellipse(smoke.x+3*zoom,smoke.y-(5+int(clock*8)%9)*zoom,3*zoom,4*zoom,edge);}
  if(a.selected||a.hp<specs[a.kind].hp){int ww=infantry?16:27,yy=y-(infantry?40:47)*zoom;r.rect(x-ww/2-1,yy-1,ww+2,4,navy);r.rect(x-ww/2,yy,ww*std::clamp(a.hp/specs[a.kind].hp,0.f,1.f),2,a.hp>specs[a.kind].hp*.35f?team:red);}
 }
 [[gnu::noinline]] void ground_details(Raster& r,const World& w,Vec p,unsigned hash,uint16_t col){
  int x=int(p.x),y=int(p.y),c=y*N+x;auto q=project(p);
  if(w.terrain[c]==2){
   uint16_t foam=mix(col,rgb(155,179,156),6);
   if(x>0&&w.terrain[c-1]!=2)wire(r,{p.x-.48f,p.y-.5f},{p.x-.48f,p.y+.5f},1,foam);
   if(x<w.side-1&&w.terrain[c+1]!=2)wire(r,{p.x+.48f,p.y-.5f},{p.x+.48f,p.y+.5f},1,foam);
   if(y>0&&w.terrain[c-N]!=2)wire(r,{p.x-.5f,p.y-.48f},{p.x+.5f,p.y-.48f},1,foam);
   if(y<w.side-1&&w.terrain[c+N]!=2)wire(r,{p.x-.5f,p.y+.48f},{p.x+.5f,p.y+.48f},1,foam);
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
 void rallies(Raster& r,const World& w){if(w.chapter>=3&&!w.extra.mode){for(auto& node:w.e)if(node.hp>0&&node.kind==relay&&node.team==2&&!w.fog[cell(node.p)]){auto q=project(node.p);if(onscreen(q,24)){r.ring(q.x,q.y,12*zoom,6*zoom,gold);r.text(q.x,q.y-22*zoom,"通信节点",gold,11,1);}}if(w.chapter==4){auto q=project(w.convoy_stop(3));if(onscreen(q,60)){ShadowArt::base(r,art::buildings_first+7,q.x,q.y,zoom*.45f,true);Sprites::anchor(r,art::buildings_first+7,q.x,q.y,zoom*.45f);r.text(q.x,q.y-42*zoom,"撤离点",gold,11,1);}}}for(auto& a:w.e)if(a.hp>0&&a.team==0&&a.selected&&a.moving&&production_building(a.kind)){
  for(int i=0;i<10;++i){float t=i/10.f;Vec p={a.p.x+(a.goal.x-a.p.x)*t,a.p.y+(a.goal.y-a.p.y)*t};auto q=project(p,1);r.rect(q.x,q.y,2,1,cyan);}
  auto flag=project(a.goal,2);r.ring(flag.x,flag.y,9*zoom,4*zoom,cyan);r.line(flag.x,flag.y,flag.x,flag.y-18*zoom,cream);r.poly({Pt{flag.x,flag.y-18*zoom},Pt{flag.x+12*zoom,flag.y-15*zoom},Pt{flag.x,flag.y-10*zoom},Pt{flag.x,flag.y-18*zoom}},gold);
 }}
 [[gnu::noinline]] void draw_ground(Raster& r,const World& w){
  for(int y=top;y<bottom;y+=4)r.rect(0,y,width,std::min(4,bottom-y),mix(rgb(17,31,36),rgb(34,47,40),(y-top)*6/std::max(1,bottom-top)));
  material.material=w.material();
  material.prepare(width,top,bottom,camx,camy,zoom);material.paint(r,width,top,bottom,camx,camy,zoom,w.side);
  auto tiles=visible_tiles(w);for(int d=tiles.left+tiles.first;d<=tiles.right+tiles.last;++d)for(int x=std::max(tiles.left,d-tiles.last);x<=std::min(tiles.right,d-tiles.first);++x){int y=d-x;if(y<0||y>=w.side)continue;int c=y*N+x;Vec p={x+.5f,y+.5f};auto q=project(p);if(!onscreen(q,42))continue;unsigned hash=x*8749+y*6329+x*y*17;
   uint16_t color=!w.seen[c]?rgb(20,32,34):w.terrain[c]==water?rgb(21,54,59):w.paved(p)?rgb(37,50,46):rgb(48,57,42);if(!w.seen[c]){quad(r,p,.51f,.51f,0,color);continue;}if(!w.fog[c])material.dim(r,q,zoom);
   auto type=w.terrain[c];
   if(type==water||type==road){
    auto road_at=[&](int xx,int yy){return xx>=0&&xx<w.side&&yy>=0&&yy<w.side&&w.paved({xx+.5f,yy+.5f});};
    // Look beyond the adjacent lane for the long axis of a two-cell road.
    unsigned horizontal=road_at(x-2,y)+road_at(x+2,y),vertical=road_at(x,y-2)+road_at(x,y+2);
    unsigned tile=type==water?4+(int(clock*2)&1):vertical>horizontal?3:2;
    Sprites::blit(r,art::terrain_first+tile,std::lround(q.x-20*zoom),std::lround(q.y-10*zoom),zoom,zoom,w.fog[c]?0:4);
   }
   land_details(r,w,p,hash);
   if(type==bridge){
    unsigned tile=(x==0||w.terrain[c-1]!=bridge||x==w.side-1||w.terrain[c+1]!=bridge)?3:0;Sprites::anchor(r,art::bridge_first+tile,std::lround(q.x),std::lround(q.y),zoom,w.fog[c]?0:4);
    if(y==0||w.terrain[c-N]!=bridge){auto rail=project({p.x,p.y-.48f});Sprites::anchor(r,art::bridge_first+1,std::lround(rail.x),std::lround(rail.y),zoom*.55f,w.fog[c]?0:4);}
   }
   else if(!w.extra.map_rules&&w.chapter==1&&x>=13&&x<=15&&(y==5||y==8||y==16||y==19||y==23||y==25))ground_details(r,w,p,hash,color);
   if(!w.extra.map_rules&&w.extra.mode&&w.defense_bridge(x,y)){quad(r,p,.5f,.5f,1,rgb(102,115,105));wire(r,{p.x-.5f,p.y-.45f},{p.x+.5f,p.y-.45f},4,rgb(153,164,139));wire(r,{p.x-.5f,p.y+.45f},{p.x+.5f,p.y+.45f},4,rgb(80,101,93));}

  }
 }
 static unsigned shot_facing(const Shot& s){return CombatArt::facing(std::atan2(s.dest.y-s.from.y,s.dest.x-s.from.x));}
 static bool shot_behind(const Shot& s){return s.kind<=artillery&&art::muzzles[s.kind][shot_facing(s)].behind;}
 Pt muzzle(const Shot& s)const{if(s.kind==turret){auto origin=project(s.from),point=TurretArt::muzzle(shot_facing(s),true);return {origin.x+point.x*zoom,origin.y+point.y*zoom};}auto origin=project(s.from);if(s.kind>turret)return project(s.from,s.kind==rotorcraft?craft_altitude+14:18);return CombatArt::socket(s.kind,shot_facing(s),std::lround(origin.x),std::lround(origin.y),zoom);}
 [[gnu::noinline]] void draw_shot(Raster& r,const Shot& s,const World& w){
  int victim=shot_target(s);
  auto start=muzzle(s),end=project(s.dest,victim>=0&&victim<MAX&&w.e[victim].kind==rotorcraft?craft_altitude+13:16);unsigned facing=shot_facing(s);float phase=std::clamp(s.t/std::max(.001f,s.duration),0.f,1.f);
  auto point=[&](float t){return CombatArt::trajectory(start,end,t,s.kind==artillery,zoom,facing);};auto head=point(phase);
  if(s.kind==artillery){auto shadow=project(s.p);r.ellipse(shadow.x,shadow.y,3*zoom,2*zoom,navy);}
  for(int i=3;i>=0;--i){float t=std::max(0.f,phase-i*.065f),next=std::max(0.f,phase-(i+1)*.065f);auto a=point(t),b=point(next);r.line(a.x,a.y,b.x,b.y,i?mix(gold,red,i*3):cream,std::max(1,int((s.kind==rifle?1:2)*zoom)));if(s.kind==rocket&&i&&t>0)r.ellipse(b.x,b.y,std::max(1,int(2*zoom)),std::max(1,int(2*zoom)),edge);}
  r.dot(head.x,head.y,cream);
  if(s.t<.08f){int radius=std::max(1,int((s.kind==rifle?2:4)*zoom));r.line(start.x-radius,start.y,start.x+radius,start.y,gold);r.line(start.x,start.y-radius,start.x,start.y+radius,cream);}
 }
 void draw_order_tag(Raster& r,const World& w,int id){
  auto& a=w.e[id];
  if(!a.selected||(!w.squads[id]&&a.target!=retreat_order&&escort_id(a)<0))return;
  auto p=project(a.p,a.kind==rotorcraft?craft_altitude+42:32);
  const char* order=a.target==retreat_order?"撤":escort_id(a)>=0?"护":"";
  char tag[5]={char('0'+w.squads[id])};
  for(int j=0;order[j];++j)tag[j+1]=order[j];
  r.text(p.x,p.y,w.squads[id]?tag:order,gold,12,1);
 }
 [[gnu::noinline]] void draw(Raster& r,const World& w){
  r.clip={0,top,width,bottom-top};locomotion.update(w);water_visible=false;auto tiles=visible_tiles(w);for(int y=tiles.first;y<=tiles.last&&!water_visible;++y)for(int x=tiles.left;x<=tiles.right;++x){int c=y*N+x;if(w.terrain[c]==water&&w.seen[c]&&onscreen(project(center(c)),42)){water_visible=true;break;}}
  if(cache_ground)ground.draw(r,w,width,top,bottom,camx,camy,zoom,water_visible?(int(clock*2)&1):0,[&](Raster& backing){draw_ground(backing,w);});else draw_ground(r,w);
  rallies(r,w);
  for(const auto& shot:w.shots)if(shot.alive&&shot_behind(shot)&&w.fog[cell(shot.p)])draw_shot(r,shot,w);
  // Painter sort props and entities by their ground plane depth.
  std::array<int,MAX> order{};int num=0;for(int i=0;i<MAX;++i)if(deployed(w.e[i])&&(w.e[i].team==0||w.fog[cell(w.e[i].p)]))order[num++]=i;std::sort(order.begin(),order.begin()+num,[&](int a,int b){return w.e[a].p.x+w.e[a].p.y<w.e[b].p.x+w.e[b].p.y;});
  for(int d=0,ii=0;d<w.side*2;++d){for(int x=std::max(1,tiles.left);x<=std::min(w.side-2,tiles.right);++x){int y=d-x;if(y<1||y>=w.side-1)continue;int c=y*N+x;if(!w.fog[c])continue;Vec p={x+.5f,y+.5f};if(!onscreen(project(p),60))continue;unsigned hash=x*8749+y*6329+x*y*17;tall_environment(r,w,p,hash);}
   while(ii<num&&int(w.e[order[ii]].p.x+w.e[order[ii]].p.y)<=d){auto& a=w.e[order[ii++]];if(a.kind==rotorcraft||!onscreen(project(a.p),110))continue;if(!unit_kind(a.kind)){building_art(r,a);if(w.chapter==5&&a.kind==hq&&a.team==1&&w.mission.stage<2){auto lock=project(a.p,68);r.text(lock.x,lock.y,"屏障",cyan,12,1);}if(w.extra.mode&&a.kind==hq&&a.team==0){auto beacon=project(a.p,58);for(int lamp=0;lamp<3;++lamp)r.rect(beacon.x+(lamp-1)*3*zoom,beacon.y-(1+((int(clock*4)+lamp)%3))*2*zoom,std::max(1,int(2*zoom)),std::max(1,int((2+((int(clock*4)+lamp)%3)*2)*zoom)),cyan);}}else{
    int id=order[ii-1];
    unit_art(r,a,id,w.in_cover(a));
    draw_order_tag(r,w,id);
    if(a.kind==landing_craft){
     int load=w.transport_load(id);
     if(a.selected||load){
      auto p=project(a.p,55);
      char seats[12];std::snprintf(seats,sizeof seats,"%d/%d",load,transport_capacity);
      r.text(p.x,p.y,seats,gold,12,1);
     }
    }
   }if(!unit_kind(a.kind)&&(a.selected||a.hp<specs[a.kind].hp)){auto p=project(a.p,48);r.rect(p.x-18,p.y,36,3,navy);r.rect(p.x-17,p.y+1,34*std::clamp(a.hp/specs[a.kind].hp,0.f,1.f),1,a.team==1?red:cyan);}}
   // Near rails are sorted after units behind their ground-plane edge.
   for(int x=std::max(1,tiles.left);x<=std::min(w.side-2,tiles.right);++x){int y=d-2-x;if(y<1||y>=w.side-1)continue;int c=y*N+x;if(w.terrain[c]!=bridge||w.terrain[c+N]==bridge||!w.fog[c])continue;auto q=project({x+.5f,y+.98f});if(onscreen(q,30))Sprites::anchor(r,art::bridge_first+2,std::lround(q.x),std::lround(q.y),zoom*.55f);}
  }
  for(int n=0;n<num;++n){
   int id=order[n];auto& a=w.e[id];
   if(a.kind==rotorcraft&&onscreen(project(a.p,craft_altitude),70)){
    unit_art(r,a,id);draw_order_tag(r,w,id);
   }
  }
  for(const auto& shot:w.shots)if(shot.alive&&!shot_behind(shot)&&w.fog[cell(shot.p)])draw_shot(r,shot,w);
  for(auto& b:w.blasts)if(b.alive&&w.fog[cell(b.p)]){
   auto p=project(b.p,b.type==5?10:b.age*10);
   if(b.type==5)continue; // Legacy save flash; current flashes share the projectile socket.
   auto origin=project(b.p);
   if(b.type==3){draw_fx(r,3,origin.x,origin.y,b.age/.7f,zoom*.9f);continue;}
   if(b.type==4||b.type==6){float f=b.age/(b.type==6?1.2f:1.f);draw_fx(r,4,origin.x,origin.y,f,zoom);if(b.type==6)r.text(p.x,p.y-20,"+280",gold,16,1);else {r.line(p.x-6,p.y-10,p.x,p.y-17,gold,2);r.line(p.x,p.y-17,p.x+6,p.y-10,gold,2);}continue;}
   float life=b.type==2?2.2f:b.type==1?1.1f:.65f;draw_fx(r,b.type==2?2:b.type==1?1:0,origin.x,origin.y,b.age/life,zoom);
  }
  r.clip={0,0,r.w,r.h};
 }
};
}
