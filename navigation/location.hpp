// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
namespace navigation {
inline std::optional<double> number(media::json::Value v) {
    if (!v || v.n > 64) return {};
    auto s = text(v);
    if (s.empty()) s.assign(reinterpret_cast<const char*>(v.p),v.n);
    return finite_number(s);
}
struct Location { Fix fix; bool wgs84 = true; std::string sample_id; };
inline std::optional<Location> location(std::string_view body, std::uint64_t observed_ms,
    bool beacon, std::string& error) {
    const auto* p = reinterpret_cast<const uint8_t*>(body.data());
    media::json::Reader reader{p,p+body.size()};
    if (body.empty() || !reader.skip()) {error="定位响应无效";return {};}
    reader.ws(); if(reader.p!=reader.end){error="定位响应不完整";return {};}
    media::json::Value root{p,body.size()};
    const auto service_error=root.get("error");
    if(service_error){
        const auto kind=text(service_error);const auto code=integer(service_error.get("code"));
        error=kind=="no_coverage"||(beacon&&code&&*code==404)?"当前区域暂无免费 Wi-Fi 定位覆盖":kind=="stale_location"?"位置测量已过期，请重新获取":"定位服务暂不可用，请稍后重试";
        return {};
    }
    const auto loc=beacon?root.get("location"):root;
    const auto lat=number(loc.get("lat")), lon=number(loc.get("lng")), accuracy=number(root.get("accuracy"));
    const auto age=beacon?std::optional(0u):integer(root.get("age_ms"));
    const auto datum=beacon?std::string("wgs84"):text(root.get("datum"));
    if (!lat || !lon || !accuracy || !age || *lat < -85 || *lat > 85 || *lon < -180 || *lon > 180 ||
        *accuracy<=0 || *accuracy>1000000 || *age>30000 || observed_ms<=*age || (datum!="wgs84" && datum!="gcj02")) {
        error="定位缺少有效坐标、精度、时效或坐标系";return {};
    }
    auto id=text(root.get("sample_id"));if(id.size()>64){error="定位测量标识过长";return {};}
    return Location{{{*lon,*lat},*accuracy,observed_ms-*age,false},datum=="wgs84",std::move(id)};
}
class LocationGate {
    std::string last_id;
    std::uint64_t last_time=0,last_uncertainty=0;
public:
    void reset(){last_id.clear();last_time=last_uncertainty=0;}
    bool eligible(const Location& loc,std::uint64_t request_ms)const{
        const auto t=loc.fix.time_ms;
        if(t<=last_time||(!loc.sample_id.empty()&&loc.sample_id==last_id))return false;
        const auto jitter=std::min<std::uint64_t>(500,std::max(last_uncertainty,request_ms)+25);
        if(last_time&&loc.sample_id.empty()&&last_id.empty()&&t-last_time<=jitter)return false;
        return true;
    }
    bool accept(const Location& loc,std::uint64_t request_ms){
        if(!eligible(loc,request_ms))return false;
        last_time=loc.fix.time_ms;last_id=loc.sample_id;last_uncertainty=request_ms;return true;
    }
};

}
