// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
namespace navigation {
struct ImagePlacement { double x=0,y=0,width=0,height=0; };
inline bool same_view(const Viewport& a,const Viewport& b){return a.center.lon==b.center.lon&&a.center.lat==b.center.lat&&a.zoom==b.zoom&&a.width==b.width&&a.height==b.height;}
// Preserve the geographic registration of an old bitmap while a new view is
// downloading. Large zoom jumps show an empty map until fresh pixels arrive.
inline std::optional<ImagePlacement> image_placement(const Viewport& source,const Viewport& target,int width,int height){
    if(source.width<=0||source.height<=0||target.width<=0||target.height<=0||width<=0||height<=0||std::abs(source.zoom-target.zoom)>4)return {};
    const auto center=pixel(source.center,target);const double scale=std::ldexp(1.0,target.zoom-source.zoom);
    const double sx=double(width)/target.width,sy=double(height)/target.height;
    return ImagePlacement{(center.lon-source.width*scale/2)*sx,(center.lat-source.height*scale/2)*sy,source.width*scale*sx,source.height*scale*sy};
}
inline std::string bookmark(const Viewport& v){return coordinate(v.center)+";"+unsigned_text(v.zoom);}
inline bool restore_bookmark(std::string_view text,Viewport& view){
    const auto at=text.find(';');if(at==text.npos)return false;const auto point=coordinate(text.substr(0,at));
    auto zoom=text.substr(at+1);if(!point||zoom.empty()||zoom.size()>2)return false;
    unsigned z=0;for(char c:zoom){if(c<'0'||c>'9')return false;z=z*10+unsigned(c-'0');}if(z<1||z>17)return false;
    view.center=*point;view.zoom=z;return true;
}
}
