#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include "world.hpp"

namespace han {
using MapPoint=std::pair<float,float>;
constexpr int roadCurveSteps=5;

constexpr size_t roadPointCapacity() {
    size_t total=std::size(roads)*2;
    for (const auto& road:roads)
        for (const auto& bend:road.bends) {
            if (!bend.lon) break;
            total+=roadCurveSteps+1;
        }
    return total;
}

inline size_t roundedRoad(const Road& road,int year,std::span<MapPoint> output) {
    std::array<MapPoint,roadBends+2> raw;
    size_t count=0;
    const auto add=[&](MapPoint point) {
        if (!count || point!=raw[count-1]) raw[count++]=point;
    };
    add(cityMapPoint(road.from,year));
    for (const auto& bend:road.bends) {
        if (!bend.lon) break;
        add({worldX(bend.lon),worldY(bend.lat)});
    }
    add(cityMapPoint(road.to,year));
    size_t written=0;
    output[written++]=raw[0];
    for (size_t i=1;i+1<count;++i) {
        const auto [x,y]=raw[i];
        const float ax=x-raw[i-1].first,ay=y-raw[i-1].second;
        const float bx=raw[i+1].first-x,by=raw[i+1].second-y;
        const float incoming=std::sqrt(ax*ax+ay*ay),outgoing=std::sqrt(bx*bx+by*by);
        // Keep nearly straight or reversing segments, and trim only locally.
        const float turn=(ax*bx+ay*by)/(incoming*outgoing);
        const float trim=std::min(4.6f,.36f*std::min(incoming,outgoing));
        if (turn>.9848f || turn<-.94f || trim<.4f) {
            output[written++]=raw[i];
            continue;
        }
        const float inX=ax*trim/incoming,inY=ay*trim/incoming;
        const float outX=bx*trim/outgoing,outY=by*trim/outgoing;
        for (int step=0;step<=roadCurveSteps;++step) {
            const float t=float(step)/roadCurveSteps,s=1-t;
            output[written++]={x-inX*s*s+outX*t*t,y-inY*s*s+outY*t*t};
        }
    }
    if (count>1) output[written++]=raw[count-1];
    return written;
}

inline MapPoint roadPosition(std::span<const MapPoint> points,std::span<const float> distance,float progress) {
    if (progress<=0 || points.size()==1) return points.front();
    if (progress>=1) return points.back();
    const float along=distance.back()*progress;
    const size_t right=std::lower_bound(distance.begin()+1,distance.end(),along)-distance.begin();
    const size_t left=right-1;
    const float fraction=(along-distance[left])/(distance[right]-distance[left]);
    return {std::lerp(points[left].first,points[right].first,fraction),
            std::lerp(points[left].second,points[right].second,fraction)};
}
}
