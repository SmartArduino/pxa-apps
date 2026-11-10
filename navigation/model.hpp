// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace navigation {
constexpr std::size_t MaxBody = 196608, MaxPoints = 4096, MaxSteps = 128;
struct Point { double lon = 0, lat = 0; };
struct Place { std::string name, address; Point point; };
struct Step { std::string instruction, road; unsigned distance = 0; std::size_t end = 0; };
struct Route {
    std::vector<Point> points;
    std::vector<Step> steps;
    unsigned distance = 0, seconds = 0;
};
enum class Mode { walking, cycling, driving };
inline const char* mode_name(Mode mode) {
    return mode == Mode::walking ? "步行" : mode == Mode::cycling ? "骑行" : "驾车";
}
inline std::string text(media::json::Value v) {
    if (!v || v.n > MaxBody) return {};
    std::string out(v.n, '\0');
    bool truncated = false;
    if (!v.string(out.data(), out.size(), &truncated) || truncated) return {};
    out.resize(std::strlen(out.c_str()));
    return out;
}
inline std::string unsigned_text(std::uint64_t n) {
    char digits[20];unsigned size=0;
    do{digits[size++]=char('0'+n%10);n/=10;}while(n);
    std::string out;out.reserve(size);while(size)out+=digits[--size];return out;
}
inline std::optional<double> finite_number(std::string_view s) {
    if(s.empty()||s.size()>64)return {};
    std::size_t i=0;const bool negative=s[0]=='-';
    if(s[0]=='-'||s[0]=='+')++i;
    double value=0;unsigned digits=0,fraction=0;
    while(i<s.size()&&s[i]>='0'&&s[i]<='9'){value=value*10+(s[i++]-'0');++digits;}
    if(!digits)return {};
    if(i<s.size()&&s[i]=='.'){
        ++i;
        while(i<s.size()&&s[i]>='0'&&s[i]<='9'){value=value*10+(s[i++]-'0');++fraction;}
        if(!fraction)return {};
    }
    int exponent=0;
    if(i<s.size()&&(s[i]=='e'||s[i]=='E')){
        ++i;bool minus=false;if(i<s.size()&&(s[i]=='+'||s[i]=='-'))minus=s[i++]=='-';
        unsigned count=0;
        while(i<s.size()&&s[i]>='0'&&s[i]<='9'){
            exponent=exponent*10+(s[i++]-'0');if(exponent>400)return {};++count;
        }
        if(!count)return {};if(minus)exponent=-exponent;
    }
    if(i!=s.size())return {};
    exponent-=int(fraction);
    while(exponent>0){value*=10;--exponent;}
    while(exponent<0){value/=10;++exponent;}
    if(negative)value=-value;
    return std::isfinite(value)?std::optional(value):std::nullopt;
}
inline std::optional<double> decimal(std::string_view s) {
    if (s.empty() || s.size() > 24) return {};
    // Reject whitespace, exponents, NaN, hex and partial numbers.
    std::size_t i = s[0] == '-' || s[0] == '+' ? 1 : 0, digits = 0;
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) ++digits;
    if (!digits) return {};
    if (i < s.size() && s[i] == '.') {
        const auto start = ++i;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) {}
        if (i == start || i - start > 6) return {};
    }
    if (i != s.size()) return {};
    return finite_number(s);
}
inline std::optional<Point> coordinate(std::string_view s) {
    const auto split = s.find(',');
    if (split == s.npos) return {};
    const auto lon = decimal(s.substr(0, split)), lat = decimal(s.substr(split + 1));
    if (!lon || !lat || *lon < -180 || *lon > 180 || *lat < -85 || *lat > 85) return {};
    return Point{*lon, *lat};
}
inline std::string coordinate(Point p) {
    if(!std::isfinite(p.lon)||!std::isfinite(p.lat)||std::abs(p.lon)>1000||std::abs(p.lat)>1000)return {};
    std::string out;
    auto append=[&](double v){
        const auto scaled=std::int64_t(v*1000000+(v<0?-.5:.5));
        if(scaled<0)out+='-';
        const auto magnitude=std::uint64_t(scaled<0?-scaled:scaled);
        out+=unsigned_text(magnitude/1000000);out+='.';
        for(unsigned divisor=100000;divisor;divisor/=10)out+=char('0'+magnitude/divisor%10);
    };
    append(p.lon);out+=',';append(p.lat);
    return out;
}
inline std::string encode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            out += char(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
inline bool valid_key(std::string_view key) {
    return key.size() == 32 && std::all_of(key.begin(), key.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}
inline media::json::Value document(std::string_view body, std::string& error) {
    if (body.empty() || body.size() > MaxBody) { error = "响应为空或超过大小限制"; return {}; }
    const auto* p = reinterpret_cast<const uint8_t*>(body.data());
    media::json::Reader reader{p, p + body.size()};
    if (!reader.skip()) { error = "地图服务返回了无效 JSON"; return {}; }
    reader.ws();
    if (reader.p != reader.end) { error = "地图服务响应不完整"; return {}; }
    media::json::Value root{p, body.size()};
    if (text(root.get("status")) != "1") {
        const auto code = text(root.get("infocode"));
        if (code == "10001" || code == "10009" || code == "10002") error = "Key 无效或权限不匹配，请检查 Web 服务 Key";
        else if (code == "10003" || code == "10004" || code == "10044") error = "高德配额或调用频率受限，请稍后重试";
        else if (code == "20803") error = "路线超出当前出行方式支持的范围，请缩短距离或切换出行方式";
        else error = "高德请求失败：" + text(root.get("info")) + " (" + code + ")";
        return {};
    }
    return root;
}
inline std::optional<unsigned> integer(media::json::Value value) {
    auto s = text(value);
    if (s.empty() && value && *value.p != '"') s.assign(reinterpret_cast<const char*>(value.p), value.n);
    if (s.empty()) return {};
    unsigned result = 0;
    for (char c : s) {
        if (c < '0' || c > '9' || result > (UINT_MAX - unsigned(c - '0')) / 10) return {};
        result = result * 10 + unsigned(c - '0');
    }
    return result;
}
inline bool places(std::string_view body, std::vector<Place>& output, std::string& error) {
    output.clear();
    auto root = document(body, error);
    if (!root) return false;
    media::json::Array list(root.get("pois")); media::json::Value item;
    while (list.next(item) && output.size() < 5) {
        auto p = coordinate(text(item.get("location")));
        auto name = text(item.get("name"));
        if (p && !name.empty()) output.push_back({std::move(name), text(item.get("cityname")) + text(item.get("adname")) + " " + text(item.get("address")), *p});
    }
    if (output.empty()) { error = "没有找到地点，请补充城市或更换关键词"; return false; }
    return true;
}
inline bool route(std::string_view body, Route& output, std::string& error) {
    output = {};
    const auto root = document(body, error);
    if (!root) return false;
    media::json::Array paths(root.get("route").get("paths")); media::json::Value path;
    if (!paths.next(path)) { error = "没有找到可用路线"; return false; }
    Route result;
    const auto distance = integer(path.get("distance"));
    if (!distance) { error = "路线缺少距离信息"; return false; }
    result.distance = *distance;
    result.seconds = integer(path.get("cost").get("duration")).value_or(integer(path.get("duration")).value_or(0));
    media::json::Array steps(path.get("steps")); media::json::Value step;
    while (steps.next(step)) {
        if (result.steps.size() == MaxSteps) { error = "路线过长，请分段规划（最多128段）"; return false; }
        auto instruction = text(step.get("instruction"));
        auto road = text(step.get("road_name"));
        if(road.empty())road=text(step.get("road"));
        if (instruction.empty()) instruction = text(step.get("navi").get("action")) + " " + road;
        result.steps.push_back({std::move(instruction), road, integer(step.get("step_distance")).value_or(integer(step.get("distance")).value_or(0))});
        auto line = text(step.get("polyline"));
        std::string_view remaining(line);
        if (remaining.empty()) { error = "路线缺少坐标，请检查接口权限"; return false; }
        while (!remaining.empty()) {
            const auto delimiter = remaining.find(';');
            auto p = coordinate(remaining.substr(0, delimiter));
            if (!p) { error = "路线包含无效坐标"; return false; }
            if (result.points.empty() || p->lon != result.points.back().lon || p->lat != result.points.back().lat)
                result.points.push_back(*p);
            if (result.points.size() > MaxPoints) { error = "路线过长，请分段规划（最多4096点）"; return false; }
            if (delimiter == remaining.npos) break;
            remaining.remove_prefix(delimiter + 1);
        }
        result.steps.back().end = result.points.size() - 1;
    }
    if (result.points.size() < 2 || result.steps.empty()) { error = "路线数据不完整"; return false; }
    output = std::move(result);
    return true;
}
inline std::string distance(unsigned meters) {
    if(meters<1000)return unsigned_text(meters)+" 米";
    const auto tenths=(std::uint64_t(meters)+50)/100;
    return unsigned_text(tenths/10)+"."+char('0'+tenths%10)+" 公里";
}
inline std::string summary(const Route& r, Mode mode) {
    return std::string(mode_name(mode)) + " · " + distance(r.distance) +
        (r.seconds ? " · 约 " + unsigned_text((r.seconds + 59) / 60) + " 分钟" : "");
}
inline Point project(Point p) {
    constexpr double pi = 3.14159265358979323846;
    const double lat = std::clamp(p.lat, -85.0, 85.0) * pi / 180;
    return {(p.lon + 180) / 360, (1 - std::asinh(std::tan(lat)) / pi) / 2};
}
inline Point unproject(Point p) {
    constexpr double pi = 3.14159265358979323846;
    return {p.lon * 360 - 180, std::atan(std::sinh(pi * (1 - 2 * p.lat))) * 180 / pi};
}
struct Viewport { Point center{}; int zoom = 13, width = 280, height = 200; };
inline Viewport fit(const Route& r, int width, int height, int padding_x=48, int padding_y=64) {
    Point lo{1, 1}, hi{0, 0};
    for (auto p : r.points) {
        p = project(p);
        lo.lon = std::min(lo.lon, p.lon); lo.lat = std::min(lo.lat, p.lat);
        hi.lon = std::max(hi.lon, p.lon); hi.lat = std::max(hi.lat, p.lat);
    }
    Viewport v{unproject({(lo.lon + hi.lon) / 2, (lo.lat + hi.lat) / 2}), 17, width, height};
    const int inner_w=std::max(1,width-padding_x),inner_h=std::max(1,height-padding_y);
    while (v.zoom > 1 && ((hi.lon - lo.lon) * (256 << v.zoom) > inner_w ||
            (hi.lat - lo.lat) * (256 << v.zoom) > inner_h)) --v.zoom;
    // The API uses the rounded centre; render using precisely that centre too.
    v.center = *coordinate(coordinate(v.center));
    return v;
}
inline Point pixel(Point p, const Viewport& view) {
    p = project(p); const auto c = project(view.center); const double scale = 256 << view.zoom;
    return {(p.lon - c.lon) * scale + view.width / 2.0, (p.lat - c.lat) * scale + view.height / 2.0};
}
// Fit the full route inside the unobstructed part of a map. This keeps turn
// banners, floating controls and the provider's attribution out of the path.
inline Viewport fit_insets(const Route& route,int width,int height,int left,int top,int right,int bottom) {
    auto view=fit(route,std::max(1,width-left-right),std::max(1,height-top-bottom),0,0);
    auto center=project(view.center);const double scale=256<<view.zoom;
    center.lon+=(right-left)/2.0/scale;center.lat+=(bottom-top)/2.0/scale;
    center.lon=std::clamp(center.lon,0.0,1.0);center.lat=std::clamp(center.lat,.000001,.999999);
    view.center=unproject(center);view.center.lat=std::clamp(view.center.lat,-85.0,85.0);view.center=*coordinate(coordinate(view.center));
    view.width=width;view.height=height;return view;
}
// Bound the display list by simplifying in screen pixels, preserving endpoints.
// The full route remains intact for distance, turn selection and map matching.
inline std::vector<Point> drawing_points(const Route& r, const Viewport& view) {
    std::vector<Point> points;for(auto p:r.points)points.push_back(pixel(p,view));
    if(points.size()<3)return points;
    std::vector<Point> result;
    for(double tolerance=1;;tolerance*=2) {
        std::vector<bool> keep(points.size());keep.front()=keep.back()=true;
        std::vector<std::pair<std::size_t,std::size_t>> pending{{0,points.size()-1}};
        while(!pending.empty()) {
            auto [a,b]=pending.back();pending.pop_back();double best=tolerance*tolerance;std::size_t index=0;
            const double dx=points[b].lon-points[a].lon,dy=points[b].lat-points[a].lat;
            for(std::size_t i=a+1;i<b;++i) {
                const double x=points[i].lon-points[a].lon,y=points[i].lat-points[a].lat;
                const double t=dx*dx+dy*dy>0?std::clamp((x*dx+y*dy)/(dx*dx+dy*dy),0.0,1.0):0;
                const double d=(x-t*dx)*(x-t*dx)+(y-t*dy)*(y-t*dy);
                if(d>best){best=d;index=i;}
            }
            if(index){keep[index]=true;pending.push_back({a,index});pending.push_back({index,b});}
        }
        result.clear();for(std::size_t i=0;i<points.size();++i)if(keep[i])result.push_back(points[i]);
        if(result.size()<=110)return result;
    }
}
struct RouteDrawing {
    std::vector<Point> vertices;
    Point center{},projected_center{};
    int zoom=0,width=0,height=0;
    unsigned revision=0;
    bool valid=false;
    const std::vector<Point>& get(const Route& route,unsigned version,const Viewport& view) {
        if(!valid||revision!=version||zoom!=view.zoom){
            vertices=drawing_points(route,view);center=view.center;projected_center=project(center);
            width=view.width;height=view.height;zoom=view.zoom;revision=version;valid=true;
        }
        return vertices;
    }
    Point offset(const Viewport& view)const {
        Point p{(view.width-width)/2.0,(view.height-height)/2.0};
        if(view.center.lon!=center.lon||view.center.lat!=center.lat){
            auto c=project(view.center);const double scale=256<<zoom;
            p.lon+=(projected_center.lon-c.lon)*scale;p.lat+=(projected_center.lat-c.lat)*scale;
        }
        return p;
    }
};
inline std::string search_url(std::string_view origin, std::string_view key, std::string_view query, std::string_view city) {
    return std::string(origin) + "/v3/place/text?key=" + encode(key) + "&keywords=" + encode(query) +
        (city.empty()?std::string{}:"&city=" + encode(city) + "&citylimit=true") +
        "&offset=5&page=1&extensions=base&output=JSON";
}
inline std::string route_url(std::string_view origin, std::string_view key, Point start, Point end, Mode mode) {
    const auto type = mode == Mode::walking ? "walking" : mode == Mode::cycling ? "bicycling" : "driving";
    return std::string(origin) + "/v5/direction/" + type + "?key=" + encode(key) + "&origin=" + coordinate(start) +
        "&destination=" + coordinate(end) + "&show_fields=cost,navi,polyline&output=JSON";
}
inline std::string map_url(std::string_view origin, std::string_view key, const Viewport& v) {
    return std::string(origin) + "/v3/staticmap?key=" + encode(key) + "&location=" + coordinate(v.center) +
        "&zoom=" + unsigned_text(v.zoom) + "&size=" + unsigned_text(v.width) + "*" + unsigned_text(v.height) + "&scale=1";
}

inline double meters(Point a, Point b) {
    constexpr double rad = 0.017453292519943295;
    const double x = (b.lon - a.lon) * rad * std::cos((a.lat + b.lat) * rad / 2);
    const double y = (b.lat - a.lat) * rad;
    return 6371000 * std::hypot(x, y);
}
struct Fix {
    Point point;
    double accuracy = 0;
    std::uint64_t time_ms = 0;
    bool simulated = false;
};
struct Guidance {
    bool usable = false, off_route = false, arrived = false;
    std::size_t segment = 0, step = 0;
    double along = 0, remaining = 0, next_turn = 0, deviation = 0;
    Point snapped;
};
// Local route matching is independent of location provider. IP/unknown-accuracy
// results never advance guidance; repeated or stale samples cannot advance it.
class Tracker {
    std::vector<double> lengths;
    std::uint64_t last_time = 0;
    unsigned outside = 0, arrival = 0;
    Guidance current;
public:
    void reset(const Route& r) {
        lengths.assign(r.points.size(), 0);
        for (std::size_t i = 1; i < r.points.size(); ++i)
            lengths[i] = lengths[i - 1] + meters(r.points[i - 1], r.points[i]);
        last_time = 0; outside = arrival = 0; current = {}; current.remaining = total();
    }
    double total() const { return lengths.empty() ? 0 : lengths.back(); }
    Guidance update(const Route& r, const Fix& f, std::uint64_t now) {
        auto bad = current; bad.usable = false; bad.arrived = false;
        if (r.points.size() < 2 || r.steps.empty() || lengths.size() != r.points.size() || !f.time_ms ||
            f.time_ms <= last_time || now < f.time_ms || now - f.time_ms > 30000 ||
            !std::isfinite(f.accuracy) || f.accuracy <= 0 || f.accuracy > 80) return bad;
        const double elapsed = last_time ? (f.time_ms - last_time) / 1000.0 : 0;
        const double ahead = last_time ? std::max(150.0, elapsed * 55 + f.accuracy) : total();
        double best = 1e30, along = 0; std::size_t segment = 0; Point snapped{};
        constexpr double rad = 0.017453292519943295;
        const double mx = 111194.9266 * std::cos(f.point.lat * rad), my = 111194.9266;
        for (std::size_t i = 0; i + 1 < r.points.size(); ++i) {
            if (last_time && (lengths[i + 1] < current.along - 60 || lengths[i] > current.along + ahead)) continue;
            const auto a = r.points[i], b = r.points[i + 1];
            const double x = (b.lon - a.lon) * mx, y = (b.lat - a.lat) * my;
            const double px = (f.point.lon - a.lon) * mx, py = (f.point.lat - a.lat) * my;
            const double t = x*x + y*y > 0 ? std::clamp((px*x + py*y)/(x*x+y*y), 0.0, 1.0) : 0;
            const double d = std::hypot(px - t*x, py - t*y);
            if (d < best) { best = d; segment = i; along = lengths[i] + t*(lengths[i+1]-lengths[i]); snapped = {a.lon+t*(b.lon-a.lon), a.lat+t*(b.lat-a.lat)}; }
        }
        last_time = f.time_ms;
        const bool off = best > std::max(40.0, f.accuracy * 1.5);
        outside = off ? outside + 1 : 0;
        current.off_route = outside >= 3; current.usable = !off || current.off_route; current.deviation = best;
        if (!off) {
            current.along = along; current.segment = segment; current.snapped = snapped;
            current.remaining = std::max(0.0, total() - along);
            current.step = 0;
            while (current.step + 1 < r.steps.size() && lengths[r.steps[current.step].end] < along + 8) ++current.step;
            current.next_turn = std::max(0.0, lengths[r.steps[current.step].end] - along);
        }
        arrival = !off && current.remaining < 25 && meters(f.point, r.points.back()) < 25 && f.accuracy <= 25 ? arrival + 1 : 0;
        current.arrived = arrival >= 2;
        return current;
    }
};
inline Route demo_route() {
    // Explicitly illustrative coordinates, not a route obtained from AMap.
    Route r;
    r.points = {{114.0500,22.5400},{114.0510,22.5400},{114.0520,22.5400},
                {114.0520,22.5410},{114.0520,22.5420},{114.0530,22.5420},{114.0540,22.5420}};
    r.steps = {{"沿示例道路直行，前方左转", "示例道路 A", 205, 2},
               {"左转后继续直行，前方右转", "示例道路 B", 222, 4},
               {"右转，前往示例终点", "示例道路 C", 205, 6}};
    r.distance = 632; r.seconds = 480;
    return r;
}
} // namespace navigation
