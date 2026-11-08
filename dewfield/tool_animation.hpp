// SPDX-License-Identifier: MIT
#pragma once
// Included inside art. All rotations and mirror pixels are compiled offline.
namespace tool_anim {
inline int duration(int tool,int grade=0) {
  constexpr int ms[]={560,420,640,360,560,540,680,380};
  return ms[tool%8]-((tool==4||tool==5)?std::clamp(grade,0,2)*45:0);
}
inline constexpr int Impact=144;
inline int surface_offset(int tool){return tool==4?14:tool==5?8:tool==7?10:6;}
inline int impact_remaining(int tool,int grade=0) {return duration(tool,grade)*(256-Impact)/256;}
inline const sprites::HeldFrame &held_frame(int tool,int grade,int angle,bool mirror=false) {
  const sprites::HeldFrame *p=nullptr;
  if(tool==7)return (mirror?sprites::sword_mirrors:sprites::sword_frames)[angle&15];
  if(tool==4) {
    const sprites::HeldFrame *normal[]={sprites::held_axe0_frames,sprites::held_axe1_frames,sprites::held_axe2_frames};
    const sprites::HeldFrame *flipped[]={sprites::held_axe0_mirrors,sprites::held_axe1_mirrors,sprites::held_axe2_mirrors};
    p=(mirror?flipped:normal)[std::clamp(grade,0,2)];
  } else if(tool==5) {
    const sprites::HeldFrame *normal[]={sprites::held_pick0_frames,sprites::held_pick1_frames,sprites::held_pick2_frames};
    const sprites::HeldFrame *flipped[]={sprites::held_pick0_mirrors,sprites::held_pick1_mirrors,sprites::held_pick2_mirrors};
    p=(mirror?flipped:normal)[std::clamp(grade,0,2)];
  } else if(tool==2)p=mirror?sprites::held_can0_mirrors:sprites::held_can0_frames;
  else if(tool==6)p=mirror?sprites::held_rod0_mirrors:sprites::held_rod0_frames;
  else p=mirror?sprites::held_hoe0_mirrors:sprites::held_hoe0_frames;
  return p[angle&15];
}
inline const sprites::Sprite &frame(int tool,int grade,int angle,bool mirror=false) {return *held_frame(tool,grade,angle,mirror).sprite;}
struct Target {int x=0,y=0;bool valid=false;};
struct Motion {int x,y,angle,lean_x,lean_y,progress;bool two_hands,mirrored;};
inline int mix(int a,int b,int t){return a+(b-a)*std::clamp(t,0,256)/256;}
inline int turn(int a,int b,int t) {int delta=((b-a+24)&15)-8;return (a+delta*std::clamp(t,0,256)/256)&15;}
inline Motion motion(int dir,int tool,int remaining,int grade=0,int target_x=0,int target_y=0,bool targeted=false) {
  int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0),side=dir==3?-1:1;
  int total=duration(tool,grade),p=remaining?std::clamp((total-remaining)*256/total,0,255):0;
  constexpr int idle[]={15,2,6,14};
  constexpr int axe_idle[]={13,15,5,1};
  constexpr int sword_idle[]={15,2,6,14};
  int resting=tool==4?axe_idle[dir&3]:tool==7?sword_idle[dir&3]:idle[dir&3];
  Motion m{dir==0?-5:side*5,dir==2?-12:-14,resting,0,0,p,false,dir==3};
  if(!remaining)return m;
  if(tool==7) {
    int strike=dir*4,arc=dir==3?-3:3;
    const auto &head=held_frame(tool,grade,strike,m.mirrored);
    int hx=(targeted?target_x:dx*16)-head.tip_x,hy=(targeted?target_y:dy*16-10)-head.tip_y;
    if(p<100){int t=p*256/100;m.y=mix(m.y,-22,t);m.angle=turn(resting,strike-arc,t);}
    else if(p<Impact){int t=(p-100)*256/(Impact-100);m.x=mix(m.x,hx,t);m.y=mix(-22,hy,t);m.angle=turn(strike-arc,strike,t);}
    else if(p<190){int t=(p-Impact)*256/(190-Impact);m.x=hx;m.y=hy;m.angle=turn(strike,strike+arc,t);}
    else {int t=(p-190)*256/66;m.x=mix(hx,m.x,t);m.y=mix(hy,m.y,t);m.angle=turn(strike+arc,resting,t);}
  } else if(tool==0||tool==4||tool==5) {
    constexpr int axe_strike[]={12,0,4,0};
    m.two_hands=true;int strike=tool==4?axe_strike[dir&3]:dir*4;
    const auto &head=held_frame(tool,grade,strike,m.mirrored);
    int tx=targeted?target_x:dx*16,ty=targeted?target_y:dy*16-surface_offset(tool);
    int hitx=tx-head.tip_x,hity=ty-head.tip_y;
    int wx=dir==0?-3:side*2,wy=-25,wa=dir==0?14:dir==2?1:dir==1?15:1;
    if(p<64) {int t=p*4;m.x=mix(m.x,wx,t);m.y=mix(m.y,wy,t);m.angle=turn(m.angle,wa,t);}
    else if(p<100){m.x=wx;m.y=wy;m.angle=wa;}
    else if(p<124) {
      int t=(p-100)*256/24;
      m.x=mix(wx,hitx-dx*8,t);m.y=mix(wy,hity-dy*8,t);m.angle=turn(wa,strike,t);
    } else if(p<Impact) {
      int t=(p-124)*256/(Impact-124);
      m.x=mix(hitx-dx*8,hitx,t);m.y=mix(hity-dy*8,hity,t);m.angle=strike;
      m.lean_x=dx*t/256;m.lean_y=t>180?1:0;
    } else if(p<166) {m.x=hitx;m.y=hity;m.angle=strike;m.lean_x=dx;m.lean_y=1;}
    else {int t=(p-166)*256/(256-166);m.x=mix(hitx,m.x,t);m.y=mix(hity,m.y,t);m.angle=turn(strike,m.angle,t);}
  } else if(tool==2) {
    int t=p<80?p*256/80:p<218?256:(256-p)*256/38;
    m.angle=turn(resting,dir*4,t);m.x=mix(m.x,side*2,t);m.y=mix(m.y,-10,t);
    m.lean_x=p>70&&p<220?dx:0;m.lean_y=p>70&&p<220?1:0;
  } else if(tool==6) {
    int t=p<144?p*256/144:(256-p)*256/112;
    m.x=mix(m.x,dx*7+side*2,t);m.y=mix(m.y,-23+dy*5,t);
    m.angle=turn(idle[dir&3],dir*4,t);m.two_hands=true;
  } else {
    int reach=p<90?p*7/90:p<190?7:(256-p)*7/66;
    m.x=side*4+dx*reach;m.y=-13+dy*(reach/2);m.lean_x=reach>4?dx:0;
  }
  return m;
}
inline void arm(Raster&r,int sx,int sy,int hx,int hy,uint16_t sleeve) {
  int ex=(sx+hx)/2,ey=(sy+hy)/2+(hy>=sy?2:-1);
  for(int ox=-1;ox<=1;++ox){r.line(sx+ox,sy,ex+ox,ey,Ink);r.line(ex+ox,ey,hx+ox,hy,Ink);}
  r.line(sx,sy,ex,ey,sleeve);r.line(sx+1,sy,ex+1,ey,sleeve);
  r.line(ex,ey,hx,hy,rgb(223,177,130));r.rect(hx-1,hy-1,3,2,rgb(236,193,144));
}
inline void body(Raster&r,int x,int y,int dir,int walk,const Motion&m) {
  const auto&s=gait::rig(dir);auto p=walk?gait::pose(walk,dir):gait::Pose{};p.bob=0;
  if(!m.two_hands)gait::arm(r,s,p,dir,(dir&1)?1:0,x-12,y-30);
  gait::leg(r,s,p,dir,1,x-12,y-30);gait::leg(r,s,p,dir,0,x-12,y-30);
  if(s.side)r.rect(x-12+(dir==1?10:9),y-14+m.lean_y,5,7,s.cloth);
  sprite(r,*s.body,x-12+m.lean_x,y-30+m.lean_y);
}
inline void use(Raster&r,int x,int y,int dir,int walk,int tool,int remaining,int grade,bool fishing=false,Target target={}) {
  Motion m=motion(dir,tool,remaining,grade,target.x-x,target.y-y,target.valid);
  if(fishing&&tool==6&&(!remaining||m.progress>=144)) {
    m.x=((dir==1)-(dir==3))*7+(dir==3?-3:3);m.y=-15+((dir==2)-(dir==0))*2;m.angle=dir*4;m.two_hands=true;
  }
  const auto &s=gait::rig(dir);const auto &pose=held_frame(tool,grade,m.angle,m.mirrored);
  bool behind=dir==0||(dir==2&&remaining&&m.progress<100&&m.two_hands);
  int near_shoulder=dir==1?-3:dir==3?3:dir==2&&tool==4?-5:5,far_shoulder=-near_shoulder;
  auto far_arm=[&]{if(m.two_hands)arm(r,x+far_shoulder+m.lean_x,y-15+m.lean_y,x+m.x+pose.second_x,y+m.y+pose.second_y,s.shirt);};
  auto near_arm=[&]{arm(r,x+near_shoulder+m.lean_x,y-14+m.lean_y,x+m.x,y+m.y,s.shirt);};
  auto held=[&]{if(tool==1)sprite(r,sprites::seed_handbag,x+m.x-4,y+m.y-2);else if(tool!=3)grounded(r,*pose.sprite,x+m.x,y+m.y);};
  far_arm();if(behind){held();near_arm();}
  body(r,x,y,dir,walk,m);
  if(!behind){held();near_arm();}
  if(tool==6&&((remaining&&m.progress>110)||(fishing&&!remaining))) {
    int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0);
    r.line(x+m.x+pose.tip_x,y+m.y+pose.tip_y,x+dx*32,y+dy*32-2,farm_palette::Soft);
    r.rect(x+dx*32-1,y+dy*32-2,3,1,rgb(191,97,48));
  }
  if(tool==7&&remaining&&m.progress>=Impact&&m.progress<190) {
    int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0),tx=target.valid?target.x:x+dx*16,ty=target.valid?target.y:y+dy*16-10;
    for(int i=0;i<3;++i)r.line(tx-dy*(8-i*2),ty-dx*(8-i*2),tx+dy*(7-i),ty+dx*(7-i),i?Cream:Gold);
  }
  if(tool!=6&&tool!=7&&remaining&&m.progress>=Impact&&m.progress<218) {
    int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0);
    int tx=target.valid?target.x:x+dx*16,ty=target.valid?target.y:y+dy*16-surface_offset(tool);
    if(tool==2)r.line(x+m.x+pose.tip_x,y+m.y+pose.tip_y,tx,ty,rgb(135,183,190));
    for(int i=0;i<(tool==2?7:4);++i) {
      int t=m.progress-Impact,xx=tx+(i%2?1:-1)*(2+t/13+i%3),yy=ty-t/10+i/2;
      uint16_t c=tool==2?rgb(135,183,190):tool==5?rgb(174,173,150):tool==4?rgb(181,128,68):rgb(179,132,77);
      r.rect(xx,yy,tool==2?1:2,tool==2?2:1,c);
    }
  }
}
} // namespace tool_anim
