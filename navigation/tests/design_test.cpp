// SPDX-License-Identifier: MIT
#include "../design.hpp"
#include "../model.hpp"
#include <cassert>
#include <iostream>
using namespace navui;
static bool overlaps(Rect a,Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
static void within(Rect r,Rect safe){assert(r.w>0&&r.h>0);assert(r.x>=safe.x&&r.y>=safe.y&&r.x+r.w<=safe.x+safe.w&&r.y+r.h<=safe.y+safe.h);}
int main(){
    // A floor-rounded DP viewport used to paint 239 pixels of a 240px panel.
    // Requesting a 240px image then silently resampled every drag frame.
    constexpr unsigned fractional_density=130*65536/160;
    assert(logical_extent(240,fractional_density)==296);
    assert(pixel_span(logical_extent(240,fractional_density),fractional_density)==240);
    for(unsigned density:{49152u,fractional_density,65536u,98304u,131072u})
        for(unsigned pixels:{240u,284u,296u,480u})
            assert(pixel_span(logical_extent(pixels,density),density)>=int(pixels));
    for(auto safe:{Rect{0,0,240,284},Rect{0,0,296,240},Rect{0,0,320,240},Rect{0,0,251,251},Rect{0,0,480,480},Rect{0,0,320,533},Rect{0,0,320,378},Rect{0,0,320,400},Rect{8,12,304,204}}){
        for(bool live:{false,true}){
            Layout l;l.update(safe.x+safe.w+safe.x,safe.y+safe.h+safe.y,safe,live);
            for(auto r:{l.search,l.settings,l.deck,l.primary,l.book,l.back})within(r,safe);
            assert(!overlaps(l.search,l.settings));assert(l.map_h<l.deck.y);
            if(!live){assert(!overlaps(l.secondary,l.primary));assert(!overlaps(l.book,l.primary));}
            assert(!overlaps(l.book,l.primary));
            Rect overview{l.book.x+l.book.w+8,l.book.y,l.compact||safe.w<360?36:60,36};
            if(!live)assert(!overlaps(overview,l.primary));
            assert(l.primary.w>=100&&l.primary.h>=32);
            auto route=navigation::demo_route();
            const int top=safe.y+(live?(l.compact?80:104):(l.compact?64:80));
            const int left=safe.x+16,right=l.w-safe.x-safe.w+(l.compact?96:70);
            auto camera=navigation::fit_insets(route,l.w,l.map_h,left,top,right,24);
            assert(camera.zoom>1);
            for(auto point:route.points){
                auto p=navigation::pixel(point,camera);
                assert(p.lon>=left-1&&p.lon<=l.w-right+1);
                assert(p.lat>=top-1&&p.lat<=l.map_h-24+1);
            }
        }
    }
    // Half-open edges keep adjacent controls from consuming one another's taps.
    Rect r{10,20,30,40};assert(r.contains(10,20)&&r.contains(39,59));assert(!r.contains(40,60));
    Painter p;p.begin(480,480,65536);
    p.hit({0,0,60,60},Search);p.hit({20,20,20,20},Settings);
    assert(p.find(25,25)==Settings&&p.find(0,0)==Search&&p.find(60,60)==None);
    // Truncation must preserve complete UTF-8 sequences at narrow control widths.
    assert(Painter::shorten("深圳地图导航",42,14)=="深圳…");
    assert(Painter::shorten("Map",40,14)=="Map");
    p.text({0,0,60,30},"路线就绪",0xffffffff,14);
    assert(!p.frame.failed);
    // Use Settings' LV_SYMBOL_LEFT through the Host's native icon font.
    p.begin(480,480,65536);
    p.icon("back",34,34,0xffffffff);
    assert(!p.frame.failed&&p.frame.records==1&&p.frame.length==31);
    assert(p.frame.data[0]==PXA_CANVAS_DRAW_TEXT_BOX);
    assert(p.frame.data[24]==PXA_CANVAS_FONT_ICON);
    assert(std::string_view(reinterpret_cast<const char*>(p.frame.data+28),3)=="\xef\x81\x93");
    // The turn banner and attribution already reserve vertical space. Applying
    // a second padding used to zoom a 240dp panel all the way out to the world.
    auto route=navigation::demo_route();auto camera=navigation::fit(route,272,59,0,0);
    assert(camera.zoom>=14);
    double lo=1e9,hi=-1e9;
    for(auto point:route.points){auto q=navigation::pixel(point,camera);lo=std::min(lo,q.lat);hi=std::max(hi,q.lat);}
    assert(hi-lo<=59);
    route.points={{179.999999,85},{180,85}};
    camera=navigation::fit_insets(route,296,148,16,80,96,24);
    assert(navigation::coordinate(navigation::coordinate(camera.center)));
    assert(camera.center.lat==85&&camera.center.lon<=180);
    std::cout<<"PASS: safe layouts, hit boundaries, UTF-8 labels, route framing\n";
}
