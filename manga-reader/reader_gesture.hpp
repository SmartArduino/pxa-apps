#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace manga {
struct TouchPair {
    struct Point {bool active=false;std::uint8_t id=0;float x=0,y=0;};
    std::array<Point,2> points{};bool mixed=false;
    unsigned count()const{return unsigned(points[0].active)+unsigned(points[1].active);}
    int find(unsigned id)const{for(unsigned i=0;i<2;++i)if(points[i].active&&points[i].id==id)return i;return -1;}
    bool update(unsigned id,unsigned phase,float x,float y){
        int i=find(id);
        if(phase==0){if(i>=0)return false;for(unsigned j=0;j<2;++j)if(!points[j].active){i=j;break;}if(i<0)return false;points[i]={true,std::uint8_t(id),x,y};if(count()==2)mixed=true;return true;}
        if(i<0)return false;points[i].x=x;points[i].y=y;if(phase>=2)points[i].active=false;return true;
    }
    Point remaining()const{return points[0].active?points[0]:points[1];}
    float distance()const{return std::hypot(points[1].x-points[0].x,points[1].y-points[0].y);}
    float center_x()const{return (points[0].x+points[1].x)*.5f;}
    float center_y()const{return (points[0].y+points[1].y)*.5f;}
    void reset(){points={};mixed=false;}
};
struct PinchAnchor {
    float distance=1,scale=1,content_x=0,content_y=0;
    void start(const TouchPair& p,float s,float pan_x,float pan_y,float view_w,float view_h,float image_w,float image_h){
        distance=std::max(1.f,p.distance());scale=s;
        content_x=(p.center_x()-std::max(0.f,(view_w-image_w*s)*.5f)+pan_x)/s;
        content_y=(p.center_y()-std::max(0.f,(view_h-image_h*s)*.5f)+pan_y)/s;
    }
    void apply(const TouchPair& p,float view_w,float view_h,float image_w,float image_h,float& s,float& pan_x,float& pan_y)const{
        auto fit_w=view_w/image_w,fit_h=view_h/image_h;
        s=std::clamp(scale*p.distance()/distance,std::min(fit_w,fit_h),fit_w*4);
        pan_x=std::clamp(content_x*s+std::max(0.f,(view_w-image_w*s)*.5f)-p.center_x(),0.f,std::max(0.f,image_w*s-view_w));
        pan_y=std::clamp(content_y*s+std::max(0.f,(view_h-image_h*s)*.5f)-p.center_y(),0.f,std::max(0.f,image_h*s-view_h));
    }
};
}
