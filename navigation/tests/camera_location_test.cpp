// SPDX-License-Identifier: MIT
#include "../camera.hpp"
#include "../location.hpp"
#include <cassert>
#include <iostream>
using namespace navigation;
int main(){
    Viewport a;a.center={114.052,22.541};a.width=240;a.height=197;a.zoom=16;
    auto p=image_placement(a,a,296,243);assert(p&&std::abs(p->x)<1e-6&&std::abs(p->y)<1e-6&&p->width==296);
    auto b=a;++b.zoom;p=image_placement(a,b,296,243);assert(p&&p->width==592&&p->height==486&&std::abs(p->x+148)<1e-6);
    b=a;b.center.lon+=.001;p=image_placement(a,b,296,243);assert(p&&p->x<0&&p->width==296);
    // Bitmap corners and route vertices must undergo the same transform.
    const auto center=project(a.center);const auto scale=double(256<<a.zoom);
    const auto corner=unproject({center.lon-a.width/2.0/scale,center.lat-a.height/2.0/scale});
    const auto target=pixel(corner,b);assert(std::abs(p->x-target.lon*296/b.width)<1e-5&&std::abs(p->y-target.lat*243/b.height)<1e-5);
    b.zoom=1;assert(!image_placement(a,b,296,243));
    const auto saved=bookmark(a);b.zoom=2;assert(restore_bookmark(saved,b)&&bookmark(b)==saved&&b.zoom==a.zoom);
    for(auto text:{"","1,2","181,0;16","1,2;0","1,2;18","1,2;NaN","1,2;1x","1,2;1;2"})assert(!restore_bookmark(text,b));
    std::string error;auto loc=location(R"({"lat":22.54,"lng":114.05,"accuracy":5,"age_ms":100,"datum":"gcj02","sample_id":"sensor:1"})",1000,false,error);
    assert(loc&&loc->sample_id=="sensor:1");LocationGate gate;assert(gate.eligible(*loc,50));assert(gate.eligible(*loc,50));assert(gate.accept(*loc,50));assert(!gate.eligible(*loc,50));
    loc->fix.time_ms+=1000;assert(!gate.accept(*loc,70));loc->sample_id="sensor:2";assert(gate.accept(*loc,70));
    loc->sample_id="sensor:3";loc->fix.time_ms-=100;assert(!gate.accept(*loc,30));
    gate.reset();loc->sample_id.clear();loc->fix.time_ms=1000;assert(gate.accept(*loc,60));
    loc->fix.time_ms=1030;assert(!gate.accept(*loc,50));loc->fix.time_ms=2000;assert(gate.accept(*loc,80));
    assert(!location(R"({"error":{"code":404}})",1000,true,error)&&error.find("覆盖")!=error.npos);
    assert(!location(R"({"error":"no_coverage"})",1000,false,error)&&error.find("覆盖")!=error.npos);
    assert(!location(R"({"error":"stale_location"})",1000,false,error)&&error.find("过期")!=error.npos);
    std::cout<<"PASS: bitmap registration, zoom preview, bookmarks, sample IDs, delayed and repeated fixes\n";
}
