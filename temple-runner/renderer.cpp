// SPDX-License-Identifier: MIT
#include "renderer.hpp"
#include "font_data.hpp"
#include "material_data.hpp"
#include <pxa/game_upload.hpp>
#include <pxa/canvas_geometry.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace relic {
namespace {
constexpr float PI=3.14159265f;
constexpr float ring[9][8][2]={{{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{-0.50000000f,0.86602540f},{-0.50000000f,-0.86602540f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.00000000f,1.00000000f},{-1.00000000f,0.00000000f},{-0.00000000f,-1.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.30901699f,0.95105652f},{-0.80901699f,0.58778525f},{-0.80901699f,-0.58778525f},{0.30901699f,-0.95105652f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.50000000f,0.86602540f},{-0.50000000f,0.86602540f},{-1.00000000f,0.00000000f},{-0.50000000f,-0.86602540f},{0.50000000f,-0.86602540f},{0.00000000f,0.00000000f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.62348980f,0.78183148f},{-0.22252093f,0.97492791f},{-0.90096887f,0.43388374f},{-0.90096887f,-0.43388374f},{-0.22252093f,-0.97492791f},{0.62348980f,-0.78183148f},{0.00000000f,0.00000000f}},{{1.00000000f,0.00000000f},{0.70710678f,0.70710678f},{0.00000000f,1.00000000f},{-0.70710678f,0.70710678f},{-1.00000000f,0.00000000f},{-0.70710678f,-0.70710678f},{-0.00000000f,-1.00000000f},{0.70710678f,-0.70710678f}}};
// Dedicated smooth-light palette, outside the material atlas indices.
constexpr uint32_t actor_colors[16]={0x28786f,0xe7caa0,0x535c71,0xdcad86,0xd5ad70,0x6a4832,0x9c6743,0x70503b,0x2b342f,0x4d3328,0xc9493b,0xe8c378,0x82918a,0x263b34,0xaf7b4c,0x9e7553};
constexpr V3 actor_ring12[12]={{1,0,0},{.8660254f,0,.5f},{.5f,0,.8660254f},{0,0,1},{-.5f,0,.8660254f},{-.8660254f,0,.5f},{-1,0,0},{-.8660254f,0,-.5f},{-.5f,0,-.8660254f},{0,0,-1},{.5f,0,-.8660254f},{.8660254f,0,-.5f}};
constexpr float ridge_y[3][11]={{0.36000000f,0.31949747f,0.32296910f,0.34738321f,0.35330489f,0.35539341f,0.38308702f,0.41022037f,0.39175264f,0.34494339f,0.32530029f},{0.34674606f,0.34131518f,0.37232507f,0.39132600f,0.39066246f,0.40469304f,0.43035473f,0.42203213f,0.37535777f,0.34465797f,0.35723324f},{0.36264757f,0.39421442f,0.42632144f,0.42867211f,0.42949762f,0.44775623f,0.44826090f,0.40766459f,0.36839069f,0.37532064f,0.40560126f}};
constexpr float arch_ring[7][2]={{1,0},{.8660254f,.5f},{.5f,.8660254f},{0,1},{-.5f,.8660254f},{-.8660254f,.5f},{-1,0}};
constexpr uint32_t GOLD=0xf4c66b, IVORY=0xf6efd7, INK=0x102c29, JADE=0x78d4ac;
uint16_t rgb(uint32_t c){return uint16_t(((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31));}
uint32_t fog_color=0x728f7b;
uint32_t lit(uint32_t c,float l,float fog){
 int r=int(((c>>16)&255)*l),g=int(((c>>8)&255)*l),b=int((c&255)*l);
 return uint32_t(std::clamp(int(r*(1-fog)+((fog_color>>16)&255)*fog),0,255))<<16|uint32_t(std::clamp(int(g*(1-fog)+((fog_color>>8)&255)*fog),0,255))<<8|std::clamp(int(b*(1-fog)+(fog_color&255)*fog),0,255);
}
V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 mul(V3 a,float f){return {a.x*f,a.y*f,a.z*f};}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm(V3 a){float l=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return l>.0001f?mul(a,1/l):V3{1,0,0};}
uint32_t nextchar(const char*& p){uint32_t c=uint8_t(*p++);if(c<128)return c;int n=c<224?1:c<240?2:3;c&=n==1?31:n==2?15:7;while(n--&&*p)c=(c<<6)|(uint8_t(*p++)&63);return c;}
int glyph(uint32_t c){if(c>=32&&c<127)return int(c)-32;for(unsigned i=0;i<std::size(font::codes);i++)if(font::codes[i]==c)return i;return 0;}
}
bool Renderer::upload(pxa::game::Renderer& target){
 static std::byte scratch[65556];uint16_t colors[8*256]{};
 for(int level=0;level<8;level++)for(int i=0;i<256;i++){
  uint16_t c=i>=208&&i<224?rgb(actor_colors[i-208]):materials::palette[i];float l=.36f+level*.105f;
  int r=int((c>>11)*l),g=int(((c>>5)&63)*l),b=int((c&31)*l);
  colors[level*256+i]=uint16_t(std::min(r,31)<<11|std::min(g,63)<<5|std::min(b,31));
 }
 pxa::game::Upload u(target,scratch);text_alpha=target.supports(pxa::game::RenderCapability::sprite_texel_alpha);
 textures=target.supports(pxa::game::RenderCapability::painter_polygon)&&target.supports(pxa::game::RenderCapability::textured_quad);
 if(!u.palette(colors,8)||!u.texture({0},256,256,std::as_bytes(std::span{font::atlas}))||!u.texture({1},128,36,std::as_bytes(std::span{font::title_atlas})))return false;
 if(textures&&!u.texture({2},256,128,std::as_bytes(std::span{materials::atlas})))textures=false;
 depth=depth&&target.supports(pxa::game::RenderCapability::painter_depth)&&target.supports(pxa::game::RenderCapability::lit_palette_depth);
 const uint8_t actor_texels[16]={208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223};
 if(depth&&!u.texture({4},16,1,std::as_bytes(std::span{actor_texels})))depth=false;
 if constexpr(font::extra_height>0)if(!u.texture({3},256,font::extra_height,std::as_bytes(std::span{font::atlas_extra})))return false;
 return true;
}
void Renderer::rect(pxa::game::Frame& f,float x,float y,float w,float h,uint32_t c){
 if(w<=0||h<=0)return;
 if(drawing_ui&&overlay&&display){
  float rx=float(display->width)/width,ry=float(display->height)/height;
  auto box=pxa::ui::surface_to_canvas_region({int(x*rx),int(y*ry),std::max(1,int(w*rx+.5f)),std::max(1,int(h*ry+.5f))},*display);
  overlay->rect(box.x,box.y,box.width,box.height,(c<<8)|255);return;
 }
 f.quad({int16_t(x*16),int16_t(y*16),int16_t((x+w)*16),int16_t(y*16),int16_t((x+w)*16),int16_t((y+h)*16),int16_t(x*16),int16_t((y+h)*16)},{rgb(c)});
}
void Renderer::text(pxa::game::Frame& f,const char* str,float x,float y,float scale,uint32_t c,bool center){
 if(drawing_ui&&overlay&&display){
  float rx=float(display->width)/width,ry=float(display->height)/height;
  // Size is expressed in physical display pixels. Never enlarge a 14 px
  // bitmap through the half-resolution 3D viewport on the 480x854 P4 panel.
  int font_px=std::clamp(int(14*scale*ry+.5f),display->width>=400?20:11,96);
  const char* p=str;float pixels=0;while(*p)pixels+=font::advance[glyph(nextchar(p))]*(font_px/14.f);
  int bw=std::max(1,int(pixels+4));float start=x*rx-(center?pixels*.5f:0);
  auto box=pxa::ui::surface_to_canvas_region({int(start),int(y*ry),bw,font_px+5},*display);
  overlay->text_sized(box,(c<<8)|255,str,uint16_t(font_px),center?1:0);return;
 }
 if(!strcmp(str,"遗迹疾奔")){
  float factor=scale*.5f;if(center)x-=56*factor;pxa::game::Sprite sprites[4];
  for(int i=0;i<4;i++){auto&g=sprites[i];g.x=int16_t(x+i*28*factor);g.y=int16_t(y);g.width=uint16_t(32*factor);g.height=uint16_t(36*factor);g.source_x=i*32;g.source_width=32;g.source_height=36;}
  pxa::game::SpriteOptions o;o.solid_color=true;o.texel_alpha=text_alpha;o.color={rgb(c)};f.sprites({1},sprites,o);return;
 }
 const char* p=str;float len=0;while(*p)len+=font::advance[glyph(nextchar(p))]*scale;if(center)x-=len/2;
 pxa::game::Sprite sprites[2][100];int counts[2]={};int n=0;p=str;
 while(*p&&n<100){int i=glyph(nextchar(p));auto &g=sprites[i/256][counts[i/256]++];n++;g.x=int16_t(x);g.y=int16_t(y);g.width=uint16_t(std::max(1,int(16*scale)));g.height=g.width;g.source_x=(i%16)*16;g.source_y=((i/16)%16)*16;g.source_width=g.source_height=16;x+=font::advance[i]*scale;}
 pxa::game::SpriteOptions o;o.solid_color=true;o.texel_alpha=text_alpha;o.color={rgb(c)};for(int page=0;page<2;page++)if(counts[page])f.sprites({uint8_t(page?3:0)},{sprites[page],size_t(counts[page])},o);
}
void Renderer::face(V3 a,V3 b,V3 c,V3 d,uint32_t color,float light,int material){
 if(count>=int(std::size(faces))){skipped++;return;}
 const float fog_z=(a.z+b.z+c.z+d.z)*.25f;
 // Keep road overlays ordered in road coordinates. A heading-dependent
 // average depth lets the full-width bridge deck cover its raised lane tiles.
 // Per-vertex camera depth below remains authoritative for actors/hazards.
 const float painter_z=fog_z-.043f*(a.y+b.y+c.y+d.y);
 V3 src[8]={a,b,c,d},dst[8];float u0=material>=0?(material%4)*64.f:0,v0=material>=0?(material/4)*64.f:0;
 float uv0[4][2]={{u0,v0+63},{u0+63,v0+63},{u0+63,v0},{u0,v0}},uv1[8][2];int m=0;
 // Transform once, then clip in camera space. A turning camera must use the
 // same near plane for projection, interpolation and the shared actor depth.
 for(int i=0;i<4;i++){
  float dx=src[i].x-camera_x,z=src[i].z+camera_back,dy=src[i].y-camera_height-bob;
  float cx=dx*view_cos-z*view_sin,cz=dx*view_sin+z*view_cos;
  src[i]={cx,dy*.985f+cz*.172f,cz*.985f-dy*.172f};
 }
 constexpr float near=1.0f;
 for(int i=0;i<4;i++){auto p=src[i],q=src[(i+1)%4];bool pin=p.z>near,qin=q.z>near;
  if(pin){dst[m]=p;uv1[m][0]=uv0[i][0];uv1[m++][1]=uv0[i][1];}
  if(pin!=qin){float t=(near-p.z)/(q.z-p.z);dst[m]=add(p,mul(sub(q,p),t));int j=(i+1)%4;
   uv1[m][0]=uv0[i][0]+t*(uv0[j][0]-uv0[i][0]);uv1[m++][1]=uv0[i][1]+t*(uv0[j][1]-uv0[i][1]);}}
 if(m<3)return;
 float z=0,xy[8][2];uint16_t camera_depth[8];float minx=1e6f,maxx=-1e6f,miny=1e6f,maxy=-1e6f;
 for(int i=0;i<m;i++){
  z+=dst[i].z;camera_depth[i]=pxa::game::painter_depth_from_z(dst[i].z);
  float inv=focal/dst[i].z,xx=dst[i].x*inv,sy=-dst[i].y*inv;
  float sx=width*.5f+xx+sy*roll,sy2=screen_y+sy-xx*roll;
  xy[i][0]=std::clamp(sx,-1500.f,1500.f);xy[i][1]=std::clamp(sy2,-1500.f,1500.f);
  minx=std::min(minx,sx);maxx=std::max(maxx,sx);miny=std::min(miny,sy2);maxy=std::max(maxy,sy2);
 }
 if(maxx<0||minx>width||maxy<0||miny>height)return;
 z/=m;uint16_t col=rgb(lit(color,light,std::clamp((fog_z-12)/84.f,0.f,.76f)));
 auto emit=[&](int i,int j,int k,int l){if(count>=int(std::size(faces))){skipped++;return;}Face &f=faces[count];int ids[4]={i,j,k,l};for(int t=0;t<4;t++){f.xy[t*2]=int16_t(xy[ids[t]][0]*16);f.xy[t*2+1]=int16_t(xy[ids[t]][1]*16);f.depths[t]=camera_depth[ids[t]];f.uv[t*2]=int16_t(uv1[ids[t]][0]*16);f.uv[t*2+1]=int16_t(uv1[ids[t]][1]*16);}int64_t area=0;for(int t=0;t<4;t++){int u=(t+1)%4;area+=int64_t(f.xy[t*2])*f.xy[u*2+1]-int64_t(f.xy[u*2])*f.xy[t*2+1];}if(std::abs(area)<5)return;f.depth=painter_z;f.color=col;f.layer=mesh_layer;f.material=textures&&fog_z<24?material:-1;f.light=uint8_t(std::clamp(int(light*6)-(theme==2?2:0),0,7));count++;};
 if(m==4)emit(0,1,2,3);else for(int i=1;i+1<m;i++)emit(0,i,i+1,i+1);
}
void Renderer::box(float x,float y,float z,float w,float h,float dep,uint32_t c){
 float l=x-w/2,r=x+w/2,b=y,t=y+h,n=z-dep/2,f=z+dep/2;
 face({l,t,n},{r,t,n},{r,t,f},{l,t,f},c,1.12f);
 face({l,b,n},{r,b,n},{r,t,n},{l,t,n},c,.77f);
 if(x>camera_x)face({l,b,f},{l,b,n},{l,t,n},{l,t,f},c,.68f);
 else face({r,b,n},{r,b,f},{r,t,f},{r,t,n},c,.95f);
}
bool Renderer::visible_box(V3 center,V3 half) const{
 if(!object_culling)return true;
 for(const auto& p:view_planes){
  float distance=motion::dot(p.normal,center)+p.offset;
  float radius=motion::dot(p.absolute,add(half,{.05f,.05f,.05f}));
  if(distance+radius<-.02f)return false;
 }
 return true;
}
void Renderer::limb(V3 a,V3 b,float radius,uint32_t color,int sides,bool closed){
 V3 dir=norm(sub(b,a)),u=norm(cross(dir,{0,0,1})),v=cross(dir,u);V3 front[10],back[10];
 for(int i=0;i<sides;i++){V3 off=mul(add(mul(u,ring[sides][i][0]),mul(v,ring[sides][i][1])),radius);front[i]=add(a,off);back[i]=add(b,off);}
 for(int i=0;i<sides;i++){int j=(i+1)%sides;V3 normal=mul(sub(front[i],a),1/radius);V3 center=mul(add(front[i],back[i]),.5f);V3 eye={camera_x,camera_height,-camera_back};if(normal.x*(eye.x-center.x)+normal.y*(eye.y-center.y)+normal.z*(eye.z-center.z)<-.1f)continue;
  face(front[i],front[j],back[j],back[i],color,.82f+.22f*normal.y-.14f*normal.x-.13f*normal.z);}
 if(closed){
  for(int end=0;end<2;end++){V3 n=mul(dir,end?1.f:-1.f),center=end?b:a,eye=sub({camera_x,camera_height,-camera_back},center);
   if(n.x*eye.x+n.y*eye.y+n.z*eye.z<0)continue;
   auto* points=end?back:front;
   for(int i=1;i+1<sides;i+=2){int k=std::min(i+2,sides-1);face(points[0],points[i],points[i+1],points[k],color,end?1.f:.85f);}
  }
 }else if(dir.y>0)for(int i=1;i+1<sides;i+=2){int k=std::min(i+2,sides-1);face(back[0],back[i],back[i+1],back[k],color,1.13f);}
}
void Renderer::orb(V3 c,V3 radii,int color,int sides){
 V3 points[3][8],normals[3][8];const float ys[3]={-.82f,0,.82f},rs[3]={.57f,1,.57f};
 for(int j=0;j<3;j++)for(int i=0;i<sides;i++){
  float x=ring[sides][i][0],z=ring[sides][i][1];points[j][i]=add(c,{radii.x*x*rs[j],radii.y*ys[j],radii.z*z*rs[j]});
  normals[j][i]=norm({x*rs[j]/radii.x,ys[j]/radii.y,z*rs[j]/radii.z});
 }
 auto emit=[&](V3 a,V3 b,V3 d,V3 e,V3 na,V3 nb,V3 nd,V3 ne){
  V3 mid=mul(add(add(a,b),add(d,e)),.25f),outward=add(add(na,nb),add(nd,ne)),eye=sub({camera_x,camera_height,-camera_back},mid);
  V3 n=add(cross(sub(b,a),sub(d,a)),cross(sub(d,a),sub(e,a)));if(n.x*outward.x+n.y*outward.y+n.z*outward.z<0)n=mul(n,-1);
  if(n.x*eye.x+n.y*eye.y+n.z*eye.z<0)return;int before=count;
  face(a,b,d,e,actor_colors[color],.9f);
  if(count==before+1&&textures){auto& f=faces[before];f.material=-2;f.palette=208+color;
   auto light=[](V3 n){return uint8_t(std::clamp(int(4.9f+1.4f*n.y-1.25f*n.x-1.3f*n.z),0,7));};f.shades={light(na),light(nb),light(nd),light(ne)};}
 };
 for(int j=0;j<2;j++)for(int i=0;i<sides;i++){int k=(i+1)%sides;emit(points[j][i],points[j][k],points[j+1][k],points[j+1][i],normals[j][i],normals[j][k],normals[j+1][k],normals[j+1][i]);}
 for(int j: {0,2}){V3 n={0,j?1.f:-1.f,0};for(int i=1;i+1<sides;i+=2){int k=std::min(i+2,sides-1);emit(points[j][0],points[j][i],points[j][i+1],points[j][k],n,n,n,n);}}
}
void Renderer::monster(float x,float z){
 if(!visible_box({x,1.1f,z-.5f},{.8f,1.6f,1.55f})){culled_objects++;return;}
 float gait=time*12,bounce=.055f*cosf(gait*2),y=.66f+bounce;uint32_t coat=0x263b38;
 orb({x,y,z},{.37f,.34f,.66f},8);orb({x,y+.24f,z+.52f},{.30f,.30f,.29f},13);
 orb({x,y+.12f,z+.73f},{.24f,.16f,.22f},8,6);
 for(int side: {-1,1}){
  float xx=x+side*.23f;
  face({xx-.08f,y+.43f,z+.48f},{xx+.08f,y+.43f,z+.48f},{xx,y+.68f,z+.56f},{xx-.08f,y+.43f,z+.48f},0x49726a);
  box(x+side*.16f,y+.26f,z+.805f,.09f,.065f,.035f,0xf6ce6f);
  for(int front: {-1,1}){
   float phase=gait+(side>0?PI:0)+(front<0?.75f*PI:0),swing=sinf(phase),lift=std::max(0.f,cosf(phase))*.18f;
   V3 h={x+side*.25f,y-.05f,z+front*.42f},k={x+side*.34f,.33f+lift*.4f,z+front*.43f+swing*.15f},foot={x+side*.34f,.10f+lift,z+front*.42f-swing*.28f};
   limb(h,k,.105f,coat,5,true);limb(k,foot,.075f,coat,5,true);orb(k,{.105f,.11f,.105f},13,6);
   box(foot.x,foot.y-.07f,foot.z+.04f,.20f,.13f,.27f,0x304c46);
  }
 }
 V3 tail={x,y+.05f,z-.53f};
 for(int i=0;i<3;i++){V3 next={x+sinf(time*3+i*.6f)*(.10f+i*.03f),y+.14f+i*.14f,z-.83f-i*.22f};limb(tail,next,.055f-i*.01f,i==2?0xb49757:coat,5,true);tail=next;}
 for(int i=0;i<3;i++){float zz=z-.36f+i*.3f;face({x-.14f,y+.28f,zz-.1f},{x+.14f,y+.28f,zz-.1f},{x+.08f,y+.52f,zz+.03f},{x-.08f,y+.52f,zz+.03f},0x5b9b87,1.05f);}
}
void Renderer::panel(pxa::game::Frame& f,float x,float y,float w,float h,uint32_t fill,uint32_t border){
 float u=ui_scale();
 const bool primary=fill==0xdab66e||fill==0xe7bf70;
 const auto* skin=primary?button_skin:panel_skin;
 if(drawing_ui&&overlay&&display&&skin){
  float rx=float(display->width)/width,ry=float(display->height)/height;
  auto box=pxa::ui::surface_to_canvas_region({int(x*rx),int(y*ry),std::max(1,int(w*rx)),std::max(1,int(h*ry))},*display);
  int corner=std::max(2,pxa::ui::surface_to_canvas_region({0,0,std::max(1,int(8*u*rx)),std::max(1,int(8*u*ry))},*display).width);
  corner=std::min(corner,std::min(box.width,box.height)/2);
  const int sw=skin->descriptor().width,sh=skin->descriptor().height;
  if(primary)corner=std::min(box.width/2,std::max(2,int(std::lround(box.height*64.f/sh))));
  const int inset=primary?64:24;
  const int sx[4]={0,inset,sw-inset,sw};
  const int sy[4]={0,primary?sh:24,sh-24,sh};
  const int dx[4]={box.x,box.x+corner,box.x+box.width-corner,box.x+box.width};
  const int dy[4]={box.y,primary?box.y+box.height:box.y+corner,box.y+box.height-corner,box.y+box.height};
  for(int row=0;row<(primary?1:3);row++)for(int col=0;col<3;col++){
   int dw=dx[col+1]-dx[col],dh=dy[row+1]-dy[row];if(dw<=0||dh<=0)continue;
   int iw=std::max(1,int(std::lround(float(sw)*dw/(sx[col+1]-sx[col]))));
   int ih=std::max(1,int(std::lround(float(sh)*dh/(sy[row+1]-sy[row]))));
   int ix=dx[col]-int(std::lround(float(sx[col])*dw/(sx[col+1]-sx[col])));
   int iy=dy[row]-int(std::lround(float(sy[row])*dh/(sy[row+1]-sy[row])));
   // Native Canvas has clipping and image transforms. Nine clipped image
   // transforms share one resident texture; no nine separate copies/assets.
   overlay->clip({dx[col],dy[row],dw+(col<2?1:0),dh+(row<2&&!primary?1:0)}).image({ix,iy,iw,ih},*skin,pxa::ui::ImageFit::stretch).pop_clip();
  }
  return;
 }
 rect(f,x,y+2*u,w,h,0x09221f);rect(f,x,y,w,h,border);rect(f,x+u,y+u,w-2*u,h-2*u,fill);
 rect(f,x+4*u,y+3*u,w-8*u,u,lit(border,.85f,0));
 for(int side: {-1,1}){float xx=side<0?x+3*u:x+w-10*u;rect(f,xx,y+3*u,7*u,u,GOLD);rect(f,side<0?xx:xx+6*u,y+3*u,u,5*u,GOLD);}
}
void Renderer::tree(float x,float z,int variant){
 float h=5+(variant%4)*.8f;
 if(!visible_box({x+.4f,(h-1.7f)*.5f,z},{4.3f,(h+2.3f)*.5f,4.3f})){culled_objects++;return;}
 limb({x,-2,z},{x+.4f,h,z},.24f,0x514a2c,5);
 int leaves=(z<27&&quality==0)?6:4;float cv=cosf(float(variant)),sv=sinf(float(variant));
 for(int i=0;i<leaves;i++){float dx=ring[leaves][i][0]*cv-ring[leaves][i][1]*sv,dz=ring[leaves][i][1]*cv+ring[leaves][i][0]*sv;V3 root={x+.4f,h,z};V3 tip={x+dx*3.8f,h-1.25f,z+dz*3.8f};V3 mid={x+dx*1.9f,h+.3f,z+dz*1.9f};float w=.7f;
 face(root,{mid.x-dz*w,mid.y,mid.z+dx*w},tip,{mid.x+dz*w,mid.y-.16f,mid.z-dx*w},(i&1)?0x297252:0x3b8c59,1.f);
 }
 if(z<22&&quality==0&&count<730){
  float vx=x+.9f,vy=h-.8f;
  face({vx-.025f,vy,z},{vx+.025f,vy,z},{vx+.16f,vy-2.5f,z},{vx+.1f,vy-2.5f,z},0x3d6b4c);
  for(int i=0;i<2;i++){float yy=vy-.7f-i*.8f,dx=i?-.24f:.24f;face({vx,yy,z},{vx+dx,yy-.12f,z-.04f},{vx+dx*.4f,yy-.43f,z-.06f},{vx,yy-.18f,z},i?0x568a58:0x6c9f66);}
 }
 if(z<32){box(x,-1,z,1.7f,.3f,1.5f,0x264c3d);}
}
void Renderer::idol(float x,float z,float s){
 if(!visible_box({x,1.5f*s,z},{1.05f*s,2.1f*s+.6f,s})){culled_objects++;return;}
 uint32_t stone=0x7d8972;
 if(z>20){
  box(x,-.6f,z,1.7f*s,.7f*s,1.5f*s,stone);box(x,.1f,z,1.15f*s,1.8f*s,.92f*s,stone);
  box(x,1.9f*s,z,1.45f*s,1.35f*s,1.05f*s,stone);box(x,3.25f*s,z,1.8f*s,.26f*s,1.35f*s,0xb3ad83);
  for(int side: {-1,1})face({x+side*.39f*s-.17f*s,2.6f*s,z-.535f*s},{x+side*.39f*s+.17f*s,2.6f*s,z-.535f*s},{x+side*.39f*s+.17f*s,2.77f*s,z-.535f*s},{x+side*.39f*s-.17f*s,2.77f*s,z-.535f*s},0x253a2d);
  return;
 }
 box(x,-.6f,z,1.7f*s,.7f*s,1.5f*s,stone);box(x,.1f,z,1.15f*s,1.8f*s,.92f*s,stone);
 orb({x,2.60f*s,z},{.71f*s,.76f*s,.57f*s},12,6);
 for(int side: {-1,1})box(x+side*.54f*s,3.05f*s,z,.27f*s,.42f*s,.34f*s,0x748a80);
 box(x,3.25f*s,z,1.8f*s,.26f*s,1.35f*s,0xb3ad83);
 box(x-.39f*s,2.62f*s,z-.56f*s,.35f*s,.15f*s,.12f*s,0x253a2d);
 box(x+.39f*s,2.62f*s,z-.56f*s,.35f*s,.15f*s,.12f*s,0x253a2d);
 box(x,2.22f*s,z-.65f*s,.3f*s,.4f*s,.3f*s,0xa5a17f);
 box(x,2.02f*s,z-.56f*s,.7f*s,.12f*s,.14f*s,0x394a3b);
 face({x-.34f*s,.9f*s,z-.48f*s},{x+.34f*s,.9f*s,z-.48f*s},{x+.34f*s,1.58f*s,z-.48f*s},{x-.34f*s,1.58f*s,z-.48f*s},0x7da58e,1.f,3);
}
void Renderer::runner(const State& s){
#include "player_model.inc"
}
void Renderer::render(pxa::game::Frame& f,const State& s,const Save& save){
 drawing_ui=false;count=0;mesh_layer=0;theme=int(s.distance/400)%3;camera_x=s.lane_x*.3f+s.turn_lean*.10f;roll=s.turn_lean*.075f;
 view_cos=cosf(s.turn_lean*.23f);view_sin=sinf(s.turn_lean*.23f);
 focal=std::min(width,height)*.98f;screen_y=height*(.38f+.10f*std::clamp((float(height)/width-1)*1.5f,0.f,1.f));
 bob=s.mode==Mode::Running&&s.jump_t<=0?sinf(s.elapsed*13.4f)*.018f:0;
 // Conservative object bounds reject a whole prop before mesh construction.
 // These are the same five camera planes used by projection/near clipping,
 // including the roll and heading during a corner.
 culled_objects=0;
 V3 rx={view_cos,0,-view_sin},ry={view_sin*.172f,.985f,view_cos*.172f},rz={view_sin*.985f,-.172f,view_cos*.985f};
 V3 horizontal=sub(rx,mul(ry,roll)),vertical=sub(mul(ry,-1),mul(rx,roll));
 V3 normals[5]={rz,add(horizontal,mul(rz,width*.5f/focal)),add(mul(horizontal,-1),mul(rz,width*.5f/focal)),add(vertical,mul(rz,screen_y/focal)),add(mul(vertical,-1),mul(rz,(height-screen_y)/focal))};
 for(int i=0;i<5;i++){
  V3 n=normals[i];view_planes[i]={n,{std::abs(n.x),std::abs(n.y),std::abs(n.z)},-motion::dot(n,{camera_x,camera_height+bob,-camera_back})-(i==0?1.f:0.f)};
 }
 fog_color=theme==0?0xa7b79a:theme==1?0xc4a17d:0x3c6270;
 travel=s.distance;
 uint32_t sky=theme==1?0xd5a47f:theme==2?0x1b3545:0xa8bda0;
 f.clear({rgb(sky)});
 if(!(s.mode==Mode::Title&&title_background&&overlay&&display)){
 for(int i=0;i<8;i++){uint32_t c=lit(sky,.78f+i*.025f,0);rect(f,0,i*height*.07f,width,height*.072f,c);}
 if(theme!=2){float sx=width*.77f,sy=height*.17f,r=height*.055f;
  for(int i=0;i<8;i++){int j=(i+1)%8;
   f.quad({int16_t(sx*16),int16_t(sy*16),int16_t((sx+ring[8][i][0]*r)*16),int16_t((sy+ring[8][i][1]*r)*16),int16_t((sx+ring[8][j][0]*r)*16),int16_t((sy+ring[8][j][1]*r)*16),int16_t(sx*16),int16_t(sy*16)}, {rgb(theme==1?0xffdf9e:0xe2dfb0)});
  }
 }
 // Broad layered ridges give the skyline depth without repeated triangular peaks.
 for(int layer=0;layer<3;layer++)for(int i=0;i<10;i++){
  float x0=i*width/10.f,x1=(i+1)*width/10.f;
  uint32_t c=theme==2?(layer==0?0x314c5b:layer==1?0x294650:0x233f49):(layer==0?0x91ac95:layer==1?0x698f77:0x416e59);
  f.quad({int16_t(x0*16),int16_t(height*ridge_y[layer][i]*16),int16_t(x1*16),int16_t(height*ridge_y[layer][i+1]*16),int16_t(x1*16),int16_t(height*.56f*16),int16_t(x0*16),int16_t(height*.56f*16)},{rgb(c)});
 }
 uint32_t stone=theme==1?0xbaa577:theme==2?0x6b8290:0x81958b;
 runner(s);
 if((s.mode==Mode::Running&&s.elapsed<3.f)||s.chase>0||s.mode==Mode::Dead){
  mesh_layer=2;
  float z=s.mode==Mode::Dead?-.8f-std::max(0.f,1.f-s.death_t)*1.8f:s.chase>0?-2.8f:-.3f-s.elapsed*.9f;
  monster(-1.2f,z);
 }
 mesh_layer=0;
 // River under the elevated causeway and mossy banks; actual 3D planes.
 face({-90,-3,-4},{90,-3,-4},{90,-3,95},{-90,-3,95},theme==1?0x698d80:theme==2?0x203e4d:0x4d9183);
 for(int side=-1;side<=1;side+=2)for(int i=0;i<6;i++){
  float z=i*15.f-fmodf(travel,15.f),x=side*(7.f+(i%2)*3);
  face({x-4,-2,z-9},{x+4,-2,z-9},{x+3,-1.8f,z+9},{x-3,-1.8f,z+9},theme==1?0x727d4a:0x3f684c);
 }
 if(quality==0)for(int side: {-1,1})for(int band=0;band<2;band++){
  float z=12+band*16-fmodf(travel*.22f,7.f),x=side*(10.f+band*3),w=3.f+band;
  face({x-w,-2.96f,z},{x+w,-2.96f,z},{x+w-.6f,-2.96f,z+.18f},{x-w+.6f,-2.96f,z+.18f},theme==2?0x416d79:0x75a799,.88f);
 }
 float turnz=s.next_turn-s.distance;bool elbow=s.mode!=Mode::Title&&turnz<75;
 const float tile=3.2f;float startz=-4.f-fmodf(travel,tile);
 for(int i=0;i<25;i++){float z=startz+i*tile;if(elbow&&z>turnz+2)continue;
  bool gap=false;for(int j=0;j<s.count;j++)if(s.objects[j].kind==Kind::Gap&&fabsf((s.objects[j].distance-s.distance)-z)<2.3f)gap=true;
  if(gap)continue;
  // Distant paving is only a few pixels tall. Keep the bridge silhouette and
  // borders, reserving the geometry budget for the explorer and hazards.
  if(z>24){
   float far=z+1.6f;bool next_gap=false;
   for(int j=0;j<s.count;j++)if(s.objects[j].kind==Kind::Gap&&fabsf((s.objects[j].distance-s.distance)-(z+tile))<2.3f)next_gap=true;
   if(z>36&&i+1<25&&!next_gap&&(!elbow||z+tile<turnz+2)){far+=tile;i++;}
   face({-2.9f,.01f,z-1.6f},{2.9f,.01f,z-1.6f},{2.9f,.01f,far},{-2.9f,.01f,far},stone,.97f);
   for(int side: {-1,1}){float rx=side*2.86f;face({rx-.08f,.15f,z-1.6f},{rx+.08f,.15f,z-1.6f},{rx+.08f,.15f,far},{rx-.08f,.15f,far},0xd0bc7f);}
   continue;
  }
  // Raised stone bridge with individually shaded paving, moss and golden borders.
  face({-2.9f,-.68f,z-1.6f},{-2.9f,-.68f,z+1.6f},{-2.9f,-.03f,z+1.6f},{-2.9f,-.03f,z-1.6f},stone,.65f);
  face({2.9f,-.68f,z+1.6f},{2.9f,-.68f,z-1.6f},{2.9f,-.03f,z-1.6f},{2.9f,-.03f,z+1.6f},stone,.8f);
  face({-2.9f,-.025f,z-1.6f},{2.9f,-.025f,z-1.6f},{2.9f,-.025f,z+1.6f},{-2.9f,-.025f,z+1.6f},stone,.66f);
  int row=int(travel/tile)+i;
  for(int lane=-1;lane<=1;lane++){
   float x=lane*1.8f;uint32_t pattern=uint32_t(row)*1103515245u+uint32_t(lane+1)*2654435761u;
   int material=theme==1?4:theme==2?5:0;
   if(lane==0&&row%7==0)material=7;else if(pattern%13==1)material=6;else if(lane&&pattern%5==0)material=1;
   uint32_t c=lit(stone,.92f+((row+lane)&3)*.04f,0);
   face({x-.85f,.01f,z-1.53f},{x+.85f,.01f,z-1.53f},{x+.85f,.01f,z+1.53f},{x-.85f,.01f,z+1.53f},c,1.f,material);
  }
  for(int side=-1;side<=1;side+=2){
   float rx=side*2.86f;face({rx-.08f,.15f,z-1.57f},{rx+.08f,.15f,z-1.57f},{rx+.08f,.15f,z+1.57f},{rx-.08f,.15f,z+1.57f},0xd0bc7f);
   if(z<12)face({rx-.08f,.02f,z-1.57f},{rx+.08f,.02f,z-1.57f},{rx+.08f,.15f,z-1.57f},{rx-.08f,.15f,z-1.57f},0xa29566);
   if(z<40&&(i+int(travel/tile))%4==0){box(side*3.f,-.05f,z,.45f,.65f,.5f,stone);if(z<9)box(side*3.f,.6f,z,.55f,.12f,.6f,0xb7b388);else{float xx=side*3.f;face({xx-.275f,.72f,z-.3f},{xx+.275f,.72f,z-.3f},{xx+.275f,.72f,z+.3f},{xx-.275f,.72f,z+.3f},0xb7b388);}}
  }
 }
 if(elbow&&turnz>5){float signx=s.turn_direction*2.8f;
  box(signx,.45f,turnz-5,.1f,1.7f,.1f,0x655235);box(signx,1.6f,turnz-5,1.15f,.7f,.14f,0xbca36c);
  float dx=s.turn_direction*.32f;face({signx-dx,1.8f,turnz-5.08f},{signx+dx,1.95f,turnz-5.08f},{signx-dx,2.12f,turnz-5.08f},{signx-dx,1.8f,turnz-5.08f},0x264f45);
 }
 if(elbow){float x=s.turn_direction*12.f;box(x,-.65f,turnz,24,.64f,5.8f,stone);box(0,0,turnz+2.8f,5.8f,.6f,.4f,0xb4a77c);
  for(int strip=-1;strip<=1;strip++){float z=turnz+strip*1.8f;face({0,.015f,z-.85f},{x*2,.015f,z-.85f},{x*2,.015f,z+.85f},{0,.015f,z+.85f},stone,1,theme==1?4:theme==2?5:0);}
  if(turnz<24)for(int mark=0;mark<2;mark++){
   float z=turnz-5-mark*3.f,dx=s.turn_direction*.65f;
   face({-dx,.035f,z-.45f},{dx,.035f,z},{-dx,.035f,z+.45f},{-dx*.65f,.035f,z},0xdcbf73,1.1f);
  }
 }
 if(s.turns>0&&std::abs(s.turn_fx)>.05f){float z=s.next_turn-180-s.distance,x=-std::copysign(12.f,s.turn_fx),edge=-std::copysign(2.9f,s.turn_fx);
  face({edge,.012f,z-2.9f},{x*2,.012f,z-2.9f},{x*2,.012f,z+2.9f},{edge,.012f,z+2.9f},stone,.96f);
  for(int side: {-1,1})face({edge,.15f,z+side*2.86f-.08f},{x*2,.15f,z+side*2.86f-.08f},{x*2,.15f,z+side*2.86f+.08f},{edge,.15f,z+side*2.86f+.08f},0xd0bc7f);
 }
 for(int side=-1;side<=1;side+=2){
  for(int i=0;i<(quality==0?3:2);i++){float z=7+i*12-fmodf(travel,12.f);float x=side*(5.f+(i%3)*1.5f);
   if(theme==2){box(x,-1,z,1.2f,5.9f,1.4f,0x4b6570);box(x,4.9f,z,1.7f,.4f,1.8f,0x87938b);if(i%2==0&&side<0)box(0,5.3f,z,12,.4f,1.5f,0x3f5664);}
   else {tree(x,z,i+side*3);if(theme==1&&i%2==0)box(x+side*2,-.5f,z,2.2f,3.f,2.4f,0x9b8b61);}
   if(z<20&&z>0&&quality==0&&theme!=2)for(int leaf=0;leaf<5;leaf++){
    float dx=ring[5][leaf][0]*.9f,dz=ring[5][leaf][1]*.8f;
    V3 base={x,-.65f,z},tip={x+dx,-.35f,z+dz};
    face(base,{x+dx*.55f-dz*.13f,.12f,z+dz*.55f+dx*.13f},tip,{x+dx*.55f+dz*.13f,.03f,z+dz*.55f-dx*.13f},leaf&1?0x407857:0x6a9c68);
   }
   if(theme==2&&z>0&&z<20)face({x-.4f,1.1f,z-.72f},{x+.4f,1.1f,z-.72f},{x+.4f,2.5f,z-.72f},{x-.4f,2.5f,z-.72f},0x6faaa0,.95f,3);
   if(theme==1&&z>2&&z<24&&i%2==0)face({x+side*2-1.f,.5f,z-1.21f},{x+side*2+1.f,.5f,z-1.21f},{x+side*2+1.f,2.3f,z-1.21f},{x+side*2-1.f,2.3f,z-1.21f},0xb7a36b,1,4);
}
  for(int i=0;i<2;i++){float z=14+i*30-fmodf(travel,30.f);if(z< -3)continue;
   if(i==1)idol(side*4.3f,z,1.05f);
   if(!visible_box({side*3.4f,1.3f,z},{.5f,1.5f,.6f})){culled_objects++;continue;}
   // Braziers are geometric flames with a flickering amber core.
   box(side*3.4f,.05f,z,.35f,1.55f,.35f,0x545e47);box(side*3.4f,1.5f,z,.65f,.18f,.6f,0x8b7142);
   float flicker=sinf(time*14+z)*.12f;limb({side*3.4f,1.65f,z},{side*3.4f+.08f,2.3f+flicker,z},.17f,theme==2?0x61cfe2:0xf3a748,5);
   if(z<20)limb({side*3.4f,1.65f,z-.05f},{side*3.4f-.04f,2.08f+flicker,z-.05f},.09f,0xffedaa,5);
  }
 }
 // Monumental gate. Repeated every 96 m, with stepped lintel and jade keystone.
 float gatez=71-fmodf(travel,96.f);
 if(gatez> -4&&visible_box({0,3.4f,gatez},{4.55f,3.95f,1.f})){
  for(int side: {-1,1}){box(side*3.65f,-.5f,gatez,1.25f,4.3f,1.4f,0x75877b);box(side*3.65f,-.1f,gatez,1.6f,.4f,1.7f,0xb3aa7f);}
  for(int i=0;i<6;i++){
   V3 oa={arch_ring[i][0]*4.15f,3.2f+arch_ring[i][1]*4.15f,gatez-.75f},ob={arch_ring[i+1][0]*4.15f,3.2f+arch_ring[i+1][1]*4.15f,gatez-.75f};
   V3 ia={arch_ring[i][0]*3.35f,3.2f+arch_ring[i][1]*3.35f,gatez-.75f},ib={arch_ring[i+1][0]*3.35f,3.2f+arch_ring[i+1][1]*3.35f,gatez-.75f};
   face(oa,ob,ib,ia,(i&1)?0xb2ae89:0x96a18a,.92f,theme==1?4:theme==2?5:0);face(ia,ib,add(ib,{0,0,1.5f}),add(ia,{0,0,1.5f}),0x6e8379,.75f);
  }
  face({-.42f,6.35f,gatez-.78f},{.42f,6.35f,gatez-.78f},{.42f,7.19f,gatez-.78f},{-.42f,7.19f,gatez-.78f},0x70a98e,1,3);
 }
 // Deterministic gold trails, culled at the same collection cursor used by the rules.
 int first=std::max(4,int(s.distance/3)-1);
 float coin_wave_s=sinf(time*4+first*.5f),coin_wave_c=cosf(time*4+first*.5f);
 float coin_spin_s=sinf(time*2.8f+first*.6f),coin_spin_c=cosf(time*2.8f+first*.6f);
 for(int i=first;i<first+(quality==0?19:13);i++){float z=i*3.f-s.distance,osc=coin_wave_s,spin=coin_spin_c;
  float ns=coin_wave_s*.87758256f+coin_wave_c*.47942554f;coin_wave_c=coin_wave_c*.87758256f-coin_wave_s*.47942554f;coin_wave_s=ns;
  ns=coin_spin_s*.82533561f+coin_spin_c*.56464247f;coin_spin_c=coin_spin_c*.82533561f-coin_spin_s*.56464247f;coin_spin_s=ns;
  if(z<.8f||coin_taken(s,i))continue;float x=coin_lane(i)*1.65f;float y=coin_height(i)+osc*.08f;
  float w=.19f*(.25f+.75f*fabsf(spin));
  face({x-w*.5f,y-.22f,z},{x+w*.5f,y-.22f,z},{x+w,y,z},{x-w,y,z},0xf8cc58);
  face({x-w,y,z},{x+w,y,z},{x+w*.5f,y+.22f,z},{x-w*.5f,y+.22f,z},0xffd96e);
  face({x-.07f,y-.12f,z-.17f},{x+.07f,y-.12f,z-.17f},{x+.07f,y+.12f,z-.17f},{x-.07f,y+.12f,z-.17f},0xffec9a);
 }
 mesh_layer=2;
 for(int i=0;i<s.count;i++){const auto&o=s.objects[i];float z=o.distance-s.distance;if(z< -3||z>85||o.passed)continue;float x=o.lane*1.65f;
  if(!visible_box({o.lane==2?0:x,1.f,z},{o.lane==2?3.1f:.85f,2.3f,2.5f})){culled_objects++;continue;}
  if(o.kind==Kind::Rock){box(x,0,z,1.18f,.95f,1.12f,0x7d7660);box(x-.12f,.95f,z,.8f,.25f,.8f,0x99937a);face({x-.55f,.1f,z-.57f},{x+.55f,.1f,z-.57f},{x+.55f,.88f,z-.57f},{x-.55f,.88f,z-.57f},0x787b65,.8f,0);}
  if(o.kind==Kind::Root){limb({-2.7f,.4f,z},{2.7f,.4f,z},.38f,0x785133,8);face({-2.6f,.18f,z-.36f},{2.6f,.18f,z-.36f},{2.6f,.61f,z-.36f},{-2.6f,.61f,z-.36f},0x92613b,1,2);for(int k=-2;k<=2;k++)box(k,.55f,z-.12f,.12f,.12f,.4f,0xb79057);}
  if(o.kind==Kind::Gate){for(int side=-1;side<=1;side+=2)box(side*2.5f,0,z,.4f,2.9f,.5f,0x68694f);box(0,1.08f,z,5.4f,.74f,.6f,0x99794d);box(0,1.83f,z,5.7f,.17f,.8f,0xcfb377);face({-2.4f,1.1f,z-.31f},{2.4f,1.1f,z-.31f},{2.4f,1.79f,z-.31f},{-2.4f,1.79f,z-.31f},0xc5b185,1.f,3);}
  if(o.kind==Kind::Gap){box(0,-.95f,z-2.1f,5.8f,.24f,.4f,0x4d5642);box(0,-.95f,z+2.1f,5.8f,.24f,.4f,0x4d5642);}
  if(o.kind==Kind::Magnet||o.kind==Kind::Shield){float y=1.2f+sinf(time*4)*.16f;limb({x,y-.32f,z},{x,y+.32f,z},.35f,o.kind==Kind::Shield?0x78d9c6:0x7cbbe7,6);box(x,y-.1f,z-.35f,.12f,.28f,.04f,0xffe6a3);}
 }
 mesh_layer=0;
 if((s.combo_t>.2f||s.feedback_t>0||s.rush>0)&&s.mode==Mode::Running){
  for(int i=0;i<(quality?5:9);i++){float t=fmodf(time*1.7f+i*.19f,1.f),angle=i*2.4f;
   float x=s.lane_x+cosf(angle)*(.25f+t*.65f),y=s.jump+.35f+t*1.1f,z=.6f-t*1.2f;float r=.025f+(1-t)*.035f;
   face({x-r,y,z},{x,y+r,z},{x+r,y,z},{x,y-r,z},s.rush>0?0xffd98d:0xe6d18c,1.2f);
  }
 }
 if(quality==0)for(int i=0;i<6;i++){float z=8+i*5-fmodf(travel,5.f),x=(i%2?1:-1)*(3.8f+(i%3)*.4f),y=1.1f+.3f*sinf(time*1.5f+i);
  face({x-.025f,y,z},{x,y+.05f,z},{x+.025f,y,z},{x,y-.05f,z},theme==2?0x80dce0:0xd6d990,1.2f);
 }
 peak_faces=std::max(peak_faces,count);
 // Sort compact indices, not the full vertex/UV payload. Depth-tested
 // actor/hazard faces need no painter sorting at all.
 int ordered=0,world_end=0,shadow_end=0;
 for(int i=0;i<count;i++)if(faces[i].layer==0)draw_order[ordered++]=uint16_t(i);
 world_end=ordered;
 for(int i=0;i<count;i++)if(faces[i].layer==3)draw_order[ordered++]=uint16_t(i);
 shadow_end=ordered;
 for(int i=0;i<count;i++)if(faces[i].layer!=0&&faces[i].layer!=3)draw_order[ordered++]=uint16_t(i);
 auto farther=[&](uint16_t a,uint16_t b){return faces[a].depth>faces[b].depth;};
 std::sort(draw_order,draw_order+world_end,farther);
 if(!depth)std::sort(draw_order+shadow_end,draw_order+count,farther);
 // Paving is drawn before the character. Average-Z painter order can place
 // a foreground ground tile over the feet even though the feet are above it.
 // The small character/obstacle pass shares real per-pixel camera depth.
 for(int i=0;i<count;i++){
  const auto &face=faces[draw_order[i]];int pass=i<world_end?0:i<shadow_end?1:2;
  if(depth&&pass==2){
   std::array<pxa::game::Vertex,4> v{};
   for(int j=0;j<4;j++){auto&q=v[j];q.x_q4=face.xy[j*2];q.y_q4=face.xy[j*2+1];q.depth_q8=face.depths[j];
    q.u_q4=face.material==-2?int16_t((face.palette-208)*16):face.uv[j*2];q.v_q4=face.material==-2?0:face.uv[j*2+1];q.light=face.material==-2?face.shades[j]:face.light;}
   if(face.material>=0||face.material==-2){pxa::game::PolygonOptions opts;opts.affine_uv=true;opts.lit_palette=true;
    f.textured_depth_polygon({uint8_t(face.material==-2?4:2)},v,opts);
   }else f.solid_depth_polygon(v,{face.color});
   continue;
  }
  if(face.material==-2){
   float light=.36f+.105f*(face.shades[0]+face.shades[1]+face.shades[2]+face.shades[3])*.25f;
   f.quad(face.xy,{rgb(lit(actor_colors[face.palette-208],light,0))});continue;
  }
  if(face.material<0){f.quad(face.xy,{face.color});continue;}
  std::array<pxa::game::Vertex,4> v{};for(int j=0;j<4;j++){v[j].x_q4=face.xy[j*2];v[j].y_q4=face.xy[j*2+1];v[j].u_q4=face.uv[j*2];v[j].v_q4=face.uv[j*2+1];v[j].light=face.light;v[j].depth_q8=0;}
  pxa::game::PolygonOptions opts;opts.painter=true;opts.affine_uv=true;f.textured_quad({2},v,opts);
 }
 }
 drawing_ui=overlay&&display;
 // Native Canvas text is rasterized by the Host at physical screen resolution;
 // the atlas remains a fallback for Hosts without sized Canvas text.
 float ui=ui_scale(),cx=width*.5f;
 auto label=[&](const char*t,float y,float size,uint32_t c){text(f,t,cx,y,size*ui,c,true);};
 char num[80];
 if(s.mode==Mode::Title){
  if(title_background&&drawing_ui){auto box=pxa::ui::surface_to_canvas_region({0,0,int(display->width),int(display->height)},*display);overlay->image(box,*title_background,pxa::ui::ImageFit::cover);}
  if(title_logo&&drawing_ui){
   float rx=float(display->width)/width,ry=float(display->height)/height;
   auto region=pxa::ui::surface_to_canvas_region({int((cx-135*ui)*rx),int(height*.10f*ry),int(270*ui*rx),int((height>width?90:70)*ui*ry)},*display);
   overlay->image(region,*title_logo,pxa::ui::ImageFit::contain);
  }else{label("遗迹疾奔",height*.12f,2.f,GOLD);label("R E L I C   R U N",height*.12f+36*ui,.7f,IVORY);}
  float by=height-73*ui;
  panel(f,cx-88*ui,by-42*ui,176*ui,33*ui,INK,0x6a805f);
  text(f,"最佳纪录",cx-74*ui,by-33*ui,.72f*ui,JADE);snprintf(num,sizeof(num),"%06u",save.best);text(f,num,cx+10*ui,by-34*ui,1.f*ui,GOLD);
  panel(f,cx-104*ui,by,208*ui,32*ui,0xdab66e,0x846233);
  rect(f,cx-99*ui,by+4*ui,198*ui,2*ui,0xf8d99a);label("开始探险",by+4*ui,1.05f,0x233b31);
  float oy=height-35*ui;panel(f,cx-134*ui,oy,128*ui,25*ui,INK,0x638875);panel(f,cx+6*ui,oy,128*ui,25*ui,INK,0x638875);
  text(f,"操作指南",cx-70*ui,oy+6*ui,.82f*ui,IVORY,true);text(f,quality?"画质 流畅":"画质 精美",cx+70*ui,oy+6*ui,.82f*ui,JADE,true);
 }else if(s.mode==Mode::Running){
  panel(f,std::max(9*ui,safe_x),8*ui+safe_y,100*ui,36*ui,INK,0x6e8162);snprintf(num,sizeof(num),"%06u",s.score);text(f,num,std::max(17*ui,safe_x+8*ui),10*ui+safe_y,1.15f*ui,s.combo_t>0?GOLD:IVORY);
  snprintf(num,sizeof(num),"%um  金币 %u",unsigned(s.distance),s.coins);text(f,num,std::max(17*ui,safe_x+8*ui),29*ui+safe_y,.8f*ui,GOLD);
  panel(f,width-38*ui-safe_x,8*ui+safe_y,29*ui,29*ui,INK,0x6e8162);rect(f,width-29*ui-safe_x,16*ui+safe_y,3*ui,12*ui,GOLD);rect(f,width-22*ui-safe_x,16*ui+safe_y,3*ui,12*ui,GOLD);
  float hx=std::max(9*ui,safe_x),hy=8*ui+safe_y;
  rect(f,hx,hy+36*ui,100*ui,3*ui,0x304c40);rect(f,hx,hy+36*ui,100*ui*(s.rush>0?s.rush/7:s.energy/60),3*ui,s.rush>0?0xffd383:JADE);
  if(multiplier(s)>1){snprintf(num,sizeof(num),"x%d",multiplier(s));text(f,num,hx+69*ui,hy+5*ui,.83f*ui,GOLD);}
  // Two compact rows beside the score replace banners over the road.
  const float status_x=(hx+100*ui+width-48*ui-safe_x)*.5f;
  const char* region=theme==0?"丛林石桥":theme==1?"落日神庙":"幽蓝古墓";
  uint32_t region_color=IVORY;
  if(s.rush>0){snprintf(num,sizeof(num),"觉醒 x4 %ds",int(s.rush)+1);region=num;region_color=GOLD;}
  else if(s.shield>0||s.magnet>0){snprintf(num,sizeof(num),"%s %ds",s.shield>0?"护盾":"磁铁",int(s.shield>0?s.shield:s.magnet));region=num;region_color=JADE;}
  text(f,region,status_x,10*ui+safe_y,.8f*ui,region_color,true);
  if(s.next_turn-s.distance<30){
   const bool armed=s.queued_turn==s.turn_direction;
   const char* turn=armed?(s.turn_direction==1?"右转 已准备":"左转 已准备"):(s.turn_direction==1?"向右转 >>>":"<<< 向左转");
   text(f,turn,status_x,30*ui+safe_y,.78f*ui,armed?JADE:GOLD,true);
  }else if(s.feedback_t>0&&s.feedback!=Feedback::Rush){
   const char* msg=s.feedback==Feedback::Jump?"漂亮跳跃":s.feedback==Feedback::Slide?"完美滑铲":s.feedback==Feedback::Dodge?"惊险闪避":s.feedback==Feedback::Turn?"漂亮转弯":s.feedback==Feedback::Quest?"挑战达成 +500":s.feedback==Feedback::Rush?"遗迹觉醒 x4":s.feedback==Feedback::Rescue?"救援护盾":s.feedback==Feedback::Shield?"护盾破碎":"";
   text(f,msg,status_x,30*ui+safe_y,.75f*ui,s.feedback==Feedback::Shield?0xe39c79:GOLD,true);
  }
  if(s.distance<28&&save.runs<=1)label("左右变道   上跳跃   下滑铲",52*ui+safe_y,.78f,IVORY);
  else{snprintf(num,sizeof(num),"%s  %u/%u",s.quests%3==0?"收集金币":s.quests%3==1?"躲避障碍":"通过转弯",quest_progress(s),quest_goal(s));label(num,52*ui+safe_y,.78f,IVORY);}

 }else {
  float pw=std::min(width-24.f,254*ui),ph=(s.mode==Mode::Help?204.f:172.f)*ui;float x=cx-pw/2,y=(height-ph)/2;
  panel(f,x-2*ui,y-2*ui,pw+4*ui,ph+4*ui,INK,0xa58a54);
  if(s.mode==Mode::Help){label("操作指南",y+16*ui,1.3f,GOLD);label("左右滑动 / 方向键：变道",y+46*ui,.83f,IVORY);label("向上滑动 / A：跳跃",y+68*ui,.83f,IVORY);label("向下滑动 / B：滑铲",y+90*ui,.83f,IVORY);label("弯口：按提示向左或右",y+112*ui,.83f,IVORY);label("START 暂停 / 失败 B 救援",y+134*ui,.8f,IVORY);label("金币连击：觉醒吸金护体",y+156*ui,.78f,IVORY);label("轻触返回",y+182*ui,.88f,JADE);
  }else if(s.mode==Mode::Paused){panel(f,cx-95*ui,y+53*ui,190*ui,27*ui,0x224c41,0x678b70);panel(f,cx-95*ui,y+90*ui,190*ui,26*ui,0x143c34,0x416d5a);panel(f,cx-95*ui,y+128*ui,190*ui,26*ui,0x143c34,0x416d5a);label("暂停旅程",y+20*ui,1.4f,GOLD);label("继续探险",y+59*ui,1.05f,IVORY);label(save.sound?"音效 开":"音效 关",y+99*ui,.9f,JADE);label("返回主菜单",y+137*ui,.87f,IVORY);
  }else{label(s.record?"新纪录":"旅程结束",y+16*ui,1.35f,GOLD);snprintf(num,sizeof(num),"%06u",s.score);label(num,y+44*ui,1.65f,IVORY);snprintf(num,sizeof(num),"距离 %um    金币 %u",unsigned(s.distance),s.coins);label(num,y+78*ui,.82f,GOLD);
   label(!strcmp(s.reason,"gap")?"坠入深渊":!strcmp(s.reason,"turn")?"错过弯口":"碰到障碍",y+99*ui,.8f,0xccac8a);
   snprintf(num,sizeof(num),"连击 %u  挑战 %u",s.best_combo,s.quests);label(num,y+116*ui,.65f,JADE);
   panel(f,cx-110*ui,y+134*ui,104*ui,26*ui,0x285346,0x648d73);panel(f,cx+6*ui,y+134*ui,104*ui,26*ui,0xe7bf70,0xb28a48);
   text(f,s.rescues?"返回":"救援继续",cx-58*ui,y+139*ui,.85f*ui,IVORY,true);text(f,"重来",cx+58*ui,y+139*ui,.85f*ui,INK,true);
  }
 }
}
}
