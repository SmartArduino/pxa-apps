// SPDX-License-Identifier: MIT
#pragma once
#include "combat.hpp"
#include "art.hpp"
namespace ash {
struct Bounds {float left,top,right,bottom;};
struct Camera {float zoom=1,x=0,y=0;};
struct ActorPose {art::Tile tile;float x,y;};
inline float fitScale(int width,int height){
    float fit=std::min(width/320.f,height/240.f);
    return fit>=1?std::floor(fit):fit;
}
inline constexpr ActorPose actorPose(const Fighter& p,int frame,float ground=193){
    art::Tile tile;art::Offset offset;int half=fighterHalf,baseline=fighterBaseline;
    if(frame>=40){tile=art::walk[p.character][frame-40];offset=art::walk_offsets[p.character][frame-40];}
    else if(frame>=32){tile=art::extras[p.character][frame-32];offset=art::extras_offsets[p.character][frame-32];half=64;baseline=125;}
    else{tile=art::fighters[p.character][0][frame];offset=art::fighter_offsets[p.character][frame];}
    float x=p.face<0?p.x+half-offset.x-tile.w:p.x-half+offset.x;
    return {tile,x,ground-baseline-p.y+offset.y};
}
inline constexpr Bounds poseBounds(const ActorPose& p){return {p.x,p.y,p.x+p.tile.w,p.y+p.tile.h};}
inline constexpr Bounds unite(Bounds a,Bounds b){return {std::min(a.left,b.left),std::min(a.top,b.top),std::max(a.right,b.right),std::max(a.bottom,b.bottom)};}
inline constexpr auto neutralFraming=[](){
    std::array<Bounds,characterCount> result{};
    for(int c=0;c<characterCount;++c){Fighter p;p.character=c;p.x=0;
        Bounds bounds{10000,10000,-10000,-10000};
        for(int face:{-1,1})for(int frame=0;frame<48;++frame)if(frame<4||frame>=40){p.face=face;bounds=unite(bounds,poseBounds(actorPose(p,frame)));}
        result[c]=bounds;
    }
    return result;
}();
inline Bounds framingBounds(const Fighter& p,int frame){
    auto b=neutralFraming[p.character];b.left+=p.x;b.right+=p.x;b.top-=p.y;b.bottom-=p.y;
    return unite(b,poseBounds(actorPose(p,frame)));
}
inline Camera fitCamera(Bounds actors,Bounds view){
    Camera c;c.zoom=std::min({1.f,(view.right-view.left)/(actors.right-actors.left),(view.bottom-view.top)/(actors.bottom-actors.top)});
    if(c.zoom<1)c.zoom*=.9999f;
    c.x=std::min(std::max(0.f,view.left-actors.left*c.zoom),view.right-actors.right*c.zoom);
    c.y=std::min(std::max(0.f,view.top-actors.top*c.zoom),view.bottom-actors.bottom*c.zoom);
    return c;
}
}
