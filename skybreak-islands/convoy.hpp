#pragma once
#include "world.hpp"

namespace sky {

enum class Sortie { patrol, escort, fleet };

struct Convoy {
    static constexpr unsigned max_hull=8;
    static constexpr float duration=37.f;
    static constexpr float speed=21.f;
    static constexpr float resupply_radius=66.f;
    static constexpr Vec route[]={{2062,0,1938},{2058,0,2208},{2034,0,2420},{2080,0,2708}};
    static constexpr float arrival[]={0,13,23,duration};
    struct Trail { Vec stern,side; };

    Vec pos=route[0];
    float yaw=-.015f,progress=0;
    unsigned hull=max_hull;
    float damage_flash=0,smoke_delay=0,resupply_flash=0;
    bool resupplied=false;
    std::array<Trail,15> trail{};
    unsigned next_trail=0,trail_count=0;
    float trail_delay=0;

    static Vec route_position(float seconds) {
        const unsigned leg=seconds<arrival[1] ? 0 : seconds<arrival[2] ? 1 : 2;
        const float phase=saturate((seconds-arrival[leg])/(arrival[leg+1]-arrival[leg]));
        return route[leg]+(route[leg+1]-route[leg])*phase;
    }
    void move(float elapsed,float dt) {
        const Vec previous=pos;
        pos=route_position(elapsed);
        const Vec motion=pos-previous;
        if (norm2(motion)>.001f) {
            const float course=std::atan2(motion.x,motion.z);
            yaw+=std::remainder(course-yaw,2*pi)*std::min(1.f,dt*3.f);
        }
        progress=saturate(elapsed/duration);
        damage_flash=std::max(0.f,damage_flash-dt);
        resupply_flash=std::max(0.f,resupply_flash-dt);
        trail_delay-=dt;
        if (trail_delay>0) return;
        const auto axes=attitude(yaw);
        trail[next_trail]={pos-axes.front*25.f,axes.right};
        next_trail=(next_trail+1)%trail.size();
        trail_count=std::min(trail_count+1,unsigned(trail.size()));
        trail_delay=.18f;
    }
    bool can_resupply(Vec craft) const {
        if (resupplied || craft.y<18 || craft.y>79) return false;
        const Vec offset=craft-pos;
        return offset.x*offset.x+offset.z*offset.z<resupply_radius*resupply_radius;
    }
    bool intercepts(Vec from,Vec to) const {
        if (std::min(from.y,to.y)>9 || std::max(from.y,to.y)<0) return false;
        if (std::max(from.x,to.x)<pos.x-42 || std::min(from.x,to.x)>pos.x+42 ||
            std::max(from.z,to.z)<pos.z-42 || std::min(from.z,to.z)>pos.z+42) return false;
        const auto axes=attitude(yaw);
        const auto local=[&](Vec p) {
            const Vec d=p-pos;
            return Vec{dot(d,axes.right),d.y,dot(d,axes.front)};
        };
        const Vec a=local(from),travel=local(to)-a;
        float first=0,last=1;
        const auto clip=[&](float start,float delta,float low,float high) {
            if (std::abs(delta)<.0001f) return start>=low && start<=high;
            const float u=(low-start)/delta,v=(high-start)/delta;
            first=std::max(first,std::min(u,v));
            last=std::min(last,std::max(u,v));
            return first<=last;
        };
        return clip(a.x,travel.x,-8,8) && clip(a.y,travel.y,.05f,8.5f) &&
               clip(a.z,travel.z,-27,27);
    }
};

struct Scene;
void draw_convoy(Scene& scene,const Convoy& convoy,float time,int shade);

} // namespace sky
