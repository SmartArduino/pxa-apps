// SPDX-License-Identifier: MIT
#pragma once
// Included inside namespace art; authored poses share the compiled tool grips.
namespace tool_anim {
inline int duration(int tool,int grade=0) { constexpr int ms[]={520,420,600,360,480,460,680}; return ms[tool%7]-((tool==4||tool==5)?grade*35:0); }
inline const sprites::Sprite &frame(int tool,int grade,int angle) {
  if(tool==0) {
    const sprites::Sprite *p[3][8]={
      {&sprites::held_hoe0_0,&sprites::held_hoe0_1,&sprites::held_hoe0_2,&sprites::held_hoe0_3,&sprites::held_hoe0_4,&sprites::held_hoe0_5,&sprites::held_hoe0_6,&sprites::held_hoe0_7},
      {&sprites::held_hoe0_0,&sprites::held_hoe0_1,&sprites::held_hoe0_2,&sprites::held_hoe0_3,&sprites::held_hoe0_4,&sprites::held_hoe0_5,&sprites::held_hoe0_6,&sprites::held_hoe0_7},
      {&sprites::held_hoe0_0,&sprites::held_hoe0_1,&sprites::held_hoe0_2,&sprites::held_hoe0_3,&sprites::held_hoe0_4,&sprites::held_hoe0_5,&sprites::held_hoe0_6,&sprites::held_hoe0_7},
    };
    return *p[std::clamp(grade,0,2)][angle&7];
  }
  else if(tool==2) {
    const sprites::Sprite *p[3][8]={
      {&sprites::held_can0_0,&sprites::held_can0_1,&sprites::held_can0_2,&sprites::held_can0_3,&sprites::held_can0_4,&sprites::held_can0_5,&sprites::held_can0_6,&sprites::held_can0_7},
      {&sprites::held_can0_0,&sprites::held_can0_1,&sprites::held_can0_2,&sprites::held_can0_3,&sprites::held_can0_4,&sprites::held_can0_5,&sprites::held_can0_6,&sprites::held_can0_7},
      {&sprites::held_can0_0,&sprites::held_can0_1,&sprites::held_can0_2,&sprites::held_can0_3,&sprites::held_can0_4,&sprites::held_can0_5,&sprites::held_can0_6,&sprites::held_can0_7},
    };
    return *p[std::clamp(grade,0,2)][angle&7];
  }
  else if(tool==4) {
    const sprites::Sprite *p[3][8]={
      {&sprites::held_axe0_0,&sprites::held_axe0_1,&sprites::held_axe0_2,&sprites::held_axe0_3,&sprites::held_axe0_4,&sprites::held_axe0_5,&sprites::held_axe0_6,&sprites::held_axe0_7},
      {&sprites::held_axe1_0,&sprites::held_axe1_1,&sprites::held_axe1_2,&sprites::held_axe1_3,&sprites::held_axe1_4,&sprites::held_axe1_5,&sprites::held_axe1_6,&sprites::held_axe1_7},
      {&sprites::held_axe2_0,&sprites::held_axe2_1,&sprites::held_axe2_2,&sprites::held_axe2_3,&sprites::held_axe2_4,&sprites::held_axe2_5,&sprites::held_axe2_6,&sprites::held_axe2_7},
    };
    return *p[std::clamp(grade,0,2)][angle&7];
  }
  else if(tool==5) {
    const sprites::Sprite *p[3][8]={
      {&sprites::held_pick0_0,&sprites::held_pick0_1,&sprites::held_pick0_2,&sprites::held_pick0_3,&sprites::held_pick0_4,&sprites::held_pick0_5,&sprites::held_pick0_6,&sprites::held_pick0_7},
      {&sprites::held_pick1_0,&sprites::held_pick1_1,&sprites::held_pick1_2,&sprites::held_pick1_3,&sprites::held_pick1_4,&sprites::held_pick1_5,&sprites::held_pick1_6,&sprites::held_pick1_7},
      {&sprites::held_pick2_0,&sprites::held_pick2_1,&sprites::held_pick2_2,&sprites::held_pick2_3,&sprites::held_pick2_4,&sprites::held_pick2_5,&sprites::held_pick2_6,&sprites::held_pick2_7},
    };
    return *p[std::clamp(grade,0,2)][angle&7];
  }
  else if(tool==6) {
    const sprites::Sprite *p[3][8]={
      {&sprites::held_rod0_0,&sprites::held_rod0_1,&sprites::held_rod0_2,&sprites::held_rod0_3,&sprites::held_rod0_4,&sprites::held_rod0_5,&sprites::held_rod0_6,&sprites::held_rod0_7},
      {&sprites::held_rod0_0,&sprites::held_rod0_1,&sprites::held_rod0_2,&sprites::held_rod0_3,&sprites::held_rod0_4,&sprites::held_rod0_5,&sprites::held_rod0_6,&sprites::held_rod0_7},
      {&sprites::held_rod0_0,&sprites::held_rod0_1,&sprites::held_rod0_2,&sprites::held_rod0_3,&sprites::held_rod0_4,&sprites::held_rod0_5,&sprites::held_rod0_6,&sprites::held_rod0_7},
    };
    return *p[std::clamp(grade,0,2)][angle&7];
  }
  return sprites::held_hoe0_0;
}
struct Motion { int x,y,angle,lean_x,lean_y,progress; bool two_hands; };
inline Motion motion(int dir,int tool,int remaining,int grade=0) {
  int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0),side=dir==3?-1:1;
  int p=remaining?std::clamp((duration(tool,grade)-remaining)*256/duration(tool,grade),0,255):0;
  Motion m{side*6,-15,dir*2,0,0,p,false};
  if(!remaining) {
    constexpr int idle[]={0,1,3,7};
    m.angle=tool==2?dir*2:idle[dir&3];
    m.x=dir==0?-4:side*6;m.y=tool==2?-17:dir==2?-11:-15;
    return m;
  }
  int base=dir*2;
  if(tool==0||tool==4||tool==5||tool==6) {
    m.two_hands=true;
    int phase=p<80?0:p<130?1:p<180?2:p<220?3:4;
    constexpr int angles[]={-2,-1,0,0,1};
    m.angle=base+angles[phase]*side;
    if(phase==0) {m.x=side*3-dx*2;m.y=-26;}
    else if(phase==1) {m.x=side*3+dx*3;m.y=-22;}
    else {m.x=side*2+dx*7;m.y=dir==2?-3:dir==0?-14:-11;
      m.lean_x=dx;m.lean_y=dy;}
    if(tool==0&&phase>=2)m.angle=base;
    if(tool==5&&phase==4)m.angle=base; // pick lifts straight out of the rock
    if(tool==6) {m.y-=5;if(phase>=2)m.angle=base;}
    if(phase==4) {constexpr int idle[]={0,1,3,7};m.x=dir==0?-4:side*6;m.y=dir==2?-11:-15;m.angle=idle[dir&3];m.lean_x=m.lean_y=0;}
  } else if(tool==2) {
    m.x=side*4+dx*6;m.y=-11+dy*4;m.angle=base;
    if(p>70&&p<220){m.lean_x=dx;m.lean_y=1;}
  } else {
    int reach=p<90?2:p<190?9:5;
    m.x=dx*reach+side*4;m.y=-13+dy*(reach/2);m.lean_x=p>90&&p<190?dx:0;
  }
  return m;
}
inline void arm(Raster&r,int sx,int sy,int hx,int hy) {
  int ex=sx+(hx-sx)/3,ey=std::min(sy+3,hy+5);
  for(int ox=0;ox<3;++ox) {r.line(sx+ox-1,sy,ex+ox-1,ey,Ink);r.line(ex+ox-1,ey,hx+ox-1,hy,Ink);}
  r.line(sx,sy,ex,ey,rgb(150,155,102));
  r.line(ex,ey,hx,hy,rgb(223,177,130));
  r.rect(hx-1,hy-1,3,2,rgb(236,193,144));
}
inline void body(Raster&r,int x,int y,int dir,int walk,const Motion&m) {
  const auto&s=gait::rig(dir);
  auto p=walk?gait::pose(walk,dir):gait::Pose{};p.bob=0;
  if(!m.two_hands) gait::arm(r,s,p,dir,(dir&1)?1:0,x-12,y-30);
  gait::leg(r,s,p,dir,1,x-12,y-30);gait::leg(r,s,p,dir,0,x-12,y-30);
  if(s.side) {r.rect(x-12+(dir==1?10:9),y-14+m.lean_y,5,7,s.cloth);}
  sprite(r,*s.body,x-12+m.lean_x,y-30+m.lean_y);
}
inline void use(Raster&r,int x,int y,int dir,int walk,int tool,int remaining,int grade,bool fishing=false) {
  Motion m=motion(dir,tool,remaining,grade);
  if(fishing && tool==6 && !remaining) {m.x=((dir==1)-(dir==3))*7+(dir==3?-3:3);m.y=-15+((dir==2)-(dir==0))*2;m.angle=dir*2;}
  auto hands=[&] {
    if(m.two_hands) arm(r,x-5+m.lean_x,y-14+m.lean_y,x+m.x-1,y+m.y);
    arm(r,x+((dir==3)?-4:5)+m.lean_x,y-14+m.lean_y,x+m.x+1,y+m.y);
  };
  auto held=[&] {
    if(tool==1) sprite(r,sprites::seed_handbag,x+m.x-4,y+m.y-2);
    else if(tool!=3) grounded(r,frame(tool,grade,m.angle),x+m.x,y+m.y);
  };
  if(dir==0) {hands();held();}
  body(r,x,y,dir,walk,m);
  if(dir!=0) {hands();held();}
  if(tool==6 && ((remaining && m.progress>110) || (fishing && !remaining))) {
    constexpr int cs[]={256,181,0,-181,-256,-181,0,181},sn[]={0,181,256,181,0,-181,-256,-181};
    int a=m.angle&7,tx=(20*sn[a])/256,ty=(-20*cs[a])/256;
    int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0);
    r.line(x+m.x+tx,y+m.y+ty,x+dx*21,y+dy*21-2,farm_palette::Soft);
    r.rect(x+dx*21-1,y+dy*21-2,3,1,rgb(191,97,48));
  }
  if(tool!=6 && remaining && m.progress>110 && m.progress<235) {
    int dx=(dir==1)-(dir==3),dy=(dir==2)-(dir==0);
    for(int i=0;i<(tool==2?7:4);++i) {
      int step=(m.progress/12+i*3)%13;
      int xx=x+dx*(9+step/2)+(dy?(i%3-1)*2:0);
      int yy=y+dy*(8+step/2)-3+(dx?i:0);
      uint16_t c=tool==2?rgb(135,183,190):tool==5?rgb(174,173,150):tool==4?rgb(181,128,68):rgb(179,132,77);
      r.rect(xx,yy,tool==2?1:2,tool==2?2:1,c);
    }
  }
}
} // namespace tool_anim
