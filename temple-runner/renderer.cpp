// SPDX-License-Identifier: MIT
#include "renderer.hpp"
#include "font_data.hpp"
#include <pxa/game_upload.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace relic {
namespace {
constexpr float PI=3.14159265f;
constexpr float ring[9][8][2]={{{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{-0.50000000f,0.86602540f},{-0.50000000f,-0.86602540f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.00000000f,1.00000000f},{-1.00000000f,0.00000000f},{-0.00000000f,-1.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.30901699f,0.95105652f},{-0.80901699f,0.58778525f},{-0.80901699f,-0.58778525f},{0.30901699f,-0.95105652f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.50000000f,0.86602540f},{-0.50000000f,0.86602540f},{-1.00000000f,0.00000000f},{-0.50000000f,-0.86602540f},{0.50000000f,-0.86602540f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.62348980f,0.78183148f},{-0.22252093f,0.97492791f},{-0.90096887f,0.43388374f},{-0.90096887f,-0.43388374f},{-0.22252093f,-0.97492791f},{0.62348980f,-0.78183148f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.70710678f,0.70710678f},{0.00000000f,1.00000000f},{-0.70710678f,0.70710678f},{-1.00000000f,0.00000000f},{-0.70710678f,-0.70710678f},{-0.00000000f,-1.00000000f},{0.70710678f,-0.70710678f}}};
constexpr uint32_t GOLD=0xf4c66b, IVORY=0xf6efd7, INK=0x102c29, JADE=0x78d4ac;
uint16_t rgb(uint32_t c){return uint16_t(((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31));}
uint32_t lit(uint32_t c,float l,float fog){
 int r=int(((c>>16)&255)*l),g=int(((c>>8)&255)*l),b=int((c&255)*l);
 return uint32_t(std::clamp(int(r*(1-fog)+63*fog),0,255))<<16|uint32_t(std::clamp(int(g*(1-fog)+102*fog),0,255))<<8|std::clamp(int(b*(1-fog)+91*fog),0,255);
}
V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 mul(V3 a,float f){return {a.x*f,a.y*f,a.z*f};}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm(V3 a){float l=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return l>.0001f?mul(a,1/l):V3{1,0,0};}
uint32_t nextchar(const char*& p){uint32_t c=uint8_t(*p++);if(c<128)return c;int n=c<224?1:c<240?2:3;c&=n==1?31:n==2?15:7;while(n--&&*p)c=(c<<6)|(uint8_t(*p++)&63);return c;}
int glyph(uint32_t c){for(unsigned i=0;i<std::size(font::codes);i++)if(font::codes[i]==c)return i;return 0;}
}
bool Renderer::upload(pxa::game::Renderer& target){
 static std::byte scratch[65556];uint16_t colors[256]{};colors[1]=0xffff;
 pxa::game::Upload u(target,scratch);
 text_alpha=target.supports(pxa::game::RenderCapability::sprite_texel_alpha);
 return bool(u.palette(colors))&&bool(u.texture({0},256,256,std::as_bytes(std::span{font::atlas})))&&bool(u.texture({1},128,36,std::as_bytes(std::span{font::title_atlas})));
}
void Renderer::rect(pxa::game::Frame& f,float x,float y,float w,float h,uint32_t c){if(w<=0||h<=0)return;f.quad({int16_t(x*16),int16_t(y*16),int16_t((x+w)*16),int16_t(y*16),int16_t((x+w)*16),int16_t((y+h)*16),int16_t(x*16),int16_t((y+h)*16)},{rgb(c)});}
void Renderer::text(pxa::game::Frame& f,const char* str,float x,float y,float scale,uint32_t c,bool center){
 if(!strcmp(str,"遗迹疾奔")){
  float factor=scale*.5f;if(center)x-=56*factor;pxa::game::Sprite sprites[4];
  for(int i=0;i<4;i++){auto&g=sprites[i];g.x=int16_t(x+i*28*factor);g.y=int16_t(y);g.width=uint16_t(32*factor);g.height=uint16_t(36*factor);g.source_x=i*32;g.source_width=32;g.source_height=36;}
  pxa::game::SpriteOptions o;o.solid_color=true;o.texel_alpha=text_alpha;o.color={rgb(c)};f.sprites({1},sprites,o);return;
 }
 const char* p=str;float len=0;while(*p)len+=font::advance[glyph(nextchar(p))]*scale;if(center)x-=len/2;
 pxa::game::Sprite sprites[100];int n=0;p=str;
 while(*p&&n<100){int i=glyph(nextchar(p));auto &g=sprites[n++];g.x=int16_t(x);g.y=int16_t(y);g.width=uint16_t(std::max(1,int(16*scale)));g.height=g.width;g.source_x=(i%16)*16;g.source_y=(i/16)*16;g.source_width=g.source_height=16;x+=font::advance[i]*scale;}
 pxa::game::SpriteOptions o;o.solid_color=true;o.texel_alpha=text_alpha;o.color={rgb(c)};f.sprites({0},{sprites,size_t(n)},o);
}
void Renderer::face(V3 a,V3 b,V3 c,V3 d,uint32_t color,float light){
 if(count>=int(std::size(faces)))return;
 V3 src[8]={a,b,c,d},dst[8];int n=4,m=0;
 // Clip against the camera near plane before perspective division.
 for(int i=0;i<n;i++){auto p=src[i],q=src[(i+1)%n];bool pin=p.z> -4.35f,qin=q.z> -4.35f;
  if(pin)dst[m++]=p;if(pin!=qin){float t=(-4.35f-p.z)/(q.z-p.z);dst[m++]=add(p,mul(sub(q,p),t));}}
 if(m<3)return;
 float z=0;for(int i=0;i<m;i++)z+=dst[i].z;z/=m;
 const float focal=std::min(width,height)*.98f;float xy[8][2];float minx=1e6f,maxx=-1e6f,miny=1e6f,maxy=-1e6f;
 for(int i=0;i<m;i++){float cz=dst[i].z+5.5f;float dy=dst[i].y-3.15f;
  float zz=cz*.985f-dy*.172f,yy=dy*.985f+cz*.172f;
  float xx=(dst[i].x-camera_x)*focal/zz,sy=-yy*focal/zz;
  float sx=width*.5f+xx+sy*roll,sy2=height*.38f+sy-xx*roll;
  xy[i][0]=std::clamp(sx,-1500.f,1500.f);xy[i][1]=std::clamp(sy2,-1500.f,1500.f);
  minx=std::min(minx,sx);maxx=std::max(maxx,sx);miny=std::min(miny,sy2);maxy=std::max(maxy,sy2);
 }
 if(maxx<0||minx>width||maxy<0||miny>height)return;
 uint16_t col=rgb(lit(color,light,std::clamp((z-12)/84.f,0.f,.76f)));
 auto emit=[&](int i,int j,int k,int l){if(count>=int(std::size(faces)))return;Face &f=faces[count];int ids[4]={i,j,k,l};for(int t=0;t<4;t++){f.xy[t*2]=int16_t(xy[ids[t]][0]*16);f.xy[t*2+1]=int16_t(xy[ids[t]][1]*16);}int64_t area=0;for(int t=0;t<4;t++){int u=(t+1)%4;area+=int64_t(f.xy[t*2])*f.xy[u*2+1]-int64_t(f.xy[u*2])*f.xy[t*2+1];}if(std::abs(area)<5)return;f.depth=z-.043f*(a.y+b.y+c.y+d.y);f.color=col;count++;};
 if(m==4)emit(0,1,2,3);else for(int i=1;i+1<m;i++)emit(0,i,i+1,i+1);
}
void Renderer::box(float x,float y,float z,float w,float h,float dep,uint32_t c){
 float l=x-w/2,r=x+w/2,b=y,t=y+h,n=z-dep/2,f=z+dep/2;
 face({l,t,n},{r,t,n},{r,t,f},{l,t,f},c,1.12f);
 face({l,b,n},{r,b,n},{r,t,n},{l,t,n},c,.77f);
 if(x>camera_x)face({l,b,f},{l,b,n},{l,t,n},{l,t,f},c,.68f);
 else face({r,b,n},{r,b,f},{r,t,f},{r,t,n},c,.95f);
}
void Renderer::limb(V3 a,V3 b,float radius,uint32_t color,int sides){
 V3 dir=norm(sub(b,a)),u=norm(cross(dir,{0,0,1})),v=cross(dir,u);V3 front[10],back[10];
 for(int i=0;i<sides;i++){V3 off=mul(add(mul(u,ring[sides][i][0]),mul(v,ring[sides][i][1])),radius);front[i]=add(a,off);back[i]=add(b,off);}
 for(int i=0;i<sides;i++){int j=(i+1)%sides;V3 normal=mul(sub(front[i],a),1/radius);V3 center=mul(add(front[i],back[i]),.5f);V3 eye={camera_x,3.15f,-5.5f};if(normal.x*(eye.x-center.x)+normal.y*(eye.y-center.y)+normal.z*(eye.z-center.z)<-.1f)continue;
  face(front[i],front[j],back[j],back[i],color,.82f+.22f*normal.y-.14f*normal.x-.13f*normal.z);}
 if(dir.y>0)for(int i=1;i+1<sides;i++)face(back[0],back[i],back[i+1],back[i+1],color,1.13f);
}
void Renderer::tree(float x,float z,int variant){
 float h=5+(variant%4)*.8f;limb({x,-2,z},{x+.4f,h,z},.24f,0x514a2c,5);
 int leaves=(z<27&&quality==0)?6:4;float cv=cosf(float(variant)),sv=sinf(float(variant));
 for(int i=0;i<leaves;i++){float dx=ring[leaves][i][0]*cv-ring[leaves][i][1]*sv,dz=ring[leaves][i][1]*cv+ring[leaves][i][0]*sv;V3 root={x+.4f,h,z};V3 tip={x+dx*3.8f,h-1.25f,z+dz*3.8f};V3 mid={x+dx*1.9f,h+.3f,z+dz*1.9f};float w=.7f;
 face(root,{mid.x-dz*w,mid.y,mid.z+dx*w},tip,{mid.x+dz*w,mid.y-.16f,mid.z-dx*w},(i&1)?0x297252:0x3b8c59,1.f);
 }
 if(z<32){box(x,-1,z,1.7f,.3f,1.5f,0x264c3d);}
}
void Renderer::idol(float x,float z,float s){
 uint32_t stone=0x7d8972;
 box(x,-.6f,z,1.7f*s,.7f*s,1.5f*s,stone);box(x,.1f,z,1.15f*s,1.8f*s,.92f*s,stone);
 box(x,1.9f*s,z,1.45f*s,1.35f*s,1.05f*s,stone);
 box(x,3.25f*s,z,1.8f*s,.26f*s,1.35f*s,0xb3ad83);
 box(x-.39f*s,2.62f*s,z-.56f*s,.35f*s,.15f*s,.12f*s,0x253a2d);
 box(x+.39f*s,2.62f*s,z-.56f*s,.35f*s,.15f*s,.12f*s,0x253a2d);
 box(x,2.22f*s,z-.65f*s,.3f*s,.4f*s,.3f*s,0xa5a17f);
 box(x,2.02f*s,z-.56f*s,.7f*s,.12f*s,.14f*s,0x394a3b);
 box(x,1.1f*s,z-.5f*s,.55f*s,.5f*s,.16f*s,0xb7a568);
}
void Renderer::runner(const State& s){
 float x=s.lane_x,y=s.jump,z=.8f;float gait=s.mode==Mode::Running?s.elapsed*13.4f:time*2;float a=sinf(gait)*.52f;
 float crouch=s.slide>0?1.15f:0;float fall=s.mode==Mode::Dead?std::min(1.f,s.death_t*2):0;
 y-=fall*.65f;float hip=.82f-crouch*.45f,shoulder=1.52f-crouch,head=1.87f-crouch;
 if(s.jump_t>0)a=.35f;
 // Shadow, boots, animated articulated trousers, sleeves and hands.
 face({x-.55f,.025f,z-.35f},{x+.55f,.025f,z-.35f},{x+.38f,.025f,z+.55f},{x-.38f,.025f,z+.55f},0x454c33,.7f);
 for(int side=-1;side<=1;side+=2){float swing=a*side;V3 h={x+side*.16f,y+hip,z},k={x+side*.18f,y+.42f-crouch*.22f,z+swing*.7f},foot={x+side*.18f,y+.12f,z-swing*.75f};
  limb(h,k,.145f,0x526b64);limb(k,foot,.115f,0x3f534d);box(foot.x,foot.y-.1f,foot.z+.08f,.25f,.16f,.42f,0x382e23);
  V3 sh={x+side*.29f,y+shoulder,z},el={x+side*.37f,y+std::max(.2f,shoulder-.3f),z-swing*.7f},hand={x+side*.32f,y+std::max(.17f,shoulder-.57f),z-swing*.42f+.14f};
  limb(sh,el,.115f,0xe7c590);limb(el,hand,.086f,0xc59169);limb(hand,{hand.x,hand.y-.1f,hand.z},.09f,0xd7a77b);
 }
 limb({x,y+hip,z},{x,y+shoulder,z+.06f},.295f,0xe6c18a,8);
 box(x,y+hip,z-.28f,.53f,.1f,.08f,0x6b462d);
 // Leather backpack, flap, brass buckle and straps, visible from the chase camera.
 box(x,y+hip+.19f,z-.32f,.45f,.48f,.25f,0x835036);box(x,y+hip+.47f,z-.34f,.5f,.13f,.29f,0xab7041);
 box(x,y+hip+.32f,z-.46f,.08f,.13f,.03f,0xefc777);
 box(x-.22f,y+hip+.1f,z-.23f,.055f,.65f,.09f,0x563a28);box(x+.22f,y+hip+.1f,z-.23f,.055f,.65f,.09f,0x563a28);
 limb({x,y+head-.17f,z+.02f},{x,y+head+.16f,z+.02f},.205f,0xce9970,8);
 limb({x,y+head+.08f,z+.02f},{x,y+head+.25f,z+.02f},.22f,0x4d3425,8);
 limb({x,y+head+.14f,z+.02f},{x,y+head+.19f,z+.02f},.32f,0xc39a5b,8);
 limb({x,y+head+.19f,z+.02f},{x,y+head+.35f,z+.02f},.22f,0xb58b4d,8);
 // Scarf tails follow the running cycle.
 face({x-.12f,y+shoulder,z-.21f},{x+.13f,y+shoulder,z-.21f},{x+.22f,y+std::max(.2f,shoulder-.3f),z-.75f},{x+.02f,y+shoulder-.32f,z-.8f},0xc64d35);
 if(s.shield>0){float r=.75f;for(int i=0;i<10;i++){float a=i*2*PI/10,b=(i+1)*2*PI/10;limb({x+cosf(a)*r,y+.95f+sinf(a)*r,z-.56f},{x+cosf(b)*r,y+.95f+sinf(b)*r,z-.56f},.027f,0x83edca,4);}}
}
void Renderer::render(pxa::game::Frame& f,const State& s,const Save& save){
 count=0;int theme=int(s.distance/400)%3;camera_x=s.lane_x*.3f;roll=s.turn_fx*.1f;travel=s.mode==Mode::Title?time*1.2f:s.distance;
 f.clear({rgb(theme==1?0xbbaa80:theme==2?0x243f50:0x7dafa0)});
 // Layered mist and a warm shaft of sunlight form the distant jungle canopy.
 for(int i=0;i<9;i++){uint32_t c=theme==1?lit(0xbca57b,.75f+i*.025f,0):theme==2?lit(0x274653,.75f+i*.025f,0):lit(0x84b09a,.75f+i*.025f,0);rect(f,0,i*height*.065f,width,height*.067f,c);}
 for(int i=0;i<9;i++){float xx=(i*53+17)%width;float hh=height*(.16f+.04f*(i%3));rect(f,xx,0,8+i%5,hh,theme==2?0x233b42:0x496f5b);}
 // Mountain and canopy silhouettes establish depth beyond the temple.
 for(int layer=0;layer<3;layer++)for(int i=0;i<6;i++){
  float x=(i*width/5.f)-width*.15f;float y=height*(.33f+layer*.075f),peak=y-height*(.15f+(i%3)*.025f);
  f.quad({int16_t(x*16),int16_t(y*16),int16_t((x+width*.11f)*16),int16_t(peak*16),int16_t((x+width*.27f)*16),int16_t(y*16),int16_t(x*16),int16_t(y*16)}, {rgb(layer==0?0x6c9785:layer==1?0x537e68:0x3c6551)});
 }
 uint32_t stone=theme==1?0xbaa577:theme==2?0x6b8290:0x9b9b76;
 runner(s);
 if((s.mode==Mode::Running&&s.elapsed<3.f)||s.mode==Mode::Dead){
  float z=s.mode==Mode::Dead?-.8f-std::max(0.f,1.f-s.death_t)*1.8f:-1.8f-s.elapsed*1.35f;
  float x=-1.65f,y=.38f+sinf(time*11)*.1f;uint32_t obsidian=0x263d35;
  limb({x,y,z},{x,y+.55f,z+.45f},.42f,obsidian,6);
  box(x,y+.4f,z+.33f,.72f,.6f,.65f,0x31483d);
  for(int side=-1;side<=1;side+=2){float swing=sinf(time*11+side)*.28f;
   limb({x+side*.36f,y+.45f,z+.2f},{x+side*.6f,.12f,z-.45f+swing},.14f,obsidian,5);
   limb({x+side*.24f,y,z+.3f},{x+side*.25f,.09f,z+.8f-swing},.15f,obsidian,5);
   box(x+side*.19f,y+.75f,z-.02f,.12f,.1f,.06f,0x9ee4af);
   limb({x+side*.26f,y+.95f,z+.26f},{x+side*.42f,y+1.24f,z+.45f},.09f,0x878e70,4);
  }
 }
 // River under the elevated causeway and mossy banks; actual 3D planes.
 face({-90,-3,-4},{90,-3,-4},{90,-3,95},{-90,-3,95},theme==1?0x698d80:theme==2?0x203e4d:0x4d9183);
 for(int side=-1;side<=1;side+=2)for(int i=0;i<6;i++){
  float z=i*15.f-fmodf(travel,15.f),x=side*(7.f+(i%2)*3);
  face({x-4,-2,z-9},{x+4,-2,z-9},{x+3,-1.8f,z+9},{x-3,-1.8f,z+9},theme==1?0x727d4a:0x3f684c);
 }
 float turnz=s.next_turn-s.distance;bool elbow=s.mode!=Mode::Title&&turnz<75;
 const float tile=3.2f;float startz=-4.f-fmodf(travel,tile);
 for(int i=0;i<25;i++){float z=startz+i*tile;if(elbow&&z>turnz+2)continue;
  bool gap=false;for(int j=0;j<s.count;j++)if(s.objects[j].kind==Kind::Gap&&fabsf((s.objects[j].distance-s.distance)-z)<2.3f)gap=true;
  if(gap)continue;
  // Raised stone bridge with individually shaded paving, moss and golden borders.
  box(0,-.68f,z,5.8f,.65f,tile-.025f,stone);
  for(int lane=-1;lane<=1;lane++){float x=lane*1.8f;uint32_t c=lit(stone,.92f+((i+lane+int(travel/tile))&3)*.04f,0);face({x-.85f,.01f,z-1.53f},{x+.85f,.01f,z-1.53f},{x+.85f,.01f,z+1.53f},{x-.85f,.01f,z+1.53f},c);}
  for(int side=-1;side<=1;side+=2){
   float rx=side*2.86f;face({rx-.08f,.15f,z-1.57f},{rx+.08f,.15f,z-1.57f},{rx+.08f,.15f,z+1.57f},{rx-.08f,.15f,z+1.57f},0xd0bc7f);
   if(z<12)face({rx-.08f,.02f,z-1.57f},{rx+.08f,.02f,z-1.57f},{rx+.08f,.15f,z-1.57f},{rx-.08f,.15f,z-1.57f},0xa29566);
   if((i+int(travel/tile))%3==0){box(side*3.f,-.05f,z,.45f,.65f,.5f,stone);box(side*3.f,.6f,z,.55f,.12f,.6f,0xb7b388);}
  }
  if(i<11&&quality==0){float x=((i*19)%7-3)*.65f;face({x,.025f,z-.9f},{x+.05f,.025f,z-.8f},{x-.28f,.025f,z+.1f},{x-.32f,.025f,z+.15f},0x68755a);}
 }
 if(elbow){float x=s.turn_direction*12.f;box(x,-.65f,turnz,24,.64f,5.8f,stone);box(0,0,turnz+2.8f,5.8f,.6f,.4f,0xb4a77c);}
 for(int side=-1;side<=1;side+=2){
  for(int i=0;i<(quality==0?5:3);i++){float z=7+i*12-fmodf(travel,12.f);float x=side*(5.f+(i%3)*1.5f);
   if(theme==2){box(x,-1,z,1.2f,5.9f,1.4f,0x4b6570);box(x,4.9f,z,1.7f,.4f,1.8f,0x87938b);if(i%2==0)box(0,5.3f,z,12,.4f,1.5f,0x3f5664);}
   else {tree(x,z,i+side*3);if(theme==1&&i%2==0)box(x+side*2,-.5f,z,2.2f,3.f,2.4f,0x9b8b61);}
   if(z<24&&quality==0)for(int leaf=0;leaf<3;leaf++){float dx=(leaf-1)*.65f;face({x-.7f,-.6f,z},{x+dx,-.05f,z+.5f},{x+dx*1.7f,-.2f,z+1.4f},{x-.7f,-.6f,z},0x568458);}
}
  for(int i=0;i<2;i++){float z=14+i*30-fmodf(travel,30.f);if(z< -3)continue;
   if(i==1)idol(side*4.3f,z,1.05f);
   // Braziers are geometric flames with a flickering amber core.
   box(side*3.4f,.05f,z,.35f,1.55f,.35f,0x545e47);box(side*3.4f,1.5f,z,.65f,.18f,.6f,0x8b7142);
   float flicker=sinf(time*14+z)*.12f;limb({side*3.4f,1.65f,z},{side*3.4f+.08f,2.3f+flicker,z},.17f,theme==2?0x61cfe2:0xf3a748,5);
   limb({side*3.4f,1.65f,z-.05f},{side*3.4f-.04f,2.08f+flicker,z-.05f},.09f,0xffedaa,5);
  }
 }
 // Monumental gate. Repeated every 96 m, with stepped lintel and jade keystone.
 float gatez=71-fmodf(travel,96.f);
 if(gatez> -4){for(int side=-1;side<=1;side+=2){box(side*3.65f,-.5f,gatez,1.25f,5.2f,1.4f,0x7d8970);box(side*3.65f,-.1f,gatez,1.6f,.4f,1.7f,0xb3aa7f);box(side*3.65f,4.2f,gatez,1.7f,.4f,1.7f,0xa9a178);}
  box(0,4.6f,gatez,8.7f,.75f,1.65f,0xa29e76);box(0,5.35f,gatez,7.4f,.32f,1.8f,0xc3b98c);box(0,4.6f,gatez-.9f,.6f,.75f,.18f,0x56ac93);
 }
 // Deterministic gold trails, culled at the same collection cursor used by the rules.
 int first=std::max(4,int(s.distance/3)-1);for(int i=first;i<first+(quality==0?19:13);i++){float z=i*3.f-s.distance;if(z<.8f||coin_taken(s,i))continue;float x=coin_lane(i)*1.65f;float y=coin_height(i)+sinf(time*4+i*.5f)*.08f;
  float spin=time*2.8f+i*.6f;float w=.19f*(.25f+.75f*fabsf(cosf(spin)));
  face({x-w*.5f,y-.22f,z},{x+w*.5f,y-.22f,z},{x+w,y,z},{x-w,y,z},0xf8cc58);
  face({x-w,y,z},{x+w,y,z},{x+w*.5f,y+.22f,z},{x-w*.5f,y+.22f,z},0xffd96e);
  face({x-.07f,y-.12f,z-.17f},{x+.07f,y-.12f,z-.17f},{x+.07f,y+.12f,z-.17f},{x-.07f,y+.12f,z-.17f},0xffec9a);
 }
 for(int i=0;i<s.count;i++){const auto&o=s.objects[i];float z=o.distance-s.distance;if(z< -3||z>85||o.passed)continue;float x=o.lane*1.65f;
  if(o.kind==Kind::Rock){box(x,0,z,1.18f,.95f,1.12f,0x7d7660);box(x-.12f,.95f,z,.8f,.25f,.8f,0x99937a);}
  if(o.kind==Kind::Root){limb({-2.7f,.4f,z},{2.7f,.4f,z},.38f,0x785133,8);for(int k=-2;k<=2;k++)box(k,.55f,z-.12f,.12f,.12f,.4f,0xb79057);}
  if(o.kind==Kind::Gate){for(int side=-1;side<=1;side+=2)box(side*2.5f,0,z,.4f,2.9f,.5f,0x68694f);box(0,1.08f,z,5.4f,.74f,.6f,0x99794d);box(0,1.83f,z,5.7f,.17f,.8f,0xcfb377);}
  if(o.kind==Kind::Gap){box(0,-.95f,z-2.1f,5.8f,.24f,.4f,0x4d5642);box(0,-.95f,z+2.1f,5.8f,.24f,.4f,0x4d5642);}
  if(o.kind==Kind::Magnet||o.kind==Kind::Shield){float y=1.2f+sinf(time*4)*.16f;limb({x,y-.32f,z},{x,y+.32f,z},.35f,o.kind==Kind::Shield?0x78d9c6:0x7cbbe7,6);box(x,y-.1f,z-.35f,.12f,.28f,.04f,0xffe6a3);}
 }
 peak_faces=std::max(peak_faces,count);
 std::sort(faces,faces+count,[](const Face&a,const Face&b){return a.depth>b.depth;});for(int i=0;i<count;i++)f.quad(faces[i].xy,{faces[i].color});
 // UI uses a single atlas and RGB565 sprites, with no per-frame glyph downloads.
 float ui=std::clamp(std::min(width/296.f,height/240.f),.8f,1.5f),cx=width*.5f;
 auto label=[&](const char*t,float y,float size,uint32_t c){text(f,t,cx,y,size*ui,c,true);};
 char num[80];
 if(s.mode==Mode::Title){
  rect(f,0,0,width,16*ui,INK);text(f,"RELIC RUN",std::max(12*ui,safe_x),2*ui+safe_y,.72f*ui,GOLD);text(f,"3D",width-30*ui-safe_x,2*ui+safe_y,.72f*ui,JADE);
  label("遗迹疾奔",height*.12f,2.f,IVORY);label("R E L I C   R U N",height*.12f+36*ui,.7f,GOLD);
  float by=height-73*ui;rect(f,cx-77*ui,by,154*ui,31*ui,0xc69b4e);rect(f,cx-75*ui,by+2*ui,150*ui,27*ui,0xe7bf70);label("开始探险",by+6*ui,1.1f,0x243e32);
  text(f,"操作指南",cx-82*ui,height-32*ui,.85f*ui,IVORY);text(f,quality?"画质 流畅":"画质 精美",cx+16*ui,height-32*ui,.85f*ui,JADE);
  snprintf(num,sizeof(num),"BEST %06u",save.best);label(num,height-15*ui,.65f,GOLD);
 }else if(s.mode==Mode::Running){
  rect(f,std::max(9*ui,safe_x),8*ui+safe_y,93*ui,36*ui,INK);snprintf(num,sizeof(num),"%06u",s.score);text(f,num,std::max(17*ui,safe_x+8*ui),10*ui+safe_y,1.15f*ui,IVORY);
  snprintf(num,sizeof(num),"%um  /  %u",unsigned(s.distance),s.coins);text(f,num,std::max(17*ui,safe_x+8*ui),29*ui+safe_y,.8f*ui,GOLD);
  rect(f,width-38*ui-safe_x,8*ui+safe_y,29*ui,29*ui,INK);rect(f,width-29*ui-safe_x,16*ui+safe_y,3*ui,12*ui,GOLD);rect(f,width-22*ui-safe_x,16*ui+safe_y,3*ui,12*ui,GOLD);
  const char* region=theme==0?"丛林石桥":theme==1?"落日神庙":"幽蓝古墓";text(f,region,cx,10*ui+safe_y,.85f*ui,IVORY,true);
  if(s.next_turn-s.distance<38){float y=height*.24f;rect(f,cx-69*ui,y,138*ui,27*ui,INK);label(s.queued_turn==s.turn_direction?"已准备":s.turn_direction==1?"向右转 >>>":"<<< 向左转",y+5*ui,.95f,s.queued_turn==s.turn_direction?JADE:GOLD);}
  if(s.shield>0||s.magnet>0){snprintf(num,sizeof(num),"%s %ds",s.shield>0?"护盾":"磁铁",int(s.shield>0?s.shield:s.magnet));label(num,height-28*ui,.85f,JADE);}
  if(s.distance<28)label("左右变道   上跳跃   下滑铲",height-20*ui,.82f,IVORY);
 }else {
  float pw=std::min(width-24.f,254*ui),ph=(s.mode==Mode::Help?204.f:172.f)*ui;float x=cx-pw/2,y=(height-ph)/2;
  rect(f,x-2,y-2,pw+4,ph+4,0x937d4c);rect(f,x,y,pw,ph,0x102f2c);rect(f,x+9*ui,y+6*ui,pw-18*ui,1,0xa68d55);
  if(s.mode==Mode::Help){label("操作指南",y+16*ui,1.3f,GOLD);label("左右滑动 / 方向键：变道",y+48*ui,.83f,IVORY);label("向上滑动 / A：跳跃",y+72*ui,.83f,IVORY);label("向下滑动 / B：滑铲",y+96*ui,.83f,IVORY);label("弯口：按提示向左或右",y+120*ui,.83f,IVORY);label("暂停：右上角 / START",y+144*ui,.83f,IVORY);label("轻触返回",y+175*ui,.93f,JADE);
  }else if(s.mode==Mode::Paused){label("暂停旅程",y+20*ui,1.4f,GOLD);label("继续探险",y+65*ui,1.1f,IVORY);label(save.sound?"音效 开":"音效 关",y+99*ui,.9f,JADE);label("返回主菜单",y+137*ui,.87f,IVORY);
  }else{label("旅程结束",y+16*ui,1.35f,GOLD);snprintf(num,sizeof(num),"%06u",s.score);label(num,y+44*ui,1.65f,IVORY);snprintf(num,sizeof(num),"距离 %um    金币 %u",unsigned(s.distance),s.coins);label(num,y+78*ui,.82f,GOLD);
   label(!strcmp(s.reason,"gap")?"坠入深渊":!strcmp(s.reason,"turn")?"错过弯口":"碰到障碍",y+102*ui,.8f,0xccac8a);
   rect(f,cx-68*ui,y+126*ui,136*ui,29*ui,0xe7bf70);label("重来",y+131*ui,1.f,INK);
  }
 }
}
}
