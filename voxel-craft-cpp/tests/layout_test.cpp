#include "../voxel_ui.hpp"
#include <pxa/core.hpp>
#include <cassert>
#include <cstdio>
int main(){
    for(auto size: {std::pair{296u,240u},std::pair{412u,412u},std::pair{240u,296u},std::pair{800u,480u},std::pair{128u,128u}})
    for(unsigned density:{65536u,98304u,131072u}){
        pxa::ui::DisplayMetrics m;m.width=size.first;m.height=size.second;m.safe={8,10,8,10};m.shape=1;m.corners={58,58,58,58};m.density_q16=density;
        auto c=voxel::ControlLayout::make(m);
        auto check=[&](voxel::Rect r){auto [l,right]=c.horizontal(r.y,r.h);assert(r.x>=l&&r.x+r.w<=right);assert(r.y>=int(m.safe.top)&&r.y+r.h<=int(m.height-m.safe.bottom));
            auto x=int(std::int64_t(r.cx())*65536/density),y=int(std::int64_t(r.cy())*65536/density);
            assert(r.contains(pxa::ui::canvas_to_surface_coordinate(x,m),pxa::ui::canvas_to_surface_coordinate(y,m)));
        };
        check(c.menu);check(c.hotbar);check(c.bag);for(auto r:c.actions)check(r);
        for(int i=0;i<5;++i)for(int j=i+1;j<5;++j){auto a=c.actions[i],b=c.actions[j];assert(!a.contains(b.cx(),b.cy()));}
    }
    std::array<std::byte,144> env{};std::size_t at=0;
    for(unsigned tag=1;tag<=12;++tag){unsigned n=tag<=5||tag==11?4:tag==6?16:tag==7||tag==8?1:tag==12?20:8;pxa::wire::put16(env.data()+at,tag);pxa::wire::put16(env.data()+at+2,n);at+=4;
        if(tag==1)pxa::wire::put32(env.data()+at,1);if(tag==2)pxa::wire::put32(env.data()+at,296);if(tag==3)pxa::wire::put32(env.data()+at,240);if(tag==4||tag==5)pxa::wire::put32(env.data()+at,65536);if(tag==12){pxa::wire::put32(env.data()+at,1);for(int i=0;i<4;++i)pxa::wire::put32(env.data()+at+4+i*4,58);}at+=n;
    }
    auto good=pxa::ui::decode_display_metrics(std::span{env}.first(at));assert(good&&good->corners[0]==58);
    for(std::size_t n=0;n<at-24;++n)assert(!pxa::ui::decode_display_metrics(std::span{env}.first(n)));
    auto bad=env;pxa::wire::put32(bad.data()+4,0);assert(!pxa::ui::decode_display_metrics(std::span{bad}.first(at)));
    pxa::ui::DisplayMetrics high;high.density_q16=3*65536;
    assert(pxa::ui::canvas_to_surface_coordinate(-7,high)==-21);
    assert(pxa::ui::canvas_to_surface_coordinate(INT32_MAX,high)==INT32_MAX);
    assert(pxa::ui::canvas_to_surface_coordinate(INT32_MIN,high)==INT32_MIN);
    std::puts("Layout: landscape/portrait/round-corner/DPI control bounds and display protocol validation OK");
}
