#include "convoy.hpp"
#include "draw.hpp"
#include "fleet.hpp"
#include "aircraft.hpp"

namespace sky {
namespace {

constexpr Color foam=rgb(0xd7f2e9),wake_color=rgb(0x69b6b4),lamp=rgb(0xc4f1c7);

bool in_frame(const Scene& scene,Vec local,float radius,float far) {
    if (local.z+radius<Scene::near || local.z>far) return false;
    if (local.z<=Scene::near+radius) return true;
    const auto point=scene.camera.project(local);
    const float margin=scene.camera.scale_at(local.z-radius)*radius+2;
    return point.x>=-margin && point.x<=scene.out.width+margin &&
           point.y>=-margin && point.y<=scene.out.height+margin;
}

struct Mesh {
    Scene& scene;
    Vec center;
    Basis axes;
    int shade,haze;
    Vec project(Vec p) const { return center+axes.apply(p); }
    Color lit(Color c) const {
        if (shade) c=blend(c,rgb(0x273f53),shade);
        return blend(c,scene.fog,haze);
    }
    void tri(Vec a,Vec b,Vec c,Color color) const {
        scene.triangle_local(project(a),project(b),project(c),lit(color));
    }
    void quad(Vec a,Vec b,Vec c,Vec d,Color color) const {
        const auto p=project(a),q=project(c);
        color=lit(color);
        scene.triangle_local(p,project(b),q,color);
        scene.triangle_local(p,q,project(d),color);
    }
    void box(Vec left,Vec right,Color color) const {
        const Vec a{left.x,right.y,left.z},b{right.x,right.y,left.z};
        const Vec c{right.x,right.y,right.z},d{left.x,right.y,right.z};
        const Vec bottom{0,left.y-right.y,0};
        quad(a,b,c,d,blend(color,rgb(0xfff0c9),6));
        quad(a,a+bottom,b+bottom,b,blend(color,rgb(0x081b2e),9));
        quad(a,d,d+bottom,a+bottom,color);
        quad(b,b+bottom,c+bottom,c,blend(color,rgb(0x081b2e),11));
        quad(d,c,c+bottom,d+bottom,color);
    }
};

void wash(Scene& scene,const Convoy& ship,float time) {
    if (ship.trail_count<2) return;
    const auto trail_at=[&](unsigned age) -> const Convoy::Trail& {
        return ship.trail[(ship.next_trail+ship.trail.size()-1-age)%ship.trail.size()];
    };
    constexpr Vec surface{0,.18f,0};
    const unsigned tick=unsigned(time*6);
    for (unsigned i=1;i<ship.trail_count;++i) {
        const auto& before=trail_at(i-1);
        const auto& after=trail_at(i);
        const float width=5.4f+i*1.66f;
        const int fade=26-int(i)*25/int(ship.trail.size());
        if (i%2) {
            const auto left=after.stern-after.side*(i*.48f)+surface;
            const auto right=after.stern+after.side*(i*.48f)+surface;
            scene.quad(before.stern+surface-before.side*2,left,right,
                       before.stern+surface+before.side*2,wake_color,fade/3);
        }
        for (float sign:{-1.f,1.f}) {
            const auto salt=hash(i,tick/2+unsigned(sign>0));
            const Vec outer=after.stern+after.side*(sign*width)+surface;
            const Vec inner=before.stern+before.side*(sign*(width-1.66f))+surface;
            const Vec wave=outer-inner;
            const float start=float(salt&7)*.031f;
            const float end=.85f+float((salt>>5)&3)*.037f;
            scene.stroke(inner+wave*start,inner+wave*end,foam,1,fade);
            if ((salt&256) && fade>12) {
                const Vec broken=after.side*(sign*2.2f);
                scene.stroke(inner+broken+wave*.40f,inner+broken+wave*.84f,wake_color,1,fade/2);
            }
        }
    }
}

void ship_model(const Mesh& mesh,const Convoy& ship,float pixels,float time) {
    constexpr Vec outline[]={{0,4.4f,29},{6.6f,4.0f,19},{8.f,4.f,7},{7.8f,4.f,-21},
                             {5.6f,4.f,-27},{-5.6f,4.f,-27},{-7.8f,4.f,-21},
                             {-8.f,4.f,7},{-6.6f,4.f,19}};
    constexpr Color hull=rgb(0x34566b),red=rgb(0x995847),deck=rgb(0xb2b1a0);
    const auto bottom=[](Vec p,float y) {
        return Vec{p.x*.80f,y,p.z*.97f};
    };
    for (unsigned i=0;i<std::size(outline);++i) {
        const Vec a=outline[i],b=outline[(i+1)%std::size(outline)];
        const auto stripe_a=bottom(a,1.3f),stripe_b=bottom(b,1.3f);
        mesh.quad(a,b,stripe_b,stripe_a,(a.x+b.x)<0 ? hull : rgb(0x426d79));
        mesh.quad(stripe_a,stripe_b,bottom(b,.08f),bottom(a,.08f),red);
        mesh.tri({0,4,0},b,a,deck);
    }
    mesh.box({-5.4f,4.f,-20},{5.4f,8.6f,-9},rgb(0xe2e5d4));
    mesh.quad({-4.7f,7.0f,-8.97f},{4.7f,7.0f,-8.97f},{4.7f,8.1f,-8.97f},{-4.7f,8.1f,-8.97f},rgb(0x345d70));
    mesh.box({-1.6f,8.5f,-17.8f},{1.6f,11.4f,-15.0f},rgb(0x3d4c50));
    if (pixels<5.f) {
        mesh.box({-5.8f,4.f,-6},{5.8f,7.2f,12},rgb(0xb78761));
        return;
    }
    const Color cargo[]{rgb(0xb27158),rgb(0xd0a347),rgb(0x679997)};
    for (unsigned row=0;row<3;++row) {
        const float z=-6.3f+row*7.2f;
        for (unsigned side=0;side<2;++side) {
            const float x=side ? .35f : -5.5f;
            const float top=7.15f+((row+side)%3)*.62f;
            const Color paint=cargo[(row*2+side)%std::size(cargo)];
            mesh.box({x,4,z},{x+5.15f,top,z+6.5f},paint);
            if (pixels>10.f) for (float seam:{1.15f,2.65f,4.15f}) {
                const float a=x+seam;
                mesh.quad({a,top+.04f,z+.18f},{a+.19f,top+.04f,z+.18f},
                          {a+.19f,top+.04f,z+6.27f},{a,top+.04f,z+6.27f},
                          blend(paint,hull,12));
            }
        }
    }
    const Color light=ship.damage_flash>0 && (int(time*22)&1) ? rgb(0xffa15d) : lamp;
    mesh.box({-.35f,8.6f,-12.3f},{.35f,13.f,-11.6f},rgb(0x6e8384));
    mesh.box({-.75f,12.8f,-12.65f},{.75f,13.65f,-11.25f},light);
    mesh.quad({-.55f,4.08f,18},{.55f,4.08f,18},{.55f,4.23f,24.6f},{-.55f,4.23f,24.6f},rgb(0xffefb8));
}

void marker(Scene& scene,Vec pos,float phase) {
    const Vec where=scene.camera.local(pos);
    if (!in_frame(scene,where,9,1100)) return;
    const Camera& camera=scene.camera;
    const float bob=.35f*std::sin(phase);
    const Mesh mesh{scene,camera.local(pos+Vec{0,bob,0}),
        {camera.local(camera.pos+Vec{1,0,0}),camera.local(camera.pos+Vec{0,1,0}),
         camera.local(camera.pos+Vec{0,0,1})},0,int(saturate(where.z/2200)*19)};
    mesh.box({-3.4f,0,-3.4f},{3.4f,1.6f,3.4f},rgb(0x248982));
    mesh.box({-.75f,1.6f,-.75f},{.75f,6.0f,.75f},rgb(0xbce8d1));
    mesh.box({-1.25f,6,-1.25f},{1.25f,7.7f,1.25f},phase-int(phase)> .55f ? lamp : rgb(0x328d80));
}

void navy_model(const Mesh& mesh,const SurfaceUnit& unit,float pixels,float time) {
    const bool carrier=unit.carrier();
    const float w=carrier ? 22.f : 6.6f,l=carrier ? 59.f : 17.2f,h=carrier ? 7.2f : 3.3f;
    const Color marking=unit.friendly() ? rgb(0x7ac4ab) : rgb(0xcb765e);
    const Color rim_color=unit.friendly() ? rgb(0xa7bab9) : rgb(0x987d73);
    auto hull=unit.friendly() || !carrier ? rgb(0x55758d) : rgb(0x49525d);
    if (!unit.hull) hull=blend(hull,rgb(0x293940),22);
    const auto deck=blend(hull,rgb(0x3f4654),25);
    const float bow=w*(carrier ? .28f : .13f);
    const Vec outline[]{{-bow,h,l},{bow,h,l},{w*.89f,h,l*.66f},{w,h,-l*.67f},
                         {w*.85f,h,-l},{-w*.85f,h,-l},{-w,h,-l*.67f},{-w*.83f,h,l*.62f}};
    const auto bottom=[](Vec p){return Vec{p.x*.74f,.08f,p.z*.91f};};
    for (unsigned i=0;i<std::size(outline);++i) {
        const Vec a=outline[i],b=outline[(i+1)%std::size(outline)];
        const Vec a0=bottom(a),b0=bottom(b),rim{0,.94f,0};
        const Vec bevel{0,carrier ? .78f : .33f,0};
        const Color flank=blend(hull,rgb(0x223f55),(a.x+b.x)>0 ? 4 : 16);
        mesh.quad(a,b,b-bevel,a-bevel,unit.hull ? rim_color : flank);
        mesh.quad(a-bevel,b-bevel,b0+rim,a0+rim,flank);
        mesh.quad(a0+rim,b0+rim,b0,a0,rgb(0x8b5043));
        mesh.tri({0,h,0},a,b,deck);
    }
    if (!carrier) {
        mesh.box({-3.2f,h,-6.9f},{3.4f,h+2.56f,1.4f},rim_color);
        mesh.quad({-3.f,h+1.4f,1.45f},{3.13f,h+1.4f,1.45f},
                  {2.55f,h+2.5f,.6f},{-2.55f,h+2.5f,.6f},rgb(0x1c4558));
        mesh.box({-3.03f,h+2.55f,-6.65f},{3.06f,h+2.98f,.57f},hull);
        mesh.box({-1.89f,h,5.6f},{1.89f,h+1.4f,9.2f},blend(hull,marking,16));
        mesh.box({-.40f,h+1.05f,8},{.40f,h+1.65f,14.2f},rgb(0x48515a));
        if (pixels>8.f) {
            mesh.box({1.65f,h,-13},{3.5f,h+.95f,-9.4f},rgb(0xa67a4c));
            mesh.box({1.65f,h+.94f,-12.7f},{2.08f,h+4.6f,-12.12f},hull);
            mesh.box({1.8f,h+4.29f,-12.8f},{2.25f,h+4.7f,-8.7f},hull);
            mesh.box({-.28f,h+2.95f,-3.2f},{.28f,h+4.5f,-2.65f},hull);
            mesh.box({-.78f,h+4.38f,-3.65f},{.78f,h+4.75f,-2.25f},rgb(0xffca81));
        }
        return;
    }
    const float y=h+.05f;
    const auto lane=[&](float x,float z,float width,float length,Color color) {
        mesh.quad({x,y,z},{x+width,y,z},{x+width,y,z+length},{x,y,z+length},color);
    };
    lane(-15,-55,20,98,blend(deck,rgb(0x272e3a),14));
    lane(-14.7f,-52,1.05f,94,rim_color);
    mesh.quad({-4.8f,y+.03f,-52},{-2.7f,y+.03f,-52},{5.3f,y+.03f,34},
              {3.2f,y+.03f,34},marking);
    for (int i=0;i<9;++i) lane(-9.3f,-51+i*10,1.7f,6.f,rgb(0xeacfac));
    for (int i=0;i<5;++i) lane(-12.87f+i*3,-45,1.17f,6.1f,rim_color);
    mesh.box({9,h,-19},{19,h+5.4f,8},hull);
    mesh.box({8.25f,h+5.4f,-17},{19.75f,h+8.3f,8.5f},rim_color);
    mesh.box({9.15f,h+8.3f,-16.3f},{18.8f,h+9.25f,6.7f},hull);
    if (pixels<8.f) return;
    constexpr Color glass=rgb(0x203b4d);
    mesh.quad({9.12f,13.02f,8.54f},{18.83f,13.02f,8.54f},{18.83f,14.95f,8.54f},
              {9.12f,14.95f,8.54f},glass);
    for (float side:{8.20f,19.80f}) {
        mesh.quad({side,13.f,-12.f},{side,13.f,6.8f},{side,14.95f,6.8f},{side,14.95f,-12.f},glass);
        for (float z:{-7.8f,-2.3f,3.3f})
            mesh.box({side-.12f,13.0f,z},{side+.12f,14.95f,z+.39f},rim_color);
    }
    mesh.box({13,16.4f,-10},{14,24,-9},hull);
    mesh.box({9.4f,21.4f,-10.5f},{17.2f,22.1f,-8.6f},rim_color);
    mesh.box({15,16.5f,-15.8f},{17.5f,19.f,-13.f},rgb(0xc5baa4));
    const Color lights=unit.hull && (int(time*2.6f)&1) ? rgb(0xffcc71) : rgb(0xc3945c);
    for (int i=0;i<7;++i) {
        const float z=-43+i*12.4f;
        mesh.box({-18.95f,h,z},{-17.95f,h+.55f,z+1.15f},lights);
    }
    for (float side:{-1.f,1.f}) {
        const float outer=side*21.2f;
        const float z=side>0 ? -36.f : -22.f;
        mesh.box({outer-.25f,2.0f,z},{outer+.25f,5.33f,z+12.0f},rgb(0x2d404d));
        mesh.box({outer-.32f,5.35f,z+1.1f},{outer+.32f,5.87f,z+10.7f},lights);
    }
    if (unit.friendly()) for (int i=0;i<3;++i)
        lane(-13.f,-35.f+i*5.7f,15.4f,.29f,blend(rim_color,deck,14));
}

void battery_model(const Mesh& mesh,const SurfaceUnit& unit,float time) {
    const auto steel=unit.hull ? rgb(0x83906a) : rgb(0x504943);
    const Color stone=rgb(0xb6a084),dark=rgb(0x7b776c),trim=rgb(0xdbbd8c);
    constexpr Vec outline[]{{-6.8f,0,10.8f},{6.8f,0,10.8f},{10.5f,0,6.2f},{10.5f,0,-6.2f},
                             {6.8f,0,-10.5f},{-6.8f,0,-10.5f},{-10.5f,0,-6.2f},{-10.5f,0,6.2f}};
    for (unsigned i=0;i<std::size(outline);++i) {
        const Vec a=outline[i],b=outline[(i+1)%std::size(outline)];
        const auto ledge=[](Vec p,float scale,float y) {return Vec{p.x*scale,y,p.z*scale};};
        mesh.quad(a,b,ledge(b,.92f,1.52f),ledge(a,.92f,1.52f),(i&2) ? dark : stone);
        mesh.quad(ledge(a,.92f,1.52f),ledge(b,.92f,1.52f),ledge(b,.72f,4.17f),ledge(a,.72f,4.17f),stone);
        mesh.tri({0,4.17f,0},ledge(a,.72f,4.17f),ledge(b,.72f,4.17f),trim);
    }
    const Color shutter=rgb(0x333f43);
    mesh.quad({-5.4f,.64f,10.51f},{5.4f,.64f,10.51f},{4.9f,2.52f,9.55f},{-4.9f,2.52f,9.55f},shutter);
    mesh.quad({-5.1f,1.27f,10.31f},{5.1f,1.27f,10.31f},{5.0f,1.47f,10.23f},{-5.f,1.47f,10.23f},dark);
    mesh.box({-4.8f,4.16f,-4.55f},{4.8f,5.6f,3.93f},dark);
    mesh.quad({-4.8f,5.60f,3.93f},{4.8f,5.60f,3.93f},{3.12f,8.05f,2.37f},{-3.12f,8.05f,2.37f},steel);
    mesh.box({-3.12f,5.6f,-4.0f},{3.12f,8.05f,2.34f},steel);
    for (float x:{-2.f,2.f}) {
        mesh.box({x-.55f,6.4f,2.4f},{x+.55f,7.64f,6.37f},steel);
        mesh.box({x-.34f,6.63f,6.3f},{x+.34f,7.45f,10.56f},rgb(0x535d54));
    }
    mesh.box({-.7f,8.2f,-1.5f},{.7f,9.4f,1.5f},
             unit.hull && (int(time*4)&1) ? rgb(0xff9663) : steel);
}

} // namespace

void draw_convoy(Scene& scene,const Convoy& convoy,float time,int shade) {
    constexpr Vec dock=Convoy::route[std::size(Convoy::route)-1];
    marker(scene,dock+Vec{-31,0,0},time*1.1f);
    marker(scene,dock+Vec{31,0,0},time*1.1f+.35f);
    const auto center=scene.camera.local(convoy.pos);
    if (convoy.hull && in_frame(scene,center,92.f,1150.f)) wash(scene,convoy,time);
    if (!in_frame(scene,center,32.f,1450.f)) return;
    const auto& cam=scene.camera.axes;
    const auto turn=[&](Vec v) { return Vec{dot(v,cam.right),dot(v,cam.up),dot(v,cam.front)}; };
    const float bob=convoy.hull ? .25f*std::sin(time*3.8f) : -2.0f;
    const auto pose=attitude(convoy.yaw,.022f*std::sin(time*2.7f),
                            convoy.hull ? .033f*std::sin(time*3.4f) : .31f);
    const auto flat=attitude(convoy.yaw);
    const Vec shadow=convoy.pos+Vec{4.f*shadow_x,.04f,4.f*shadow_z};
    const Vec port=flat.right*8.3f,fore=flat.front*23.f;
    scene.quad(shadow-fore-port,shadow-fore+port,shadow+fore+port*.68f,
               shadow+fore-port*.68f,rgb(0x173e54),11);
    const Mesh mesh{scene,center+turn({0,bob,0}),{turn(pose.right),turn(pose.up),turn(pose.front)},
                    shade,int(saturate((center.z-240.f)/1700.f)*27)};
    ship_model(mesh,convoy,scene.camera.scale_at(std::max(center.z,Scene::near))*8.f,time);
    if (!convoy.hull) return;
    for (float side:{-1.f,1.f}) {
        const Vec bow=convoy.pos+pose.front*28.f+Vec{0,.23f,0};
        const Vec crest=bow-pose.front*12+pose.right*(10.f*side);
        scene.stroke(bow,crest,foam,1,20);
        scene.stroke(crest,crest-pose.front*8+pose.right*(3.f*side),wake_color,1,15);
    }
}

void draw_surface_unit(Scene& scene,const SurfaceUnit& unit,float time,int shade) {
    if (unit.afloat() && !unit.hull && unit.wreck_age>5.1f) return;
    const auto pos=scene.camera.local(unit.pos);
    if (!in_frame(scene,pos,unit.carrier() ? 136.f : 27.f,1620.f)) return;
    const auto& cam=scene.camera.axes;
    const auto turn=[&](Vec p){return Vec{dot(p,cam.right),dot(p,cam.up),dot(p,cam.front)};};
    const float sink=unit.afloat() && !unit.hull ? -unit.wreck_age*(unit.carrier() ? 3.25f : 2.1f) : 0.f;
    const float rolls=unit.role==SurfaceRole::gunboat && unit.hull ? .014f*std::sin(time*2.1f) : 0;
    const auto axes=attitude(unit.yaw,0,sink*.025f+rolls);
    const float width=unit.carrier() ? CarrierDeck::half_width/22.f : 1.f;
    const float height=unit.carrier() ? CarrierDeck::height/7.2f : 1.f;
    const float length=unit.carrier() ? CarrierDeck::half_length/59.f : 1.f;
    const Mesh mesh{scene,pos+turn({0,sink,0}),{turn(axes.right)*width,turn(axes.up)*height,turn(axes.front)*length},
                    shade,int(saturate((pos.z-240.f)/1700.f)*27)};
    if (unit.afloat()) {
        navy_model(mesh,unit,scene.camera.scale_at(std::max(pos.z,Scene::near))*22.f,time);
        if (unit.carrier() && unit.hull && pos.z<965.f) {
            constexpr Airframe parked[]{Airframe::patrol,Airframe::dart};
            for (unsigned i=0;i<CarrierDeck::parking.size();++i) {
                const auto loc=CarrierDeck::parking[i];
                const auto center=unit.pos+axes.apply(loc);
                const auto shadow=center+Vec{1.5f,-2.96f,1.1f};
                const Vec long_edge=axes.right*10.f,wide_edge=axes.front*6.5f;
                scene.quad(shadow-long_edge-wide_edge,shadow+long_edge-wide_edge,
                           shadow+long_edge+wide_edge,shadow-long_edge+wide_edge,rgb(0x253641),18);
                Craft craft{center,unit.yaw+pi*.5f,0,0,0,parked[i]};
                craft.gear=true;
                draw_aircraft(scene,craft,unit.friendly() ? Paint::deck : Paint::enemy,0,0,time);
            }
        }
        if (unit.hull && unit.role==SurfaceRole::gunboat) {
            const Vec wake=unit.pos-axes.front*14+Vec{0,.16f,0};
            for (float side:{-1.f,1.f}) {
                const auto edge=wake+axes.right*(side*5.9f)-axes.front*11.f;
                scene.stroke(wake,edge,foam,1,19);
                scene.stroke(edge,edge+axes.right*(side*2.7f)-axes.front*8,wake_color,1,13);
            }
        }
    } else battery_model(mesh,unit,time);
}

} // namespace sky
