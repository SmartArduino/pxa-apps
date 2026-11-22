#include "world.hpp"

namespace sky {
namespace {

struct Drainage {
    float bank=42.f,waterline=0.f;
    bool flowing=false;
};
struct RiverPoint { float x,z,width,waterline; };

template<std::size_t N>
void trace_river(Drainage& stream,float x,float z,const RiverPoint (&points)[N]) {
    if (!stream.flowing && stream.bank<0) return;
    for (unsigned i=1;i<N;++i) {
        const auto a=points[i-1],b=points[i];
        const float reach=std::max(a.width,b.width)+42.f;
        if (x<std::min(a.x,b.x)-reach || x>std::max(a.x,b.x)+reach ||
            z<std::min(a.z,b.z)-reach || z>std::max(a.z,b.z)+reach) continue;
        const float dx=b.x-a.x,dz=b.z-a.z,px=x-a.x,pz=z-a.z;
        const float t=saturate((px*dx+pz*dz)/(dx*dx+dz*dz));
        const float across_x=px-dx*t,across_z=pz-dz*t;
        const float bank=std::sqrt(across_x*across_x+across_z*across_z)-mix(a.width,b.width,t);
        if (bank<stream.bank) stream={bank,mix(a.waterline,b.waterline,t),true};
    }
}

Drainage drainage_at(float x,float z) {
    struct Lake { float x,z,rx,rz,shear,waterline; };
    constexpr Lake lakes[]{
        {1538,2498,137,124,.31f,14}, {1587,2550,86,104,.47f,14},
        {2508,2675,118,179,-.34f,17}, {2577,2770,88,80,-.28f,17},
        {3330,1222,149,105,-.48f,21}, {3505,1110,82,124,.17f,24},
        {1578,1525,87,126,-.37f,19},
    };
    Drainage basin;
    for (const auto lake:lakes) {
        const float v=z-lake.z,u=x-lake.x+v*lake.shear;
        if (std::abs(u)>lake.rx+62 || std::abs(v)>lake.rz+62) continue;
        const float a=u/lake.rx,b=v/lake.rz;
        const float wobble=8*std::sin(x*.038f+z*.018f)+6*std::sin(z*.052f-x*.026f);
        const float bank=(std::sqrt(a*a+b*b)-1.f)*std::min(lake.rx,lake.rz)+wobble;
        if (bank<basin.bank) basin={bank,lake.waterline,false};
    }
    constexpr RiverPoint west[]{
        {1570,2498,35,14},{1654,2474,34,14},{1710,2518,30,11},
        {1752,2501,28,9},{1802,2557,28,6},{1851,2546,31,3},{1912,2576,42,0},
    };
    constexpr RiverPoint east[]{
        {2495,2640,32,17},{2422,2600,30,17},{2378,2622,27,13},
        {2317,2570,30,10},{2286,2598,26,8},{2243,2538,29,5},{2174,2523,43,0},{2115,2512,34,0},
    };
    constexpr RiverPoint north[]{
        {3310,1222,29,21},{3240,1299,27,20},{3303,1385,29,14},
        {3251,1430,31,10},{3295,1490,33,5},{3254,1582,47,0},
    };
    trace_river(basin,x,z,west);
    trace_river(basin,x,z,east);
    trace_river(basin,x,z,north);
    return basin;
}

} // namespace

void World::generate() {
    constexpr Color sea[]{rgb(0x9cd0b2),rgb(0x55c5b7),rgb(0x279f9f),rgb(0x247f9b),
                          rgb(0x22669a),rgb(0x205485),rgb(0x1b4577),rgb(0x16375e)};
    for (int d=0;d<64;++d) {
        const int j=std::min(6,d/8);
        const auto base=blend(sea[j],sea[j+1],std::min(32,(d-j*8)*4));
        for (int band=0;band<24;++band) {
            const int wave=band-8;
            water_palette[d][band]=wave<0 ? blend(base,rgb(0x153d67),-wave)
                : blend(base,wave>=11 ? rgb(0xc8e9e2) : rgb(0x73bbc8),wave);
        }
    }
    std::array<int,64> sine;
    for (int i=0;i<64;++i) sine[i]=int(std::round(7*std::sin(i*2*pi/64)));
    const auto wave=[&](int phase) { return sine[phase&63]; };
    const auto crest=[&](int phase,int envelope) {
        const int edge=phase&63;
        return edge>=15 && edge<=17 && envelope>0 ? 8 : wave(edge);
    };
    for (int z=0;z<wave_size;++z) for (int x=0;x<wave_size;++x) {
        // Warp each train in a different way so its local bearing bends
        // through the crossings. Both phase fields wrap at the tile edge.
        const int swell_warp=(wave(x-z+14)*7+wave(x+2*z)*4+wave(x+z+8)*3)>>2;
        const int cross_warp=(wave(x+z)*8+wave(x-2*z+19)*5)>>2;
        const int swell=crest(2*x+z+swell_warp,wave(x-2*z)+wave(x+z+6)-2);
        const int cross=crest(-2*x+3*z+cross_warp,wave(2*x+z)+wave(3*z-x+20)-3);
        water_texture[wave_index(x,z)]=std::uint8_t((swell+7)|((cross+7)<<4));
    }
    struct Island { float x,z,rx,rz,peak; };
    constexpr Island islands[] = {
        {1575,2310,359,570,155}, {2475,2400,367,515,169}, {2590,2825,285,385,104},
        {1830,2990,255,350,110}, {2160,3310,480,385,185}, {1220,2820,275,390,132},
        {2780,1860,460,315,174}, {1620,1570,535,415,148}, {1060,2010,260,290,100},
        {3150,3500,485,425,187}, {790,3410,550,390,153}, {490,2400,280,480,127},
        {3260,1090,560,455,159}, {2340,940,385,495,144}, {1300,580,480,370,123},
        {410,1030,471,475,140}, {3750,2440,310,570,150}, {3220,410,370,240,122},
        {1950,3780,180,142,61}, {2930,2220,168,128,62}, {1270,1250,152,145,71},
        {2245,1944,75,43,29}, {2273,2115,64,36,31}, {1926,2317,72,64,30},
    };
    for (int z=0; z<map_size; ++z) for (int x=0; x<map_size; ++x) {
        const float a = float(x*cell+cell/2), b = float(z*cell+cell/2);
        float height = -140;
        for (auto i : islands) {
            const float u = wrap(a-i.x)/i.rx, v = wrap(b-i.z)/i.rz;
            const float d = u*u+v*v;
            if (d < 2.f) height = std::max(height, i.peak*(1.f-d));
        }
        const float strata = 9*std::sin(a*.018f+b*.013f) + 7*std::sin(b*.035f-a*.021f)
                           + 16*std::sin((a+b)*.0067f)*std::cos((b-a)*.014f);
        height += strata;
        const float relief=4*std::sin(a*.008f+b*.011f)+3*std::cos(b*.009f-a*.013f);
        const float land=std::max(0.f,height);
        const float ramp=std::min(1.f,land/45);
        const float upland=std::max(0.f,(land-55.f)/100.f);
        const float folds=4.f*std::cos(a*.019f-b*.026f)*std::sin(b*.010f+a*.012f);
        const float top=std::clamp(land*.22f+(relief+upland*folds)*ramp,0.f,float(highest_land));
        const int depth=std::clamp(int((4-height)*.62f),0,shoreline);
        const int here=index(x,z);
        int bed=int(top)>0 ? int(top) : -depth;
        const auto basin=drainage_at(a,b);
        terrain_features[here]=0;
        if (basin.bank<39.f) {
            const float grade=basin.flowing ? .24f : .31f;
            const float cut=basin.waterline+std::clamp(basin.bank*grade,basin.flowing ? -8.f : -15.f,10.f);
            const float t=saturate((basin.bank-1.f)/38.f);
            bed=int(std::round(mix(cut,float(bed),t*t*(3-2*t))));
            if (basin.bank<33.f) {
                const int waterline=int(std::round(basin.waterline));
                terrain_features[here]=std::uint8_t((waterline<<2)|(basin.flowing ? river_flag : 0));
            }
        }
        heights[here]=std::uint8_t(std::max(0,bed));
        relief_map[sea_index(x*2,z*2)]=std::uint8_t(shoreline+bed);
    }
    // Both cameras use the same finer grid on either side of the tide line.
    for (int z=0;z<sea_size;z+=2) for (int x=1;x<sea_size;x+=2)
        relief_map[sea_index(x,z)]=(relief_map[sea_index(x-1,z)]+relief_map[sea_index(x+1,z)]+1)/2;
    for (int z=1;z<sea_size;z+=2) for (int x=0;x<sea_size;++x) {
        relief_map[sea_index(x,z)]=(relief_map[sea_index(x,z-1)]+relief_map[sea_index(x,z+1)]+1)/2;
    }
    for (int z=0; z<map_size; ++z) for (int x=0; x<map_size; ++x) {
        const int at=index(x,z), h=heights[at];
        const int waterline=terrain_features[at]>>2;
        const bool freshwater_bank=waterline>0 && h<=waterline+4;
        // The four adjacent coarse cells interpolate this vertex's waterline.
        if (waterline) for (int dz=-2;dz<2;++dz) for (int dx=-2;dx<2;++dx)
            relief_map[sea_index(x*2+dx,z*2+dz)]|=inland_margin;
        if (!h) {
            colors[at]=rgb(0xc3ad88);
            continue;
        }
        const int n=int(hash(x,z)&31);
        const int dx=int(heights[index(x+1,z)])-heights[index(x-1,z)];
        const int dz=int(heights[index(x,z+1)])-heights[index(x,z-1)];
        const int slope=std::abs(dx)+std::abs(dz);
        const float trees=.58f*std::sin(x*.16f+z*.25f)*std::sin(z*.17f-x*.08f)
                         +.29f*std::cos(x*.38f-z*.30f)+.14f*std::sin(z*.59f+x*.22f);
        if (h>4 && trees>.19f && !freshwater_bank) terrain_features[at]|=forest_cover;
        Color c;
        if (freshwater_bank) {
            const int plants=std::clamp((h-waterline)*5+n/2,0,32);
            c=blend(rgb(0x7b8558),rgb(0xb6b476),plants);
        } else if (h<=3) c=blend(rgb(0xb4a180),rgb(0xf2d6a6),h*6+n/4);
        else if (slope>10 && (h<18 || trees<.04f)) {
            const int sun=std::clamp(13+dx*2+dz+n/8,0,30);
            const Color layer=(h/3)%2 ? rgb(0xa38e77) : rgb(0x958a7f);
            c=blend(layer,rgb(0xe4c798),sun);
        }
        else if (forest(at))
            c=blend(rgb(0x274b3b),rgb(0x779155),11+n/5+int(trees*8));
        else {
            const float fields=std::cos(x*.21f-z*.12f)*std::sin(z*.16f+x*.08f);
            const int meadow=std::clamp(int(17+fields*15+trees*10)+n/6,0,32);
            if (h>23 && fields<-.28f)
                c=blend(rgb(0x62795a),rgb(0xc1a875),std::clamp((h-21)*2+n/3,0,32));
            else c=blend(rgb(0x608556),rgb(0xb7bd74),meadow);
        }
        const int sunlight=std::clamp(dx*2+dz,-14,14);
        colors[at]=sunlight>=0 ? blend(c,rgb(0xe6d88f),sunlight)
                              : blend(c,rgb(0x395750),-sunlight);
    }
}

} // namespace sky
