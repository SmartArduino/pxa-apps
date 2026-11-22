#include "landscape.hpp"

namespace sky {
namespace {

constexpr int sprite_width=15,sprite_height=17;
using Sprite=std::array<std::uint8_t,sprite_width*sprite_height>;

struct Lobe { int x,y,rx,ry; };
constexpr int magnitude(int v) { return v<0 ? -v : v; }

consteval Sprite foliage(int variant,bool overhead) {
    const int lean=(variant%3)-1;
    const Lobe levels[]{
        {5+lean,overhead ? 7 : 6,4,4},
        {7+lean,overhead ? 6 : 4,4,4},
        {10+lean,overhead ? 8 : 7,4,4},
        {7,overhead ? 10 : 9,5,3},
    };
    Sprite sprite{};
    for (int y=0;y<sprite_height;++y) for (int x=0;x<sprite_width;++x) {
        int lobe=0,shade=0;
        const unsigned grain=hash(x+variant*19,y+variant*11);
        for (const auto p:levels) {
            const int dx=x-p.x,dy=y-p.y;
            const int detail=29-(dx*dx*30)/(p.rx*p.rx)-(dy*dy*30)/(p.ry*p.ry)+int(grain&7)-3;
            if (detail<=lobe) continue;
            lobe=detail;
            shade=std::clamp(2+(dy<0)+(dx<0)-(detail<9),1,4);
        }
        if (!shade && !overhead && y>9 && magnitude(x-7-lean*(16-y)/5)<=int(y<15))
            shade=5+(x>=7);
        sprite[y*sprite_width+x]=std::uint8_t(shade);
    }
    return sprite;
}

consteval Sprite needles(int variant,bool overhead) {
    if (overhead) return foliage(variant+3,true);
    const int lean=variant-1;
    Sprite sprite{};
    for (int y=0;y<sprite_height;++y) for (int x=0;x<sprite_width;++x) {
        const unsigned grain=hash(x+variant*31,y+14);
        const int axis=7+lean*(16-y)/12;
        const int dx=x-axis;
        int shade=0;
        for (int layer=2;layer>=0;--layer) {
            const int down=y-3*layer;
            const int spread=(down*(4+layer)+4)/7;
            const int side=dx+(layer&1);
            if (down<0 || down>6 || magnitude(side)>spread || (down==6 && (grain&3)==0)) continue;
            shade=std::clamp(2+(side<0)+(down<4)-(down==6),1,4);
        }
        if (!shade && y>=12 && magnitude(x-7)<=int(y>=15)) shade=5+(x>7);
        sprite[y*sprite_width+x]=std::uint8_t(shade);
    }
    return sprite;
}

constexpr std::array leaves{foliage(0,false),foliage(1,false),foliage(2,false)};
constexpr std::array needles_side{needles(0,false),needles(1,false),needles(2,false)};
constexpr std::array crowns{foliage(0,true),foliage(1,true),foliage(2,true),
                            needles(0,true),needles(1,true),needles(2,true)};

constexpr int small_width=5,small_height=6;
using SmallSprite=std::array<std::uint8_t,small_width*small_height>;

template<std::size_t N>
consteval auto prefilter(const std::array<Sprite,N>& sprites) {
    std::array<SmallSprite,N> result{};
    for (unsigned i=0;i<N;++i) {
        for (int y=0;y<small_height;++y) for (int x=0;x<small_width;++x) {
            const int top=y*sprite_height/small_height,bottom=(y+1)*sprite_height/small_height;
            const int left=x*sprite_width/small_width,right=(x+1)*sprite_width/small_width;
            int covered=0,sum=0;
            for (int row=top;row<bottom;++row) for (int col=left;col<right;++col) {
                const int shade=sprites[i][row*sprite_width+col];
                covered+=shade!=0;
                sum+=shade;
            }
            if (covered) {
                const int alpha=31*covered/((right-left)*(bottom-top));
                result[i][y*small_width+x]=std::uint8_t((alpha<<3)|((sum+covered/2)/covered));
            }
        }
    }
    return result;
}

constexpr auto small_leaves=prefilter(leaves);
constexpr auto small_needles=prefilter(needles_side);
constexpr auto small_crowns=prefilter(crowns);

void crown_shadow(Scene& scene,Vec ground,float height,float width,int visibility) {
    const auto local=scene.camera.local(ground+Vec{height*shadow_x,0,height*shadow_z});
    if (local.z<Scene::near) return;
    const auto screen=scene.camera.project(local);
    auto& d=scene.out;
    const float rx=std::min(10.f,width*.53f),ry=std::max(.7f,rx*.57f);
    const float inv_x=1.f/rx,inv_y=1.f/ry;
    const float strength=saturate((width-1.3f)*.52f)*visibility*.34f;
    if (strength<1) return;
    const int low=std::max(0,int(std::floor(screen.y-ry)));
    const int high=std::min(d.height-1,int(screen.y+ry));
    for (int row=low;row<=high;++row) {
        const float edge=1-std::abs(row+.5f-screen.y)*inv_y;
        const float span=std::max(0.f,edge*rx);
        for (int col=std::max(0,int(std::floor(screen.x-span)));col<=std::min(d.width-1,int(screen.x+span));++col) {
            const int alpha=int(strength*saturate((edge-std::abs(col+.5f-screen.x)*inv_x)*1.8f));
            if (alpha) d.shade_ground(col,row,local.z,rgb(0x314b48),alpha);
        }
    }
}

void tree(Scene& scene,Vec ground,unsigned seed,const GroundLight& light,float fade) {
    const auto& cam=scene.camera;
    const Vec local=cam.local(ground);
    if (local.z<Scene::near+6) return;
    const int visibility=int(32.f*fade*saturate((local.z-10.f)/15.f));
    if (visibility<2) return;
    const bool pine=(seed&7)<3;
    const bool overhead=cam.overhead;
    const float height=(pine ? 13.f : 9.f)+((seed>>8)&7);
    const float scale=cam.scale_at(local.z);
    const float width=std::clamp(height*(pine ? .55f : .91f)*scale+.42f,1.18f,25.f);
    const auto base=cam.project(local);
    const auto top=cam.project(cam.local(ground+Vec{0,height,0}));
    const float left=top.x-width*.5f,right=left+width;
    const float tall=overhead ? width : std::clamp(base.y-top.y,1.18f,27.f);
    const float y0=overhead ? top.y-tall*.5f : base.y-tall;
    const float y1=y0+tall;
    auto& d=scene.out;
    if (left>=d.width || right<=0 || y0>=d.height || y1<=0) return;
    crown_shadow(scene,ground,height,width,visibility);
    const float depth=local.z-(overhead ? height*.85f : height*.20f);
    const int shade=light(ground.x,ground.y+height,ground.z);
    std::array<Color,7> colors{0,rgb(0x2d5038),rgb(0x4e703d),rgb(0x7e8d49),rgb(0xb0ad63),
                               rgb(0x88694f),rgb(0x544a35)};
    if (pine) {
        colors[1]=rgb(0x23473f);
        colors[2]=rgb(0x345d4d);
        colors[3]=rgb(0x527e60);
        colors[4]=rgb(0x8b9b68);
    } else if ((seed&96)==64) {
        colors[3]=rgb(0x929648);
        colors[4]=rgb(0xbebb68);
    }
    if (shade) for (unsigned c=1;c<colors.size();++c) colors[c]=blend(colors[c],rgb(0x274254),shade);
    const auto variation=(seed>>7)%3;
    const auto crown=variation+(pine ? 3 : 0);
    const bool small=std::min(width,tall)<7.f;
    const auto* sprite=small
        ? (overhead ? small_crowns[crown].data() : pine ? small_needles[variation].data() : small_leaves[variation].data())
        : (overhead ? crowns[crown].data() : pine ? needles_side[variation].data() : leaves[variation].data());
    const int cols=small ? small_width : sprite_width,rows=small ? small_height : sprite_height;
    const float step_x=256.f*cols/width,step_y=256.f*rows/tall;
    const int begin=std::max(0,int(std::floor(left))),end=std::min(d.width,int(std::ceil(right)));
    const int top_row=std::max(0,int(std::floor(y0))),bottom_row=std::min(d.height,int(std::ceil(y1)));
    const auto coverage=[](float from,float to,int p) {
        return std::max(0.f,std::min(float(p+1),to)-std::max(float(p),from));
    };
    const int edge_left=int(32.f*coverage(left,right,begin)+.5f);
    const int edge_right=int(32.f*coverage(left,right,end-1)+.5f);
    const int u0=int((begin+.5f-left)*step_x),du=int(step_x+.5f);
    for (int y=top_row;y<bottom_row;++y) {
        const int row=std::clamp(int((y+.5f-y0)*step_y)>>8,0,rows-1);
        const auto* texels=sprite+cols*row;
        const int alpha=int(visibility*coverage(y0,y1,y)+.5f);
        int u=u0;
        for (int x=begin;x<end;++x,u+=du) {
            const unsigned texel=texels[std::clamp(u>>8,0,cols-1)];
            if (!texel) continue;
            int opacity=alpha;
            if (x==begin) opacity=opacity*edge_left/32;
            else if (x==end-1) opacity=opacity*edge_right/32;
            if (small) opacity=opacity*((texel>>3)+1)/32;
            if (opacity) d.zpixel(x,y,depth,colors[texel&7],opacity);
        }
    }
}

void shore_rock(Scene& scene,Vec ground,unsigned seed,Color terrain,const GroundLight& light,bool shelf) {
    const auto& cam=scene.camera;
    const Vec local=cam.local(ground);
    if (local.z<18.f || (!cam.overhead && local.z>350.f)) return;
    const float radius=(shelf ? 8.4f : 3.9f)+float((seed>>12)&3);
    const float scale=cam.scale_at(local.z);
    const float visible=(radius+1)*scale;
    if (visible<.85f) return;
    const auto p=cam.project(local);
    const float margin=visible*1.7f+2;
    if (p.x<-margin || p.y<-margin || p.x>scene.out.width+margin || p.y>scene.out.height+margin) return;

    const float h=radius*((shelf ? .49f : .36f)+.07f*float((seed>>19)&3));
    const float aspect=.65f+float((seed>>24)&7)*.044f;
    const Vec west=ground+Vec{-radius,.06f,radius*.21f};
    const Vec north=ground+Vec{radius*.10f,.06f,-radius*aspect};
    const Vec east=ground+Vec{radius*.92f,.38f,-radius*.1f};
    const Vec south=ground+Vec{-radius*.08f,.04f,radius*aspect};
    const Vec crown=ground+Vec{-radius*.27f,h,-radius*.15f};
    const Vec ledge=ground+Vec{radius*.44f,h*.62f,radius*.18f};
    const int shade=light(ground.x,ground.y+h,ground.z);
    const int coverage=int(saturate((visible-.85f)*.74f)*32);
    const auto stone=[&](Color color) {
        return blend(terrain,blend(color,rgb(0x2c4853),shade),coverage);
    };
    const Vec offset{h*shadow_x,.05f,h*shadow_z};
    if (coverage>=6)
        scene.quad(west+offset,north+offset,east+offset,south+offset,rgb(0x374c43),coverage/3);
    scene.triangle(west,north,crown,stone(rgb(0xd9c199)));
    scene.triangle(north,east,crown,stone(rgb(0xbbab8e)));
    scene.triangle(east,south,ledge,stone(rgb(0x77786d)));
    scene.triangle(east,ledge,crown,stone(rgb(0x9c9382)));
    scene.triangle(ledge,south,crown,stone(rgb(0xaa9b81)));
    scene.triangle(south,west,crown,stone(rgb(0x918773)));
    if (shelf && visible>2.6f) {
        const auto layer=[&](Vec p,float ratio) {
            return p+(crown-p)*ratio+Vec{0,.12f,.10f};
        };
        scene.quad(layer(south,.43f),layer(west,.43f),layer(west,.53f),layer(south,.53f),stone(rgb(0xc1ad8e)));
        scene.quad(layer(west,.43f),layer(north,.43f),layer(north,.53f),layer(west,.53f),stone(rgb(0xf1d7aa)));
    }
}

void reeds(Scene& scene,Vec ground,unsigned seed,const GroundLight& light) {
    const auto local=scene.camera.local(ground);
    if (local.z<Scene::near || (!scene.camera.overhead && local.z>280.f)) return;
    const float height=3.2f+float((seed>>9)&7)*.4f;
    if (scene.camera.scale_at(local.z)*height<.9f) return;
    const int shade=light(ground.x,ground.y+height,ground.z);
    const auto stem=blend(rgb(0x506f49),rgb(0x253f4a),shade);
    const auto tips=blend(rgb(0xb9bd70),rgb(0x344e48),shade);
    const Vec side{(seed&4) ? .8f : .6f,0,(seed&8) ? -.75f : .75f};
    for (int i=-1;i<=1;++i) {
        const Vec base=ground+side*(i*1.5f);
        const Vec tip=base+side*(i*1.8f)+Vec{0,height-std::abs(i)*.9f,i*1.52f};
        scene.stroke(base,tip,stem);
        scene.stroke(tip+(base-tip)*.28f,tip,tips);
    }
}

} // namespace

void draw_woodland(Scene& scene,const World& world,const GroundLight& light) {
    const auto& cam=scene.camera;
    const float ground_start=std::max(55.f,cam.pos.y-World::highest_land);
    const Vec center=cam.overhead ? cam.pos : cam.pos+cam.axes.front*(ground_start+118.f);
    // A tree remains at the same location across camera and render-scale changes.
    constexpr int cadence=2,reach=12*cadence;
    constexpr float radius=347.f,fade_band=105.f;
    const int center_x=World::tile(center.x)&~1,center_z=World::tile(center.z)&~1;
    for (int z=center_z-reach;z<=center_z+reach;z+=cadence) {
        for (int x=center_x-reach;x<=center_x+reach;x+=cadence) {
            const int at=World::index(x,z);
            const int height=world.heights[at];
            const bool forest=world.forest(at);
            const int waterline=world.terrain_features[at]>>2;
            const bool wet_bank=waterline && height>=waterline && height<=waterline+5;
            if (!forest && !wet_bank && (height<3 || height>13)) continue;
            const unsigned seed=hash(x&255,z&255);
            const int shoreline_slope=forest || wet_bank ? 0 :
                std::abs(int(world.heights[World::index(x+1,z)])-world.heights[World::index(x-1,z)])+
                std::abs(int(world.heights[World::index(x,z+1)])-world.heights[World::index(x,z-1)]);
            const bool shelf=shoreline_slope>=5 && (seed&8);
            if (!forest && (seed&15)>=(wet_bank ? 11u : shelf ? 14u : 3u)) continue;
            const float px=float(x*World::cell+3+int((seed>>17)%11));
            const float pz=float(z*World::cell+3+int((seed>>26)%11));
            const float dx=px-center.x,dz=pz-center.z;
            const float dist=dx*dx+dz*dz;
            if (dist>=radius*radius) continue;
            const auto surface=world.surface_at(px,pz);
            if (surface.water()) continue;
            const auto altitude=surface.height();
            if (forest && altitude>=4) {
                const float fade=saturate((radius*radius-dist)/(fade_band*(2*radius-fade_band)));
                tree(scene,{px,altitude,pz},seed,light,fade);
            } else if (wet_bank) {
                reeds(scene,{px,altitude,pz},seed,light);
            } else if (!forest && altitude>1.5f && dist<285.f*285.f) {
                shore_rock(scene,{px,altitude,pz},seed,world.colors[at],light,shelf);
            }
        }
    }
}

} // namespace sky
