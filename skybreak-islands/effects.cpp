#include "effects.hpp"

namespace sky {
namespace {

constexpr Color ivory=rgb(0xfff3cf), gold=rgb(0xffc360), ember=rgb(0xf57646);
constexpr Color smoke_blue=rgb(0x5f7784), wake_white=rgb(0xe6edda);

void puff(Scene& scene,Vec position,float radius,Color color,int opacity) {
    const Vec local=scene.camera.local(position);
    if (local.z<Scene::near || local.z>=8191 || opacity<=0) return;
    const Vec projected=scene.camera.project(local);
    const int r=std::clamp(int(scene.camera.scale_at(local.z)*radius+.5f),1,9);
    auto& canvas=scene.out;
    const int x=int(projected.x),y=int(projected.y);
    const int clip_left=std::max(0,x-r),clip_right=std::min(canvas.width-1,x+r);
    const int clip_top=std::max(0,y-r),clip_bottom=std::min(canvas.height-1,y+r);
    const auto depth=std::uint16_t(local.z*8);
    for (int row=clip_top;row<=clip_bottom;++row) {
        const int trim=std::max(0,std::abs(row-y)-r/2);
        const int left=std::max(clip_left,x-r+trim),right=std::min(clip_right,x+r-trim);
        for (int col=left;col<=right;++col) {
            if (depth>canvas.depth[row*canvas.width+col]) continue;
            auto& pixel=canvas.pixels[row*canvas.stride+col];
            pixel=blend(pixel,color,opacity);
        }
    }
}

void ring(Scene& scene,Vec position,float radius,Color color) {
    constexpr Vec points[]={{1,0,0},{.71f,.71f,0},{0,1,0},{-.71f,.71f,0},{-1,0,0},
                            {-.71f,-.71f,0},{0,-1,0},{.71f,-.71f,0},{1,0,0}};
    const auto& axes=scene.camera.axes;
    const auto point=[&](Vec v) { return position+(axes.right*v.x+axes.up*v.y)*radius; };
    for (unsigned i=1;i<std::size(points);++i)
        scene.stroke(point(points[i-1]),point(points[i]),color);
}

void blast(Scene& scene,const Effect& fx,float progress) {
    constexpr Vec sparks[] = {{.83f,.31f,.38f},{-.67f,.55f,.14f},{.17f,.86f,-.29f},
                              {-.32f,-.45f,.73f},{.54f,-.34f,.58f},{-.08f,.21f,-.78f},
                              {.34f,.38f,-.71f},{-.63f,-.61f,-.17f},{.08f,-.81f,.32f},
                              {-.44f,.04f,.86f}};
    const bool explosion=fx.kind==EffectKind::explosion;
    const bool ejection=fx.kind==EffectKind::ejection;
    const float size=explosion ? 1.8f : ejection ? 1.08f : .62f;
    const Vec center=fx.pos+fx.motion*fx.age;
    const float remaining=1-progress;
    if (explosion && progress>.17f) {
        const int smoke_alpha=int(15*remaining);
        puff(scene,center+Vec{-4*progress,8*progress,0},size*(3+10*progress),smoke_blue,smoke_alpha);
        puff(scene,center+Vec{8*progress,3*progress,-6*progress},size*(2+8*progress),
             rgb(0x687b80),smoke_alpha);
        for (int i=0;i<3;++i) {
            const Vec velocity=sparks[(fx.seed+i*3)%std::size(sparks)]*52.f;
            const Vec fragment=center+velocity*fx.age+Vec{0,-38*fx.age*fx.age,0};
            const Vec edge{i&1 ? -2.4f : 2.4f,2.f-progress*5,1.3f};
            scene.stroke(fragment-edge,fragment+edge,rgb(0x52616a),2,int(31*remaining));
            scene.stroke(fragment,fragment+edge,blend(ember,rgb(0xa7a59b),int(30*progress)),1,int(30*remaining));
        }
    }
    if (progress<.58f) {
        puff(scene,center,size*(3+8*std::min(progress,.28f)),ember,int(26*remaining));
        puff(scene,center,size*2.6f,gold,int(29*(1-progress/.65f)));
        puff(scene,center,size*1.3f,ivory,int(31*(1-progress/.60f)));
    }
    if (progress<.39f && (explosion || ejection)) {
        ring(scene,center,size*(4+42*progress),blend(gold,rgb(0x9a8581),int(progress*56)));
    }
    const int count=explosion ? 10 : ejection ? 7 : 5;
    for (int i=0;i<count;++i) {
        const unsigned noise=hash(fx.seed,i);
        auto axis=sparks[(i+(fx.seed%std::size(sparks)))%std::size(sparks)];
        if (ejection) axis.y=-std::abs(axis.y)*1.2f;
        const Vec tail=axis*(size*(2+progress*(11+float(noise&7))));
        const Vec tip=center+tail+Vec{0,-progress*progress*9,0};
        const Vec start=tip-axis*(size*(2+3*remaining));
        scene.stroke(start,tip,progress<.26f ? ivory : progress<.61f ? gold : ember);
    }
}

constexpr Vec splash_axes[]={{1,0,0},{.71f,0,.71f},{0,0,1},{-.71f,0,.71f},
                             {-1,0,0},{-.71f,0,-.71f},{0,0,-1},{.71f,0,-.71f}};

void foam_ring(Scene& scene,Vec surface,float radius,Color color,int opacity,unsigned seed) {
    if (opacity<2) return;
    // Uneven segments and varying brightness leave a broken rim around a splash.
    Vec last=surface+splash_axes[7]*(radius*.92f);
    for (unsigned i=0;i<std::size(splash_axes);++i) {
        const auto grain=hash(seed,i);
        const Vec next=surface+splash_axes[i]*(radius*(.87f+float(grain&15)*.016f));
        if ((grain&48)!=0) {
            const float gap=float((grain>>6)&7)*.045f;
            const Vec arc=next-last;
            scene.stroke(last+arc*gap,last+arc*(.83f-gap*.62f),color,1,
                         opacity*(4+int((grain>>9)&3))/8);
        }
        last=next;
    }
}

void surface_burst(Scene& scene,const Effect& fx,float progress,bool water) {
    const float power=fx.strength;
    const float fade=1-progress;
    const Vec base=fx.pos+Vec{0,.7f,0};
    const Vec drift=fx.motion*(progress*(water ? 3.f : 10.f));
    const Vec center=base+drift;
    const bool large=power>1.6f;
    const int opacity=int(30*fade);
    if (water) {
        const float reach=(2.f+23.f*progress)*power;
        foam_ring(scene,base,reach,rgb(0xc9fffa),opacity,fx.seed);
        if (large && progress>.17f)
            foam_ring(scene,base,reach*.64f,rgb(0x86cdd9),opacity/2,fx.seed+8);
        const float lift=std::max(0.f,1.f-progress*1.6f)*progress*31.f*power;
        puff(scene,center+Vec{0,lift*.52f,0},(1.8f+progress*6.f)*power,rgb(0x94d7e5),opacity);
        if (progress<.63f)
            puff(scene,center+Vec{0,lift+1,0},(1.1f+progress*3.f)*power,rgb(0xeaffef),opacity);
    } else {
        const int stain=int(15*fade);
        const float mark=(1.6f+std::min(.3f,progress)*6.f)*power;
        const Vec fore=fx.motion*mark, across{-fore.z,0,fore.x};
        if (stain>2 && norm2(fore)>0.1f)
            scene.quad(base+fore,base+across*.69f,base-fore,base-across*.69f,rgb(0x493d33),stain);
        puff(scene,center+Vec{0,1.2f+progress*15.f*power,0},(2.9f+progress*8.f)*power,
             rgb(0xd2b18a),opacity*7/8);
        if (progress>.15f)
            puff(scene,center+Vec{-1.f,progress*9,2.f},(1.2f+progress*3.f)*power,
                 rgb(0x8a795c),opacity/2);
        if (progress<.21f) {
            puff(scene,base+Vec{0,1.4f,0},2.7f*power,ember,opacity);
            puff(scene,base+Vec{0,1.8f,0},1.2f*power,ivory,opacity);
        }
    }
    if (progress>.71f) return;
    const int count=large ? 8 : 5;
    for (int i=0;i<count;++i) {
        const auto grain=hash(fx.seed,i);
        const Vec axis=splash_axes[(i+fx.seed)%std::size(splash_axes)];
        const float distance=(5.f+float(grain&15))*progress*power;
        const float lift=power*progress*(24.f+float((grain>>4)&15)-progress*48.f);
        if (lift<0) continue;
        const Vec tip=center+axis*distance+Vec{0,.8f+lift,0};
        const Vec tail=tip-axis*(1.1f+power)-Vec{0,1.f,0};
        const Color color=water ? (i&1 ? ivory : rgb(0xabe9ec))
                                : progress<.22f ? gold : rgb(0xa38258);
        scene.stroke(tail,tip,color,1,opacity);
    }
}

void flash_muzzle(Scene& scene,const Effect& fx,float progress) {
    const Vec length=fx.motion*((3.4f+progress*2)*fx.strength);
    const Vec across=scene.camera.axes.right*fx.strength*(1-progress)*1.5f;
    scene.triangle(fx.pos-across,fx.pos+across,fx.pos+length,gold);
    puff(scene,fx.pos+length*.32f,fx.strength*1.4f,ivory,int((1-progress)*30));
}

bool is_fragment(const Effect& fx) {
    return fx.kind==EffectKind::wing_piece || fx.kind==EffectKind::canopy_piece;
}

Vec fragment_position(const Effect& fx) {
    return fx.pos+fx.motion*fx.age+Vec{0,-22*fx.age*fx.age,0};
}

void fragment(Scene& scene,const Effect& fx) {
    const float spin=float(fx.seed&31)*.21f;
    const auto axes=attitude(spin+fx.age*5.7f,fx.age*7.8f,fx.age*3.8f);
    const Vec p=fragment_position(fx);
    const bool canopy=fx.kind==EffectKind::canopy_piece;
    const float width=canopy ? 2.4f : 3.55f;
    const Vec a=p-axes.right*width-axes.front*2.9f;
    const Vec b=p+axes.right*width-axes.front*2.1f;
    const Vec c=p+axes.right*(width*.54f)+axes.front*3.1f+axes.up*.62f;
    const Vec d=p-axes.right*(width*.77f)+axes.front*1.1f+axes.up*.47f;
    scene.quad(a,b,c,d,canopy ? rgb(0x92d8d9) : rgb(0x504744));
    scene.triangle(a,d,p+axes.up*.71f,canopy ? ivory : rgb(0xb78159));
    scene.stroke(b,c,canopy ? rgb(0xd9f7e9) : ember);
}

bool effect_visible(const Scene& scene,const Effect& fx) {
    const auto local=scene.camera.local(is_fragment(fx) ? fragment_position(fx) : fx.pos);
    const float reach=fx.kind==EffectKind::muzzle ? 7.f : 44.f*std::max(1.f,fx.strength);
    if (local.z+reach<Scene::near || local.z>1250.f) return false;
    if (local.z<=Scene::near+reach) return true;
    const auto p=scene.camera.project(local);
    const float margin=scene.camera.scale_at(local.z-reach)*reach+3;
    return p.x>=-margin && p.x<=scene.out.width+margin &&
           p.y>=-margin && p.y<=scene.out.height+margin;
}

void sea_spray(Scene& scene,const World& world,const Effect& fx,float progress) {
    const Vec side=fx.motion,back{side.z,0,-side.x};
    const Vec center=fx.pos+back*(2+progress*8);
    const float power=fx.strength*(1-progress);
    const int ripple=int(27*power);
    if (ripple<1) return;
    const float reach=3.f+progress*(14.f+fx.strength*13.f);
    const float trail=5.f+progress*8.f;
    const auto follows_surface=[&](Vec& p) {
        const auto surface=world.surface_at(p.x,p.z);
        if (!surface.water() || std::abs(surface.height()-fx.pos.y)>2.5f) return false;
        p.y=surface.height()+.12f;
        return true;
    };
    for (float sign:{-1.f,1.f}) {
        const Vec lateral=side*sign;
        Vec inner=center+lateral*(reach*.28f);
        Vec edge=center+lateral*reach+back*trail;
        const Vec lip=lateral*(1.1f+fx.strength);
        if (!follows_surface(inner) || !follows_surface(edge)) continue;
        // A compressed dark trough and a bright rim sit on the water plane.
        scene.quad(inner,edge,edge-lip-back*3.f,inner-lip-back*2.f,rgb(0x104c73),ripple);
        scene.quad(inner,inner+lip,edge+lip,edge,rgb(0xb8e8e8),std::min(26,ripple+2));
        if (power>.32f && progress<.73f) {
            const float pattern=float((hash(fx.seed,unsigned(sign>0))>>12)&7)/7.f;
            const Vec droplets=inner+(edge-inner)*(.64f+pattern*.26f)+Vec{0,.8f+progress*3.3f,0};
            puff(scene,droplets,1.f+fx.strength,rgb(0xe9fff3),int(28*power));
        }
    }
}

void wakes(Scene& scene,const Game& game) {
    for (int i=1;i<game.wake_count;++i) {
        const auto& older=game.wake[(game.wake_next+game.wake.size()-i-1)%game.wake.size()];
        const auto& newer=game.wake[(game.wake_next+game.wake.size()-i)%game.wake.size()];
        const int opacity=int(std::min(older.vapor,newer.vapor)*24*(game.wake_count-i)/game.wake_count);
        if (opacity<2) continue;
        scene.stroke(older.left,newer.left,rgb(0xddeef0),1,opacity);
        scene.stroke(older.right,newer.right,rgb(0xddeef0),1,opacity);
    }
}

} // namespace

void render_flight_effects(Scene& scene,const Game& game,bool show_trails) {
    if (show_trails) wakes(scene,game);
    for (unsigned i=0;i<game.effects.size();++i) {
        const auto& fx=game.effects[(game.effect_next+i)%game.effects.size()];
        if (fx.age>=fx.duration || !effect_visible(scene,fx)) continue;
        const float progress=fx.age/fx.duration;
        switch (fx.kind) {
        case EffectKind::spray:
            sea_spray(scene,game.world,fx,progress);
            break;
        case EffectKind::water_hit:
        case EffectKind::land_hit:
            surface_burst(scene,fx,progress,fx.kind==EffectKind::water_hit);
            break;
        case EffectKind::muzzle:
            flash_muzzle(scene,fx,progress);
            break;
        case EffectKind::wing_piece:
        case EffectKind::canopy_piece:
            fragment(scene,fx);
            break;
        case EffectKind::burning: {
            const Vec p=fx.pos+fx.motion*fx.age;
            const float size=(2.2f+progress*9)*fx.strength;
            const int fade=int(24*(1-progress));
            puff(scene,p,size,rgb(0x38444b),fade);
            puff(scene,p+Vec{-.7f,2,1.6f},size*.76f,rgb(0x596367),fade);
            if (progress<.29f) {
                const float flicker=1+float(hash(fx.seed,31)&3)*.22f;
                puff(scene,p-Vec{0,1.4f,0},size*flicker,ember,int(25*(1-progress*2)));
                puff(scene,p-Vec{0,1.75f,0},flicker,ivory,int(27*(1-progress*3)));
            }
            break;
        }
        case EffectKind::smoke:
        case EffectKind::missile_smoke: {
            const bool dark=fx.kind==EffectKind::smoke;
            const Vec center=fx.pos+fx.motion*fx.age;
            const float spread=(dark ? 3+8*progress : 1.25f+4.5f*progress)*fx.strength;
            const int alpha=int((dark ? 19 : 18)*(1-progress)*fx.strength);
            puff(scene,center,spread,dark ? smoke_blue : rgb(0xb0cbce),alpha);
            if (dark || progress<.40f)
                puff(scene,center+Vec{0,1,0},spread*.59f,dark ? rgb(0x73858a) : ivory,alpha);
            break;
        }
        default:
            blast(scene,fx,progress);
            break;
        }
    }
    for (const auto& shot:game.shots) {
        if (shot.life<=0) continue;
        const Vec tail=shot.pos-shot.velocity*(shot.missile ? .047f : .028f);
        const auto color=shot.allied ? rgb(0x6de8c6) : shot.hostile ? rgb(0xf06c57) : shot.missile ? ember : gold;
        scene.stroke(tail,shot.pos,color,2);
        const float core=shot.missile ? .58f : .46f;
        scene.stroke(tail+(shot.pos-tail)*core,shot.pos,ivory);
        if (shot.missile) puff(scene,shot.pos,2.f,ivory,27);
    }
    if (game.phase==Phase::ejection && game.phase_time<.50f) {
        const float thrust=1-game.phase_time*2;
        const Vec seat=game.pilot-Vec{0,3.5f,0};
        const Vec jet=seat-Vec{0,4+8*thrust,0}-game.drift*.075f;
        scene.stroke(seat,jet,ember,2);
        scene.stroke(seat,seat+(jet-seat)*.62f,ivory,2);
        puff(scene,seat-Vec{0,1.3f,0},1.35f,gold,int(thrust*26));
    }
}

void render_speed_streaks(Canvas& canvas,const Game& game,View view) {
    if (view!=View::chase || game.exhaust_heat<.22f) return;
    constexpr Vec lanes[]={{-.83f,-.36f,0},{.76f,-.42f,0},{-.45f,-.67f,0},{.40f,-.73f,0},
                           {-.91f,.10f,0},{.86f,.20f,0},{-.78f,.52f,0},{.75f,.54f,0},
                           {-.37f,.82f,0},{.51f,.75f,0},{-.64f,-.50f,0},{.60f,-.53f,0}};
    const float cx=canvas.width*.5f,cy=canvas.height*.46f;
    for (unsigned i=0;i<std::size(lanes);++i) {
        const Vec lane=lanes[i];
        const float phase=game.elapsed*(1.30f+.07f*(i%3))+i*.17f;
        const float travel=phase-std::floor(phase);
        const float distance=.57f+travel*.48f;
        const int alpha=int(game.exhaust_heat*(14+travel*14)*(travel>.88f ? (1-travel)/.12f : 1));
        const float length=.042f+travel*.092f;
        const float dx=lane.x*cx,dy=lane.y*cy;
        const int steps=std::clamp(int(length*std::max(std::abs(dx),std::abs(dy)))+1,3,20);
        for (int j=0;j<steps;++j) {
            const float ray=distance+length*j/(steps-1);
            const int x=int(cx+dx*ray),y=int(cy+dy*ray);
            const int strength=alpha*(j+2)/(steps+1);
            canvas.tint({x,y,j*3>steps*2 ? 2 : 1,1},wake_white,strength);
        }
    }
}

} // namespace sky
