#include "clouds.hpp"

namespace sky {
namespace {

Vec wind(float time) { return {time*4,0,time*1.5f}; }

struct Puff {
    Vec center,inverse_radius;
    float cosine,sine,strength;

    float envelope(Vec point) const {
        const auto d=point-center;
        const Vec local{d.x*cosine+d.z*sine,d.y,d.z*cosine-d.x*sine};
        return strength-norm2({local.x*inverse_radius.x,local.y*inverse_radius.y,
                               local.z*inverse_radius.z});
    }
};
struct Random {
    std::uint32_t state;
    float between(float lo,float hi) {
        state=state*1664525u+1013904223u;
        return mix(lo,hi,float(state>>8)*(1.f/16777216.f));
    }
};

template<int spacing>
class Noise {
    static constexpr int w=CloudField::width/spacing+2;
    static constexpr int h=CloudField::height/spacing+2;
    static constexpr int l=CloudField::length/spacing+2;
    std::array<float,w*h*l> values;

    static float weight(int v) {
        const float t=float(v%spacing)/spacing;
        return t*t*(3-2*t);
    }
public:
    explicit Noise(Random& random) {
        for (auto& value:values) value=random.between(-1,1);
    }
    float sample(int x,int y,int z) const {
        const int i=((z/spacing)*h+y/spacing)*w+x/spacing;
        const float fx=weight(x),fy=weight(y),fz=weight(z);
        const auto row=[&](int d) { return mix(values[i+d],values[i+d+1],fx); };
        return mix(mix(row(0),row(w),fy),mix(row(w*h),row(w*h+w),fy),fz);
    }
};

struct RayAxis { float direction,inverse; int step; };
RayAxis ray_axis(float direction) {
    const float divisor=std::copysign(std::max(std::abs(direction),1e-6f),direction);
    return {direction,1/divisor,divisor>0 ? 1 : -1};
}
struct Extent { float entry,exit; };
Extent intersect_axis(float origin,const RayAxis& ray,float lower,float upper) {
    const float a=(lower-origin)*ray.inverse,b=(upper-origin)*ray.inverse;
    return {std::min(a,b),std::max(a,b)};
}

Rect projected_box(const Scene& scene,const CloudField::Bank& bank,Vec center) {
    const auto& cam=scene.camera;
    const float unit=CloudField::cell*bank.scale, height=CloudField::layer*bank.scale;
    const auto mid=(bank.lower+bank.upper)*.5f-CloudField::pivot;
    const auto size=(bank.upper-bank.lower)*.5f;
    center=cam.local(center+Vec{mid.x*unit,mid.y*height,mid.z*unit});
    const float half_x=size.x*unit,half_y=size.y*height,half_z=size.z*unit;
    const float view_x=std::abs(cam.axes.right.x)*half_x+std::abs(cam.axes.right.z)*half_z;
    const float view_z=std::abs(cam.axes.front.x)*half_x+std::abs(cam.axes.front.z)*half_z;
    if (!cam.overhead && center.z+view_z<=Scene::near) return {};
    const float first=std::max(Scene::near,center.z-view_z);
    const float last=center.z+view_z;
    const auto interval=[&](float lo,float hi,float axis) {
        return std::array{
            axis+cam.focal*std::min(lo/first,lo/last),
            axis+cam.focal*std::max(hi/first,hi/last)
        };
    };
    const float vertical=cam.overhead
        ? std::abs(cam.axes.up.x)*half_x+std::abs(cam.axes.up.z)*half_z : half_y;
    const auto xx=cam.overhead
        ? std::array{cam.cx+(center.x-view_x)*cam.focal,cam.cx+(center.x+view_x)*cam.focal}
        : interval(center.x-view_x,center.x+view_x,cam.cx);
    const auto yy=cam.overhead
        ? std::array{cam.horizon+(-center.y-vertical)*cam.focal,cam.horizon+(-center.y+vertical)*cam.focal}
        : interval(-center.y-vertical,-center.y+vertical,cam.horizon);
    const auto rounded=[&](float value,int limit,bool end) {
        // Align bound and ray sampling to the same two-pixel grid on every bank.
        const float p=value*.5f;
        return std::clamp(2*int(end ? std::ceil(p) : std::floor(p)),0,limit);
    };
    const int left=rounded(xx[0],scene.out.width,false),right=rounded(xx[1],scene.out.width,true);
    const int top=rounded(yy[0],scene.out.height,false),bottom=rounded(yy[1],scene.out.height,true);
    return {left,top,right-left,bottom-top};
}

void draw_bank(Scene& scene,const CloudField::Bank& bank,Vec center,Rect box) {
    auto& out=scene.out;
    const auto& cam=scene.camera;
    const float voxel=CloudField::cell*bank.scale;
    const float inv_voxel=1.f/voxel;
    const float near=Scene::near*inv_voxel;
    constexpr float aspect=CloudField::cell/CloudField::layer;
    Vec origin=(cam.pos-center)*inv_voxel;
    origin.y*=aspect;
    origin+=CloudField::pivot;
    const int haze=int(saturate((cam.local(center).z-830.f)/2870.f)*22);
    constexpr Color shade=rgb(0x8daec6), sun=rgb(0xfff7e6);
    std::array<Color,9> palette;
    for (int i=0;i<int(palette.size());++i)
        palette[i]=blend(blend(shade,sun,i*4),scene.fog,haze);

    struct Column { RayAxis rx,rz; Extent x,z; };
    std::array<Column,Canvas::max_width/2> columns;
    const int right=box.x+box.w,bottom=box.y+box.h;
    const int area=box.w*box.h,total=out.width*out.height;
    const int spacing=area>total/2 ? (out.width<=150 ? 5 : 4) : area>total/5 ? 3 : 2;
    for (int x=box.x;x<right;x+=spacing) {
        const auto direction=cam.axes.front+cam.axes.right*((x+std::min(spacing,right-x)*.5f-cam.cx)/cam.focal);
        auto& col=columns[x/2];
        col.rx=ray_axis(direction.x);
        col.rz=ray_axis(direction.z);
        col.x=intersect_axis(origin.x,col.rx,bank.lower.x,bank.upper.x);
        col.z=intersect_axis(origin.z,col.rz,bank.lower.z,bank.upper.z);
    }
    for (int y=box.y;y<bottom;y+=spacing) {
        const int hh=std::min(spacing,bottom-y);
        const auto ry=ray_axis((cam.horizon-y-hh*.5f)/cam.focal*aspect);
        const auto yt=intersect_axis(origin.y,ry,bank.lower.y,bank.upper.y);
        for (int x=box.x;x<right;x+=spacing) {
            const auto& col=columns[x/2];
            const auto& rx=col.rx;
            const auto& rz=col.rz;
            const float begin=std::max({near,col.x.entry,yt.entry,col.z.entry});
            const float exit=std::min({col.x.exit,yt.exit,col.z.exit});
            if (exit-begin<.0002f) continue;
            const int ww=std::min(spacing,right-x);
            std::uint16_t scene_depth=0;
            for (int j=0;j<hh && scene_depth!=65535;++j) {
                const auto* row=out.depth+(y+j)*out.width+x;
                for (int i=0;i<ww;++i) {
                    scene_depth=std::max(scene_depth,row[i]);
                    if (scene_depth==65535) break;
                }
            }
            const float limit=std::min(exit,scene_depth*(.125f*inv_voxel));
            if (limit-begin<.0002f) continue;
            const float at=begin+.0001f;
            int ix=std::clamp(int(origin.x+rx.direction*at),0,CloudField::width-1);
            int iy=std::clamp(int(origin.y+ry.direction*at),0,CloudField::height-1);
            int iz=std::clamp(int(origin.z+rz.direction*at),0,CloudField::length-1);
            float nx=(ix+(rx.step>0)-origin.x)*rx.inverse;
            float ny=(iy+(ry.step>0)-origin.y)*ry.inverse;
            float nz=(iz+(rz.step>0)-origin.z)*rz.inverse;
            const float tx=std::abs(rx.inverse),ty=std::abs(ry.inverse),tz=std::abs(rz.inverse);
            const int step_y=ry.step*CloudField::width;
            const int step_z=rz.step*CloudField::height*CloudField::width;
            int offset=CloudField::Bank::index(ix,iy,iz);
            float distance=begin, first=limit, transmission=1.f, illuminated=0.f;

            // Integrate the density along each voxel crossing. Light from
            // exposed wisps contributes first, before light through the core.
            while (distance<limit) {
                const bool cross_x=nx<=ny && nx<=nz;
                const bool cross_y=ny<=nz;
                const float next=cross_x ? nx : cross_y ? ny : nz;
                const float chord=std::min(next,limit)-distance;
                const auto value=bank.voxels[offset];
                const int density=value&15;
                if (density && chord>.0001f) {
                    if (first==limit) first=distance;
                    const float thickness=chord*density*.073f;
                    const float scatter=transmission*thickness/(1+thickness);
                    transmission-=scatter;
                    illuminated+=scatter*float(value>>4);
                    if (transmission<.025f) break;
                }
                distance=next;
                if (cross_x) {
                    ix+=rx.step; nx+=tx; offset+=rx.step;
                    if (unsigned(ix)>=CloudField::width) break;
                } else if (cross_y) {
                    iy+=ry.step; ny+=ty; offset+=step_y;
                    if (unsigned(iy)>=CloudField::height) break;
                } else {
                    iz+=rz.step; nz+=tz; offset+=step_z;
                    if (unsigned(iz)>=CloudField::length) break;
                }
            }
            const float coverage=1-transmission;
            if (coverage<.085f) continue;
            const int alpha=std::clamp(int(coverage*16+.5f)*2,0,32);
            const int lit=std::clamp(int(illuminated*(8.f/15.f)/coverage+.5f),0,8);
            const auto front=std::uint16_t(first*voxel*8);
            for (int j=0;j<hh;++j) for (int i=0;i<ww;++i) {
                if (front>=out.depth[(y+j)*out.width+x+i]) continue;
                auto& pixel=out.pixels[(y+j)*out.stride+x+i];
                pixel=blend(pixel,palette[lit],alpha);
            }
        }
    }
}

void draw_overhead(Scene& scene,const CloudField::Bank& bank,Vec center,Rect box) {
    auto& out=scene.out;
    const auto& cam=scene.camera;
    const float unit=CloudField::cell*bank.scale, height=CloudField::layer*bank.scale;
    const auto step_x=cam.axes.right*(1/(cam.focal*unit));
    const auto step_y=cam.axes.up*(-1/(cam.focal*unit));
    const Vec origin=(cam.pos-center)*(1/unit)+CloudField::pivot
                     +step_x*(.5f-cam.cx)+step_y*(.5f-cam.horizon);
    std::array<Color,16> palette;
    for (int i=0;i<16;++i) palette[i]=blend(rgb(0x8daec6),rgb(0xfff7e6),i*32/15);
    for (int y=box.y;y<box.y+box.h;++y) {
        auto p=origin+step_y*float(y)+step_x*float(box.x);
        for (int x=box.x;x<box.x+box.w;++x,p+=step_x) {
            const int tx=int(std::floor(p.x)),tz=int(std::floor(p.z));
            if (unsigned(tx)>=CloudField::width || unsigned(tz)>=CloudField::length) continue;
            const unsigned column=bank.columns[tz*CloudField::width+tx];
            if (!(column&63)) continue;
            const float depth=cam.pos.y-(center.y+(float(column>>10)-CloudField::pivot.y)*height);
            if (depth*8>=out.depth[y*out.width+x]) continue;
            auto& pixel=out.pixels[y*out.stride+x];
            pixel=blend(pixel,palette[(column>>6)&15],column&63);
        }
    }
}

} // namespace

void CloudField::generate_next(std::uint32_t seed) {
    if (ready()) return;
    const unsigned i=generated;
    constexpr Vec centers[bank_count] = {
        {1775,182,2178},{2258,202,2330},{2032,85,2490},{2600,224,2700},
        {1610,279,2710},{2190,256,3040},{1415,202,3130},{3090,196,2100},
        {1420,132,1700},{2420,260,1500},{2940,217,1130},{1810,240,1050},
        {990,223,2300},{840,207,1330},{3570,225,3500},{1750,236,3660},
        {1980,176,2630}
    };
    auto& bank=banks[i];
    Random random{hash(seed,i+1)};
    bank.center=centers[i]+Vec{random.between(-21,21),random.between(-10,10),random.between(-23,23)};
    bank.scale=random.between(.72f,1.27f);
    const int shape=(int(seed&7)+int(i)*3)%5;
    const float loft=std::array{.73f,1.05f,1.21f,.54f,.90f}[shape]*random.between(.84f,1.15f);
    const float spread=std::array{1.04f,.81f,.72f,1.28f,.92f}[shape];
    const float heading=random.between(-pi,pi);
    const Vec downwind{std::cos(heading),0,std::sin(heading)};
    const Vec crosswind{-downwind.z,0,downwind.x};
    const auto at=[&](float along,float across,float y) {
        return downwind*along+crosswind*across+Vec{0,y,0};
    };
    const auto make=[&](Vec point,Vec radius,float strength,float yaw) {
        const float angle=heading+yaw;
        return Puff{point,{1/radius.x,1/radius.y,1/radius.z},std::cos(angle),std::sin(angle),strength};
    };
    std::array<Puff,12> puffs;
    const int count=std::clamp(int(random.between(7,12)),7,11);
    puffs[0]=make(at(random.between(-20,6),random.between(-13,14),-4),
                  {random.between(32,53)*spread,random.between(13,23)*loft,random.between(19,33)},
                  1.f,random.between(-.33f,.33f));
    puffs[1]=make(at(random.between(-37,-13)*spread,random.between(-18,14),random.between(8,24)*loft),
                  {random.between(23,36)*spread,random.between(19,30)*loft,random.between(17,27)},
                  random.between(.81f,1.18f),random.between(-.6f,.6f));
    puffs[2]=make(at(random.between(18,46)*spread,random.between(-25,15),random.between(-4,18)*loft),
                  {random.between(19,36)*spread,random.between(13,25)*loft,random.between(15,29)},
                  random.between(.75f,1.14f),random.between(-.6f,.6f));
    for (int p=3;p<count-1;++p) {
        const int branch=int(random.between(0,3));
        const auto point=puffs[branch].center+
            at(random.between(-34,30),random.between(-24,27),random.between(-12,17)*loft);
        puffs[p]=make(point,{random.between(10,25)*spread,random.between(8,18)*loft,
                            random.between(9,22)},random.between(.55f,1.f),random.between(-1.f,1.f));
    }
    puffs[count-1]=make(at(random.between(43,66)*spread,random.between(-17,19),random.between(-6,10)),
                        {random.between(19,31),random.between(6,11),random.between(10,17)},
                        random.between(.42f,.68f),random.between(-.3f,.3f));

    const float base=random.between(-22,-15);
    const Noise<9> broad(random);
    const Noise<4> billows(random);
    const Noise<2> fringe_noise(random);
    bank.lower={width,height,length};
    bank.upper={0,0,0};
    for (int y=1;y<height-1;++y) {
        const float py=(y+.5f-pivot.y)*layer;
        std::array<const Puff*,12> active;
        int candidates=0;
        for (int p=0;p<count;++p) {
            const auto& puff=puffs[p];
            // Include the maximum vertical warp and positive noise contribution.
            const float distance=std::max(0.f,std::abs(py-puff.center.y)-5.f)*puff.inverse_radius.y;
            if (distance*distance<puff.strength+.49f) active[candidates++]=&puff;
        }
        if (!candidates) continue;
        for (int z=1;z<length-1;++z) for (int x=1;x<width-1;++x) {
            const float pz=(z+.5f-pivot.z)*cell;
            const float px=(x+.5f-pivot.x)*cell;
            const float broadening=broad.sample(x,y,z);
            const float swell=billows.sample(x,y,z);
            const float base_fade=saturate((py-base+5-broadening*12)/14);
            const float edge=saturate(std::min({x,width-1-x,y,height-1-y,z,length-1-z})*.5f);
            const Vec point{px+swell*5,py+broadening*5,pz-swell*4};
            float core=-1.f,overlap=0;
            for (int p=0;p<candidates;++p) {
                const float density=active[p]->envelope(point);
                core=std::max(core,density);
                overlap+=std::max(0.f,density+.19f);
            }
            const float erosion=broadening*.22f+swell*.19f+fringe_noise.sample(x,y,z)*.075f;
            const float density=saturate((core+overlap*.07f+erosion)*2.35f)*base_fade*edge;
            const auto packed=std::uint8_t(density*15+.5f);
            bank.voxels[Bank::index(x,y,z)]=packed;
            if (packed) {
                bank.lower={std::min(bank.lower.x,float(x)),std::min(bank.lower.y,float(y)),
                            std::min(bank.lower.z,float(z))};
                bank.upper={std::max(bank.upper.x,float(x+1)),std::max(bank.upper.y,float(y+1)),
                            std::max(bank.upper.z,float(z+1))};
            }
        }
    }

    // At each occupied voxel, trace toward the sun through the same
    // density field. Also project the whole cloud onto the sea-level grid.
    for (int z=0;z<length;++z) for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        auto& v=bank.voxels[Bank::index(x,y,z)];
        const int density=v&15;
        if (!density) continue;
        int blocking=0;
        for (int up=1;up<=10 && y+up<height;++up) {
            const int sx=x-int(up*(shadow_x*layer/cell)+.5f);
            const int sz=z-int(up*(shadow_z*layer/cell)+.5f);
            if (sx<0 || sz<0) break;
            blocking+=bank.sample(sx,y+up,sz)&15;
        }
        const int lit=std::clamp(15-blocking/8,3,15);
        v=std::uint8_t(density | (lit<<4));
        const float altitude=bank.center.y+(y+.5f-pivot.y)*layer*bank.scale;
        const float px=bank.center.x+(x+.5f-pivot.x)*cell*bank.scale+altitude*shadow_x;
        const float pz=bank.center.z+(z+.5f-pivot.z)*cell*bank.scale+altitude*shadow_z;
        const int sx=int(std::floor(px)) - World::cell/2;
        const int sz=int(std::floor(pz)) - World::cell/2;
        const int gx=World::tile(sx),gz=World::tile(sz);
        const int fx=sx&(World::cell-1),fz=sz&(World::cell-1);
        const auto spread=[&](int x,int z,int coverage) {
            auto& strength=shadows[World::index(x,z)];
            strength=std::uint8_t(std::min(150,int(strength)+(density*coverage+128)/256));
        };
        spread(gx,gz,(16-fx)*(16-fz));
        spread(gx+1,gz,fx*(16-fz));
        spread(gx,gz+1,(16-fx)*fz);
        spread(gx+1,gz+1,fx*fz);
    }
    for (int z=0;z<length;++z) for (int x=0;x<width;++x) {
        float through=1.f,illuminated=0;
        unsigned top=0;
        for (int y=height-1;y>=0;--y) {
            const unsigned voxel=bank.sample(x,y,z);
            if (!(voxel&15)) continue;
            if (!top) top=y+1;
            const float thickness=(voxel&15)*.052f;
            const float scatter=through*thickness/(1.f+thickness);
            through-=scatter;
            illuminated+=scatter*float(voxel>>4);
            if (through<.03f) break;
        }
        const float coverage=1-through;
        bank.columns[z*width+x]=coverage<.085f ? 0 :
            std::uint16_t((top<<10)|(unsigned(illuminated/coverage+.5f)<<6)|unsigned(coverage*32+.5f));
    }
    ++generated;
}

float CloudField::density_at(Vec position,float time) const {
    position=position-wind(time);
    float density=0;
    for (unsigned i=0;i<generated;++i) {
        const auto& bank=banks[i];
        auto offset=position-bank.center;
        if (std::abs(offset.y)>112.f) continue;
        offset.x=World::wrap(offset.x);
        offset.z=World::wrap(offset.z);
        if (std::abs(offset.x)>160.f || std::abs(offset.z)>112.f) continue;
        const float scale=1.f/bank.scale;
        const Vec point{offset.x*scale/cell+pivot.x-.5f,
                        offset.y*scale/layer+pivot.y-.5f,
                        offset.z*scale/cell+pivot.z-.5f};
        if (point.x<0 || point.x>=width-1 || point.y<0 || point.y>=height-1 ||
            point.z<0 || point.z>=length-1) continue;
        const int x=int(point.x),y=int(point.y),z=int(point.z);
        const float fx=point.x-x,fy=point.y-y,fz=point.z-z;
        const auto slice=[&](int depth) {
            const auto a=mix(bank.sample(x,y,depth)&15,bank.sample(x+1,y,depth)&15,fx);
            const auto b=mix(bank.sample(x,y+1,depth)&15,bank.sample(x+1,y+1,depth)&15,fx);
            return mix(a,b,fy);
        };
        density=std::max(density,mix(slice(z),slice(z+1),fz));
    }
    return density*(1.f/15);
}

void CloudField::draw(Scene& scene,float time) const {
    struct Visible { const Bank* bank; Vec center; float depth; Rect box; };
    std::array<Visible,bank_count> list;
    int count=0;
    const Vec movement=wind(time);
    for (unsigned i=0;i<generated;++i) {
        const auto& bank=banks[i];
        Vec center=bank.center+movement;
        center.x=scene.camera.pos.x+World::wrap(center.x-scene.camera.pos.x);
        center.z=scene.camera.pos.z+World::wrap(center.z-scene.camera.pos.z);
        const auto local=scene.camera.local(center);
        if (local.z>3250.f || local.z<-170.f) continue;
        const auto box=projected_box(scene,bank,center);
        if (box.w && box.h) list[count++]={&bank,center,local.z,box};
    }
    std::sort(list.begin(),list.begin()+count,[](const auto& a,const auto& b) { return a.depth>b.depth; });
    for (int i=0;i<count;++i) {
        if (scene.camera.overhead) draw_overhead(scene,*list[i].bank,list[i].center,list[i].box);
        else draw_bank(scene,*list[i].bank,list[i].center,list[i].box);
    }
}

} // namespace sky
