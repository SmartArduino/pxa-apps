// SPDX-License-Identifier: MIT
#include "../location.hpp"
#include <cassert>
#include <iostream>
using namespace navigation;
int main() {
    assert(coordinate("114.123456,22.540000"));
    for(auto value:{"NaN,22","114,inf","114,22x","181,22","114,91","1,2,3","0x10,2","1.0000001,2"})assert(!coordinate(value));
    assert(encode("深圳 &") == "%E6%B7%B1%E5%9C%B3%20%26");
    assert(coordinate(Point{114.05,22.54})=="114.050000,22.540000");
    assert(coordinate(Point{-180,85})=="-180.000000,85.000000");
    assert(coordinate(Point{-.000001,1.9999997})=="-0.000001,2.000000");
    assert(coordinate(Point{-.0000004,0})=="0.000000,0.000000");
    assert(unsigned_text(UINT64_MAX)=="18446744073709551615");
    assert(distance(999)=="999 米"&&distance(1000)=="1.0 公里"&&distance(1150)=="1.2 公里");
    assert(finite_number("1.1405e2")&&std::abs(*finite_number("1.1405e2")-114.05)<1e-10);
    assert(finite_number("22.54123456789")&&std::abs(*finite_number("22.54123456789")-22.54123456789)<1e-10);
    for(auto value:{"NaN","inf","0x10","1e","1e+","1e999"," 1","1 ","--1","1.",".1"})assert(!finite_number(value));
    std::string error;Route r;
    const auto global_search=search_url("https://maps.test","test","市民中心","");
    assert(global_search.find("citylimit=true")==std::string::npos);
    assert(global_search.find("&city=")==std::string::npos);
    const auto local_search=search_url("https://maps.test","test","市民中心","深圳");
    assert(local_search.find("&city="+encode("深圳")+"&citylimit=true")!=std::string::npos);
    std::vector<Place> found;
    assert(places(R"({"status":"1","pois":[{"name":"市民中心","location":"114.059614,22.543673","cityname":"深圳市","adname":"福田区","address":"福中三路"}]})",found,error));
    assert(found.front().address.find("深圳市")!=std::string::npos);
    assert(!document(R"({"status":"0","infocode":"20803","info":"OVER_DIRECTION_RANGE"})",error));
    assert(error.find("超出")!=std::string::npos&&error.find("出行方式")!=std::string::npos);
    assert(route(R"({"status":"1","route":{"paths":[{"distance":"629","duration":"229","steps":[{"instruction":"向南骑行后左转","road_name":"鹏程二路","step_distance":143,"polyline":"114.054896,22.543884;114.054896,22.542884"}]}]}})",r,error));
    assert(r.seconds==229&&r.steps[0].road=="鹏程二路"&&r.steps[0].distance==143);
    assert(route(R"({"status":"1","route":{"paths":[{"distance":"591","cost":{"duration":"157"},"steps":[{"instruction":"向南行驶后左转","road_name":"鹏程二路","step_distance":"137","polyline":"114.054896,22.543884;114.054896,22.542884"}]}]}})",r,error));
    assert(r.seconds==157&&r.steps[0].road=="鹏程二路"&&r.steps[0].distance==137);
    const auto response=R"({"status":"1","route":{"paths":[{"distance":"333","cost":{"duration":"240"},"steps":[{"instruction":"直行","road":"A","distance":"111","polyline":"114.050000,22.540000;114.051000,22.540000"},{"instruction":"左转","road":"B","distance":"222","polyline":"114.051000,22.540000;114.051000,22.541000;114.051000,22.542000"}]}]}})";
    assert(route(response,r,error));assert(r.points.size()==4&&r.steps[0].end==1&&r.steps[1].end==3&&r.seconds==240);
    assert(!document(R"({"status":"0","infocode":"10001","info":"INVALID_USER_KEY"})",error));assert(error.find("Key")!=error.npos);
    assert(!document("{} trailing",error));
    assert(!route(R"({"status":"1","route":{"paths":[]}})",r,error));assert(r.points.empty());
    r=demo_route();Tracker tracker;tracker.reset(r);
    auto g=tracker.update(r,{r.points.front(),5,1000,false},1000);assert(g.usable&&!g.off_route&&g.step==0);
    g=tracker.update(r,{r.points[2],5,2000,false},2000);assert(g.step==1&&g.along>150);
    const auto along=g.along;
    g=tracker.update(r,{r.points.back(),5000,3000,false},3000);assert(!g.usable&&g.along==along&&!g.arrived);
    g=tracker.update(r,{r.points.back(),5,1000,false},3000);assert(!g.usable);
    g=tracker.update(r,{r.points.back(),5,3000,false},40000);assert(!g.usable);
    Point off=r.points[2];off.lat-=0.005;
    for(unsigned i=0;i<3;++i)g=tracker.update(r,{off,10,4000+i*1000,false},4000+i*1000);
    assert(g.off_route&&g.along==along&&!g.arrived);
    g=tracker.update(r,{r.points[3],5,7000,false},7000);assert(!g.off_route);
    // Walk forward with realistic elapsed times, then confirm arrival twice.
    for(unsigned i=4;i<r.points.size();++i)g=tracker.update(r,{r.points[i],5,10000+i*10000,false},10000+i*10000);
    g=tracker.update(r,{r.points.back(),5,80000,false},80000);assert(g.arrived&&g.remaining<1);
    Tracker far_tracker;far_tracker.reset(r);Point far=r.points.front();far.lat+=1;
    for(unsigned i=0;i<3;++i){auto f=far_tracker.update(r,{far,5,1000+i*1000,false},1000+i*1000);assert(f.remaining>600&&!f.arrived);assert(f.usable==(i==2)&&f.off_route==(i==2));}
    Route empty_steps=r;empty_steps.steps.clear();far_tracker.reset(empty_steps);
    assert(!far_tracker.update(empty_steps,{far,5,5000,false},5000).usable);
    auto v=fit(r,280,160);auto center=pixel(v.center,v);assert(std::abs(center.lon-140)<0.01&&std::abs(center.lat-80)<0.01);
    for(auto p:r.points){auto q=pixel(p,v);assert(q.lon>=20&&q.lon<=260&&q.lat>=25&&q.lat<=135);}
    Route dense;
    for(unsigned i=0;i<MaxPoints;++i)dense.points.push_back({114.0+i*0.000001,22.54+(i%2)*0.001});
    auto simplified=drawing_points(dense,v);assert(simplified.size()<=110&&simplified.size()>=2);
    auto first=pixel(dense.points.front(),v),last=pixel(dense.points.back(),v);
    assert(simplified.front().lon==first.lon&&simplified.back().lat==last.lat);
    RouteDrawing drawing;const auto& cached=drawing.get(r,1,v);assert(!cached.empty());
    const auto* cached_data=cached.data();auto moved=v;moved.center.lon+=.001;moved.width+=20;moved.height+=10;
    const auto& reused=drawing.get(r,1,moved);assert(reused.data()==cached_data);
    const auto delta=drawing.offset(moved);const auto target=pixel(r.points.front(),moved);
    assert(std::abs(reused.front().lon+delta.lon-target.lon)<.001&&std::abs(reused.front().lat+delta.lat-target.lat)<.001);
    ++moved.zoom;const auto& zoomed=drawing.get(r,1,moved);const auto zoomed_target=pixel(r.points.back(),moved);
    assert(std::abs(zoomed.back().lon-zoomed_target.lon)<.001&&std::abs(zoomed.back().lat-zoomed_target.lat)<.001);
    auto changed=r;changed.points.front().lon+=.0001;const auto& rebuilt=drawing.get(changed,2,moved);
    assert(std::abs(rebuilt.front().lon-pixel(changed.points.front(),moved).lon)<.001);
    auto loc=location(R"({"location":{"lat":22.54123456789,"lng":114.05123456789},"accuracy":30})",1000,true,error);
    assert(loc&&loc->wgs84&&loc->fix.accuracy==30);
    assert(location(R"({"lat":22.54,"lng":114.05,"accuracy":5,"age_ms":100,"datum":"gcj02"})",1000,false,error));
    assert(!location(R"({"lat":22.54,"lng":114.05,"accuracy":5,"datum":"gcj02"})",1000,false,error));
    assert(!location(R"({"lat":22.54,"lng":114.05,"accuracy":5,"age_ms":31000,"datum":"gcj02"})",40000,false,error));
    std::vector<Place> p;assert(places(R"({"status":"1","pois":[{"name":"A","location":"114,22","address":[]}]})",p,error));assert(p.size()==1);
    auto url=route_url("https://restapi.amap.com","k",r.points.front(),r.points.back(),Mode::walking);
    assert(url.find("/v5/direction/walking")!=url.npos&&url.find("show_fields=cost,navi,polyline")!=url.npos);
    std::cout<<"Navigation model: PASS (coordinates, responses, matching, stale/coarse fixes, deviation, arrival, projection)\n";
}
