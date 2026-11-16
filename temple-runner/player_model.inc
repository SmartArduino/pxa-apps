// SPDX-License-Identifier: MIT
// Original explorer mesh. Elliptical profiles and tapered joints share a small
// smooth-light palette; all vertices live on the stack, no model/runtime loader.
const auto pose=motion::pose(s,time);
const float phase=pose.phase,root_y=pose.lift;
const float fall=s.mode==Mode::Dead?std::min(1.f,s.death_t*2):0;
const float pitch=pose.pitch+fall*.7f,cp=cosf(pitch),sp=sinf(pitch);
const float cy=cosf(pose.yaw),sy=sinf(pose.yaw),cr=cosf(pose.roll),sr=sinf(pose.roll);
bool body_space=false;int foot_index=-1;float foot_cp=1,foot_sp=0;V3 foot_origin{},foot_offset{};
auto rotate=[&](V3 p){
 if(foot_index>=0)p={p.x,p.y*foot_cp-p.z*foot_sp,p.y*foot_sp+p.z*foot_cp};
 if(body_space){p={p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};p={p.x*cr-p.y*sr,p.x*sr+p.y*cr,p.z};}
 return V3{p.x*cy+p.z*sy,p.y,p.z*cy-p.x*sy};
};
auto world=[&](V3 p){
 if(body_space)p.y-=.88f;
 if(foot_index>=0)p=sub(p,foot_origin);
 p=rotate(p);if(foot_index>=0)p=add(p,foot_offset);p.x+=s.lane_x;p.y+=root_y+(body_space?pose.hip_height:0)-fall*.35f;p.z+=.8f;return p;
};
struct ActorVertex {V3 point,normal;uint8_t light;};
auto shade_world=[](V3 n){return uint8_t(std::clamp(int(4.9f+1.4f*n.y-1.25f*n.x-1.3f*n.z),0,7));};
auto vertex=[&](V3 p,V3 n){V3 wn=rotate(n);return ActorVertex{world(p),wn,shade_world(wn)};};
auto poly=[&](const ActorVertex& a,const ActorVertex& b,const ActorVertex& c,const ActorVertex& d,int color){
 V3 center=mul(add(add(a.point,b.point),add(c.point,d.point)),.25f);
 V3 outward=add(add(a.normal,b.normal),add(c.normal,d.normal));
 V3 n=add(cross(sub(b.point,a.point),sub(c.point,a.point)),cross(sub(c.point,a.point),sub(d.point,a.point)));
 if(n.x*outward.x+n.y*outward.y+n.z*outward.z<0)n=mul(n,-1);
 V3 eye=sub(V3{camera_x,camera_height,-camera_back},center);
 if(n.x*eye.x+n.y*eye.y+n.z*eye.z<-.01f)return;
 int before=count;face(a.point,b.point,c.point,d.point,actor_colors[color],.9f);
 if(count==before+1&&textures){auto& f=faces[before];f.material=-2;f.palette=uint8_t(208+color);f.shades={a.light,b.light,c.light,d.light};}
};
auto cap=[&](ActorVertex a,ActorVertex b,ActorVertex c,ActorVertex d,V3 normal,int color){
 V3 n=rotate(normal);uint8_t light=shade_world(n);
 a.normal=b.normal=c.normal=d.normal=n;a.light=b.light=c.light=d.light=light;poly(a,b,c,d,color);
};
auto circle=[](int i,int sides){return sides==12?actor_ring12[i]:V3{ring[sides][i][0],0,ring[sides][i][1]};};
struct Profile{float y,rx,rz;};
auto shape=[&](V3 center,const Profile* rows,int nr,int color,int sides=8){
 if(sides==12)sides=8;
 if(quality&&sides==8)sides=6;
 ActorVertex pts[8][12];
 // A profile vertex is shared by adjacent faces. Transform and light it once.
 for(int j=0;j<nr;j++){
  int lo=std::max(0,j-1),hi=std::min(nr-1,j+1);
  float dh=std::max(.01f,rows[hi].y-rows[lo].y);
  float slope=((rows[hi].rx-rows[lo].rx)+(rows[hi].rz-rows[lo].rz))*.5f/dh;
  float aspect=rows[j].rx/std::max(.025f,rows[j].rz);
  for(int i=0;i<sides;i++){
   V3 r=circle(i,sides),p=add(center,{r.x*rows[j].rx,rows[j].y,r.z*rows[j].rz});
   pts[j][i]=vertex(p,norm({r.x,-slope,r.z*aspect}));
  }
 }
 for(int j=0;j<nr-1;j++)for(int i=0;i<sides;i++){int k=(i+1)%sides;poly(pts[j][i],pts[j][k],pts[j+1][k],pts[j+1][i],color);}
 for(int j: {0,nr-1}){V3 n={0,j?1.f:-1.f,0};for(int i=1;i+1<sides;i+=2){int k=std::min(i+2,sides-1);cap(pts[j][0],pts[j][i],pts[j][i+1],pts[j][k],n,color);}}
};
auto bone=[&](V3 a,V3 b,float ra,float rb,int color,int sides=8){
 if(quality&&sides>6)sides=6;
 V3 dir=norm(sub(b,a)),u=norm(cross(dir,{0,0,1})),v=cross(dir,u);
 a=sub(a,mul(dir,.015f));b=add(b,mul(dir,.015f));
 ActorVertex ends[2][8];float slope=(ra-rb)/std::max(.01f,motion::length(sub(b,a)));
 for(int i=0;i<sides;i++){
  V3 r=circle(i,sides),radial=add(mul(u,r.x),mul(v,r.z)),n=norm(add(radial,mul(dir,slope)));
  V3 wn=rotate(n);uint8_t light=shade_world(wn);
  ends[0][i]={world(add(a,mul(radial,ra))),wn,light};ends[1][i]={world(add(b,mul(radial,rb))),wn,light};
 }
 for(int i=0;i<sides;i++){int j=(i+1)%sides;poly(ends[0][i],ends[0][j],ends[1][j],ends[1][i],color);}
 for(int end=0;end<2;end++){V3 n=mul(dir,end?1.f:-1.f);
  for(int i=1;i+1<sides;i+=2){int k=std::min(i+2,sides-1);cap(ends[end][0],ends[end][i],ends[end][i+1],ends[end][k],n,color);}
 }
};
auto panel=[&](V3 a,V3 b,V3 c,V3 d,int color){
 // Cloth and leather ribbons have two visible sides, including during slides.
 V3 wa=world(a),wb=world(b),wc=world(c),wd=world(d);int before=count;
 face(wa,wb,wc,wd,actor_colors[color],.92f);
 if(count==before+1&&textures){auto& f=faces[before];f.material=-2;f.palette=uint8_t(208+color);f.shades={5,5,5,5};}
};
mesh_layer=3;
// Soft silhouette shadow remains attached to the path while jumping.
for(int i=0;i<8;i++){V3 a=actor_ring12[(i*12/8)%12],b=actor_ring12[((i+1)*12/8)%12];float r=.5f+s.jump*.1f;
 face({s.lane_x,.026f,.8f},{s.lane_x+a.x*r,.026f,.8f+a.z*r*.65f},{s.lane_x+b.x*r,.026f,.8f+b.z*r*.65f},{s.lane_x,.026f,.8f},0x49513b,.7f);
}
if(s.mode==Mode::Running&&pose.slide>.3f){
 // Low dust and heel trails follow the slide, without full-screen particles.
 for(int i=0;i<(quality?2:4);i++){float t=fmodf(s.slide_age*2.5f+i*.29f,1.f),side=i&1?1.f:-1.f;
  float x=s.lane_x+side*(.24f+t*.23f),z=.5f-t*1.5f,y=.04f+t*.13f,r=.04f+t*.065f;
  face({x-r,y,z},{x,y+r,z},{x+r,y,z},{x,y-r,z},theme==2?0x7c9392:0xadab89,.8f);
 }
}
// The pelvis and feet use world-aligned IK. Pitching the upper body must
// never rotate a planted foot through the paving during a slide.
mesh_layer=1;
const Profile joint_ball[]={{-.09f,.025f,.025f},{0,.108f,.108f},{.09f,.025f,.025f}};
for(int i=0;i<2;i++){
 body_space=false;auto h=pose.hip[i],k=pose.knee[i],ankle=pose.ankle[i];
 bone(h,k,.128f,.105f,2);bone(k,ankle,.105f,.078f,2);shape(k,joint_ball,3,2,6);
 const Profile sole[]={{-.11f,.10f,.18f},{-.075f,.113f,.20f}};
 const Profile shoe[]={{-.075f,.11f,.19f},{.005f,.114f,.18f},{.065f,.081f,.105f}};
 const Profile boot[]={{0,.082f,.08f},{.15f,.094f,.09f}};
 foot_index=i;foot_cp=cosf(pose.foot_pitch[i]);foot_sp=sinf(pose.foot_pitch[i]);foot_origin=ankle;foot_offset={ankle.x*cy+ankle.z*sy,ankle.y,ankle.z*cy-ankle.x*sy};
 shape(add(ankle,{0,0,.06f}),sole,2,8);shape(add(ankle,{0,0,.06f}),shoe,3,9);shape(ankle,boot,2,7);
 foot_index=-1;body_space=true;auto sh=pose.shoulder[i],el=pose.elbow[i],hand=pose.wrist[i];
 V3 sleeve=add(sh,mul(sub(el,sh),.62f));
 bone(sh,sleeve,.116f,.105f,0);bone(sleeve,add(sleeve,mul(sub(el,sh),.12f)),.108f,.108f,1);
 bone(sleeve,el,.082f,.075f,3,6);bone(el,hand,.079f,.065f,3,6);
 const Profile elbow_ball[]={{-.07f,.025f,.025f},{0,.08f,.08f},{.07f,.025f,.025f}};shape(el,elbow_ball,3,3,6);
 const Profile fist[]={{-.065f,.035f,.04f},{-.035f,.075f,.065f},{.04f,.065f,.06f},{.065f,.035f,.03f}};
 shape(hand,fist,4,3,6);
}
body_space=true;
const Profile torso[]={{.82f,.18f,.13f},{.98f,.21f,.145f},{1.23f,.255f,.17f},{1.43f,.29f,.16f},{1.50f,.21f,.125f}};
shape({0,0,0},torso,5,0,12);
const Profile belt[]={{.84f,.19f,.145f},{.91f,.195f,.15f}};shape({0,0,0},belt,2,5);
const Profile neck[]={{1.46f,.085f,.075f},{1.62f,.09f,.08f}};shape({0,0,.025f},neck,2,3);
const Profile head[]={{1.56f,.105f,.115f},{1.63f,.16f,.145f},{1.79f,.175f,.165f},{1.91f,.145f,.145f},{1.94f,.085f,.09f}};
shape({0,0,.025f},head,5,3,12);
const Profile hair[]={{1.77f,.172f,.165f},{1.89f,.18f,.17f},{1.97f,.115f,.11f}};shape({0,0,.015f},hair,3,9,12);
// Ears are small and rounded, visible when the runner leans into a lane.
const Profile ear[]={{-.055f,.025f,.03f},{0,.045f,.035f},{.055f,.025f,.03f}};
shape({-.17f,1.76f,.025f},ear,3,3,6);shape({.17f,1.76f,.025f},ear,3,3,6);
const Profile brim[]={{1.90f,.255f,.24f},{1.925f,.275f,.25f},{1.95f,.24f,.23f}};shape({0,0,.025f},brim,3,4,12);
const Profile crown[]={{1.94f,.18f,.17f},{2.10f,.165f,.15f},{2.15f,.11f,.105f}};shape({0,0,.025f},crown,3,4,12);
const Profile band[]={{1.95f,.19f,.18f},{1.995f,.185f,.175f}};shape({0,0,.025f},band,2,5,12);
// Tailored collar, shoulder harness and compact rounded backpack.
const Profile collar[]={{1.46f,.105f,.095f},{1.51f,.125f,.105f},{1.55f,.09f,.085f}};shape({0,0,0},collar,3,10);
for(int side: {-1,1}){
 float x=side*.17f;
 panel({x-.026f,.95f,-.17f},{x+.026f,.95f,-.17f},{x+side*.035f+.026f,1.46f,-.18f},{x+side*.035f-.026f,1.46f,-.18f},5);
 bone({side*.21f,1.46f,-.095f},{side*.21f,1.46f,.13f},.025f,.025f,5,6);
}
const Profile pack[]={{.98f,.13f,.065f},{1.02f,.185f,.09f},{1.29f,.19f,.105f},{1.38f,.14f,.075f}};
shape({0,0,-.215f},pack,4,6,12);
const Profile flap[]={{1.24f,.18f,.10f},{1.34f,.20f,.11f},{1.41f,.13f,.07f}};shape({0,0,-.23f},flap,3,14,12);
panel({-.026f,1.12f,-.325f},{.026f,1.12f,-.325f},{.026f,1.32f,-.36f},{-.026f,1.32f,-.36f},5);
panel({-.043f,1.17f,-.37f},{.043f,1.17f,-.37f},{.043f,1.23f,-.37f},{-.043f,1.23f,-.37f},11);
// Three broad scarf segments carry the motion, rather than tiny ornaments.
float flutter=sinf(phase*.75f)*.055f;
panel({-.09f,1.51f,-.09f},{.08f,1.51f,-.09f},{.18f,1.39f,-.36f},{.02f,1.36f,-.37f},10);
panel({.02f,1.36f,-.37f},{.18f,1.39f,-.36f},{.27f+flutter,1.27f,-.56f},{.11f+flutter,1.22f,-.57f},10);
panel({.11f+flutter,1.22f,-.57f},{.27f+flutter,1.27f,-.56f},{.34f,1.29f+flutter,-.75f},{.22f,1.25f+flutter,-.78f},10);
if(s.shield>0||s.rush>0)for(int i=0;i<8;i++){
 float a=i*PI/4+time*.7f,b=a+.28f,r=.73f;
 float x=s.lane_x,y=root_y+.96f,z=.13f;
 uint32_t c=s.rush>0?0xffd487:0x83edca;
 face({x+cosf(a)*r,y+sinf(a)*r,z},{x+cosf(b)*r,y+sinf(b)*r,z},{x+cosf(b)*(r+.025f),y+sinf(b)*(r+.025f),z},{x+cosf(a)*(r+.025f),y+sinf(a)*(r+.025f),z},c,1.2f);
}

mesh_layer=0;
