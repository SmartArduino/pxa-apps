// SPDX-License-Identifier: MIT
#include "../ui_layout.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>

int main(){
    unsigned cases=0;
    for(auto display:{ash::RenderExtent{320,240},{854,480},{480,480},{640,480},{1280,720}}){
        auto surface=ash::pixelSurface(display.width,display.height);
        assert(surface.width>=320&&surface.height>=240);
        assert(std::abs(float(surface.width)/surface.height-float(display.width)/display.height)<.002f);++cases;
    }
    for(auto viewport: {ash::UiRect{0,0,320,240},ash::UiRect{-53,0,427,240},
                       ash::UiRect{-160,0,640,240},ash::UiRect{0,-40,320,320}})
        for(auto source: {ash::UiRect{0,0,431,256},ash::UiRect{0,0,455,256}}){
            auto cover=ash::coverImage(source.w,source.h,viewport);
            assert(cover.x<=viewport.x+.001f&&cover.y<=viewport.y+.001f);
            assert(cover.x+source.w*cover.scale>=viewport.x+viewport.w-.001f);
            assert(cover.y+source.h*cover.scale>=viewport.y+viewport.h-.001f);
            assert(std::abs((cover.x+source.w*cover.scale/2)-(viewport.x+viewport.w/2))<.001f);
            // One shared scale for both axes is required even when cropping.
            assert(std::abs((source.w*cover.scale)/(source.h*cover.scale)-source.w/source.h)<.001f);
            ++cases;
        }
    for(int lift=6;lift<=84;++lift){
        auto p=ash::practiceLayout(lift);
        assert(p.rowHeight>=20&&p.rowStep>=p.rowHeight);
        assert(p.rowTop+2*p.rowStep+p.rowHeight<=p.resetY);
        assert(p.resetY+p.resetHeight<=p.returnY);
        if(p.showStats){assert(p.resetY+p.resetHeight<=p.statsY);assert(p.statsY+12<p.returnY);}
        ++cases;
    }
    auto table=ash::coverImage(455,256,{0,0,320,240},.525f);
    for(float face:{.2f,.855f}){
        float center=table.x+face*455*table.scale;
        assert(center>12&&center<308); // Keep both outside fighters' faces in a 4:3 crop.
    }
    std::printf("%u layout cases passed: full-bleed proportional art and reserved-bottom panel separation.\n",cases);
}
