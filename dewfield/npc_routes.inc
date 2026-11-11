// SPDX-License-Identifier: MIT
// Included inside farm. Daily paths are built once, never searched per frame.
namespace npc_routes {
struct Node { uint32_t ms;uint16_t x,y;uint8_t region; };
struct DayRoute {Node nodes[768];uint16_t count=0;};
static DayRoute routes[NpcCount];
static uint32_t route_key=~0u;
struct Stop {int depart;Region region;int x,y;};
inline void add(DayRoute&r,uint32_t ms,Region region,int x,int y) {
  if(r.count>=768)return;
  r.nodes[r.count++]={ms,uint16_t(x*16+8),uint16_t(y*16+12),uint8_t(region)};
}
inline bool path(const State&s,int sx,int sy,int gx,int gy,uint16_t *steps,int &length,bool roads) {
  if(sx==gx&&sy==gy){length=0;return true;}
  int16_t parent[MapW*MapH];uint16_t queue[MapW*MapH];
  std::fill(parent,parent+MapW*MapH,int16_t(-1));
  int start=sy*MapW+sx,goal=gy*MapW+gx,head=0,tail=0;
  if(start<0||start>=MapW*MapH||goal<0||goal>=MapW*MapH)return false;
  parent[start]=start;queue[tail++]=start;
  while(head<tail&&parent[goal]<0) {
    int at=queue[head++],x=at%MapW,y=at/MapW;
    for(auto d:{Point{0,1},Point{1,0},Point{-1,0},Point{0,-1}}) {
      int xx=x+d.x,yy=y+d.y;if(xx<0||yy<0||xx>=MapW||yy>=MapH)continue;
      int n=yy*MapW+xx;if(parent[n]>=0||!walkable(s,xx,yy))continue;
      if(roads&&!indoors(s.region)&&!road_tile(s.region,xx,yy)&&std::abs(xx-sx)+std::abs(yy-sy)>2&&std::abs(xx-gx)+std::abs(yy-gy)>2)continue;
      parent[n]=at;queue[tail++]=n;
    }
  }
  if(parent[goal]<0)return false;
  length=0;for(int at=goal;at!=start;at=parent[at])steps[length++]=at;
  std::reverse(steps,steps+length);return true;
}
inline bool walk(DayRoute&r,State&probe,int gx,int gy,uint32_t &ms) {
  auto &last=r.nodes[r.count-1];uint16_t steps[MapW*MapH];int length=0;
  if(!path(probe,last.x/16,last.y/16,gx,gy,steps,length,true)&&!path(probe,last.x/16,last.y/16,gx,gy,steps,length,false))return false;
  if(length+r.count>=768)return false;
  for(int i=0;i<length;++i){ms+=400;add(r,ms,Region(probe.region),steps[i]%MapW,steps[i]/MapW);}
  return true;
}
inline void trip(DayRoute&r,State&probe,Stop stop,bool ferry=false) {
  auto last=r.nodes[r.count-1];uint32_t ms=std::max(uint32_t(stop.depart*500),last.ms);
  add(r,ms,Region(last.region),last.x/16,last.y/16);probe.region=last.region;
  for(int hop=0;probe.region!=unsigned(stop.region)&&hop<RegionCount;++hop) {
    int id=next_portal(probe,stop.region);if(id<0)return;const auto &p=Portals[id];
    if(probe.story<3&&ferry&&((p.from==Region::Forest&&p.to==Region::Lake)||(p.from==Region::Lake&&p.to==Region::Forest))) {
      // Cheng's small skiff reaches the dry east bank while the public bridge
      // is broken. His route must never interpolate through blocked water.
      if(!walk(r,probe,p.from==Region::Forest?7:p.x,p.y,ms))return;
      ms+=1000;probe.region=uint8_t(p.to);add(r,ms,p.to,p.to==Region::Forest?7:p.spawn_x,p.spawn_y);continue;
    }
    if(p.bridge&&probe.story<3)return;
    if(!walk(r,probe,p.x,p.y,ms))return;
    ms+=400;probe.region=uint8_t(p.to);add(r,ms,p.to,p.spawn_x,p.spawn_y);
  }
  walk(r,probe,stop.x,stop.y,ms);
}
inline void build(const State&s) {
  for(int id=0;id<NpcCount;++id) {
    auto &r=routes[id];r.count=0;State probe=s;
    add(r,0,NpcHomes[id],NpcRest[id].x,NpcRest[id].y);probe.region=unsigned(NpcHomes[id]);
    auto go=[&](int time,Region region,int x,int y){trip(r,probe,{time,region,x,y},id==2);};
    bool wet=s.rain||s.weather==unsigned(Weather::Snow);
    bool gathering=s.story>=5;
    if(id==0) {
      if((s.day-1)%7!=2) {go(465,Region::Town,7,8);go(520,Region::SeedShop,14,9);go(1020,Region::Town,17,12);}
      else if(!wet){go(480,Region::Orchard,15,9);go(840,Region::Town,11,12);}
    } else if(id==1) {
      if(!wet){go(450,Region::Town,20,8);go(660,Region::Forest,17,12);if(s.friend_chapter[1]>=2&&s.story>=3)go(840,Region::Lake,24,11);go(930,Region::Town,20,8);}
      else go(480,Region::Lodge,16,12);
    } else if(id==2) {
      if(!wet){go(450,s.story>=3?Region::Lake:Region::Forest,s.story>=3?24:7,12);go(930,Region::Town,19,12);}
    } else {
      if(!wet){go(450,Region::Farm,22,7);go(530,Region::Town,14,12);go(780,Region::Orchard,8,10);go(930,Region::Town,18,8);}
    }
    if(season_day(s.day)==14)go(1020,(wet||season_of(s.day)==3)?Region::Lodge:Region::Town,(wet||season_of(s.day)==3)?10+id*3:12+id*2,(wet||season_of(s.day)==3)?14:13);
    else if(gathering&&!wet)go(1020,Region::Town,12+id*2,13);
    // Heading home is a visible walk; each villager has their own front door.
    go(1140,NpcHomes[id],NpcRest[id].x,NpcRest[id].y);
  }
}
}
NpcPose npc_pose(const State&s,int id,int subminute_ms) {
  id=std::clamp(id,0,NpcCount-1);
  uint32_t key=uint32_t(s.day)|(uint32_t(s.story)<<16)|(uint32_t(s.rain)<<20)|(uint32_t(s.weather)<<21)|(uint32_t(s.friend_chapter[1]>=2)<<24);
  if(npc_routes::route_key!=key){npc_routes::build(s);npc_routes::route_key=key;}
  auto &r=npc_routes::routes[id];uint32_t ms=uint32_t(s.minute)*500+std::clamp(subminute_ms,0,499);
  int lo=0,hi=r.count;while(lo+1<hi){int mid=(lo+hi)/2;if(r.nodes[mid].ms<=ms)lo=mid;else hi=mid;}
  auto &a=r.nodes[lo];
  if(lo+1>=r.count)return {Region(a.region),a.x,a.y,2,false,0};
  auto &b=r.nodes[lo+1];
  if(a.region!=b.region||a.x==b.x&&a.y==b.y)return {Region(a.region),a.x,a.y,2,false,0};
  int duration=std::max(1,int(b.ms-a.ms)),t=std::clamp(int(ms-a.ms),0,duration);
  int x=a.x+(int(b.x)-a.x)*t/duration,y=a.y+(int(b.y)-a.y)*t/duration;
  int dir=b.x!=a.x?(b.x>a.x?1:3):(b.y>a.y?2:0);
  int distance=lo*16+std::abs(x-int(a.x))+std::abs(y-int(a.y));
  return {Region(a.region),x,y,dir,true,walk_frame(distance)};
}
const char *npc_activity(const State&s,int id,bool en) {
  auto p=npc_pose(s,id);
  if(p.walking)return en?"On the way":"正在沿路行走";
  if(season_day(s.day)==14&&s.minute>=1020&&s.minute<1140)return en?"Seasonal gathering":"季节聚会";
  if(unsigned(p.region)>=unsigned(Region::SeedHome))return en?"At their own home":"在自己的家中休息";
  if(p.region==Region::Lodge)return en?(s.minute>=1140?"At home":"Resting at the inn"):(s.minute>=1140?"回家休息":"会馆避雨、歇脚");
  if(p.region==Region::SeedShop)return en?"Tending the seed shop":"在种子铺上班";
  if(p.region==Region::Forest&&id==1)return en?"Selecting timber":"林间挑选木料";
  if(p.region==Region::Lake||p.region==Region::Forest&&id==2)return en?"Fishing by the water":"在水边钓鱼";
  if(p.region==Region::Orchard)return en?"Orchard walk":"果园散步";
  return en?(s.minute>=1020?"Evening with friends":"In the village"):(s.minute>=1020?"和朋友共度傍晚":"在镇上忙碌、散步");
}
