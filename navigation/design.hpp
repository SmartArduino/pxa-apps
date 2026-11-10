// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_canvas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace navui {
// Match native Canvas's truncating DP conversion. Ceil the logical viewport
// first, so fractional density covers the final physical row and column.
inline int logical_extent(unsigned pixels,unsigned density){return int((std::uint64_t(pixels)*65536+std::max(1u,density)-1)/std::max(1u,density));}
inline int pixel_span(int dp,unsigned density){return int(std::int64_t(dp)*density/65536);}

struct Rect {
    int x=0,y=0,w=0,h=0;
    bool contains(int px,int py)const{return w>0&&h>0&&px>=x&&py>=y&&px<x+w&&py<y+h;}
    bool operator==(const Rect&)const=default;
};
enum Action : unsigned {None,Search,Settings,Plan,Demo,Locate,Plus,Minus,Overview,Start,Book,Back,Swap,
    SelectStart,SelectEnd,Walk,Bike,Drive,Save,MapSettings,LocationSettings,WifiSource,ForwardSource,
    Previous,Next,Submit,Clear,CitySettings,ClearField,RetryMap,OpenCompass,CalibrateCompass,SaveCompassCalibration,CancelCompassCalibration,RetryCompass,FirstMagnetometer,SecondMagnetometer,Result0=100};
struct Hit {Rect box;unsigned action;};
struct Layout {
    int w=320,h=240,left=0,top=0,right=0,bottom=0;
    bool compact=true;
    Rect search,settings,deck,primary,secondary,recenter,zoom_in,zoom_out,book,back;
    int map_h=150;
    void update(int width,int height,Rect safe,bool navigating) {
        w=width;h=height;left=safe.x;top=safe.y;right=w-safe.x-safe.w;bottom=h-safe.y-safe.h;
        compact=safe.h<400;
        const int m=compact?10:16, content=safe.w-2*m;
        const int deck_h=navigating?(compact?76:116):(compact?90:150);
        deck={left+m,h-bottom-m-deck_h,content,deck_h};
        map_h=deck.y-6;
        search={left+m,top+m,content-48,compact?40:48};
        settings={w-right-m-40,top+m+(compact?0:4),40,40};
        back={left+m,top+m,36,36};
        const int primary_w=compact?std::clamp(content-118,82,110):150;
        primary={deck.x+deck.w-primary_w-12,deck.y+deck.h-48,primary_w,36};
        secondary={deck.x+12,primary.y,compact?58:100,36};
        recenter={w-right-m-38,map_h-66,38,38};
        zoom_in={compact?w-right-m-76:recenter.x,compact?map_h-64:top+88,38,34};
        zoom_out={compact?zoom_in.x+38:zoom_in.x,compact?zoom_in.y:zoom_in.y+34,38,34};
        book={deck.x+12,primary.y,compact||safe.w<360?44:74,36};
    }
};
inline std::uint32_t mix(std::uint32_t a,std::uint32_t b,unsigned amount) {
    std::uint32_t out=0;for(unsigned shift:{8u,16u,24u})out|=(((a>>shift&255)*(255-amount)+(b>>shift&255)*amount)/255)<<shift;
    return out|255;
}
struct Palette {
    std::uint32_t background=0xfff8eaff,surface=0xffffffff,primary=0x20866bff,
        ink=0x28433fff,muted=0x6a7e77ff,line=0xdfe9e2ff,on_primary=0xffffffff,danger=0xc05b50ff;
    bool dark=false;
};
struct Painter {
    std::array<std::uint8_t,32768> data{};
    std::array<std::uint8_t,32768> previous{};
    std::size_t previous_size=0;
    std::array<std::uint8_t,4096> packet{};
    pxa_canvas_frame_t frame{};
    std::array<Hit,48> hits{};
    unsigned hit_count=0;
    std::uint32_t generation=0,density=65536;
    int width=320,height=240;
    void begin(int w,int h,std::uint32_t d){width=w;height=h;density=d;hit_count=0;pxa_canvas_begin(&frame,data.data(),data.size());}
    void rect(Rect r,std::uint32_t c,int radius=0){if(r.w>0&&r.h>0)pxa_canvas_rect_rgba(&frame,r.x,r.y,r.w,r.h,c,radius,0,0);}
    void stroke(Rect r,std::uint32_t c,int radius=0,int line=1){if(r.w>0&&r.h>0)pxa_canvas_rect_rgba(&frame,r.x,r.y,r.w,r.h,0,radius,line,c);}
    void circle(int x,int y,int radius,std::uint32_t c){pxa_canvas_ellipse_rgba(&frame,x-radius,y-radius,2*radius,2*radius,c,0,0);}
    void ring(int x,int y,int radius,std::uint32_t c,int line=2){pxa_canvas_ellipse_rgba(&frame,x-radius,y-radius,2*radius,2*radius,0,line,c);}
    void line(int x,int y,int xx,int yy,std::uint32_t c,int n=1){pxa_canvas_line_rgba(&frame,x,y,xx,yy,c,std::max(1,n));}
    void clip(Rect r){pxa_canvas_clip_push(&frame,r.x,r.y,std::max(1,r.w),std::max(1,r.h));}
    void unclip(){pxa_canvas_clip_pop(&frame);}
    void image(Rect r,std::uint64_t handle){if(handle)pxa_canvas_image_handle(&frame,r.x,r.y,r.w,r.h,255,1,handle);}
    static std::size_t next(std::string_view s,std::size_t at){unsigned c=(unsigned char)s[at];return std::min(s.size(),at+(c<128?1:c<224?2:c<240?3:4));}
    static int advance(std::string_view s,std::size_t at,int size){return ((unsigned char)s[at]<128?size*6/10:size);}
    static std::string shorten(std::string_view s,int width,int size){
        std::size_t at=0,last=0;int used=0;
        while(at<s.size()&&s[at]!='\n'){int n=advance(s,at,size);if(used+n>width){return std::string(s.substr(0,last))+"…";}used+=n;at=next(s,at);if(used+size<=width)last=at;}
        return std::string(s.substr(0,at));
    }
    void text(Rect r,std::string_view value,std::uint32_t color,int size=14,int align=0){
        if(r.w<=0||r.h<=0)return;
        std::size_t at=0,last=0;int used=0;bool clipped=false;
        while(at<value.size()&&value[at]!='\n'){
            const int n=advance(value,at,size);
            if(used+n>r.w){at=last;clipped=true;break;}
            used+=n;at=next(value,at);if(used+size<=r.w)last=at;
        }
        auto s=value.substr(0,at);
        if(!pxa_canvas_primitive(&frame,12,std::uint16_t(24+s.size()+(clipped?3:0))))return;
        pxa_put_u32(&frame,r.x);pxa_put_u32(&frame,r.y);pxa_put_u32(&frame,r.w);pxa_put_u32(&frame,r.h);
        pxa_put_u32(&frame,color);pxa_put_u16(&frame,std::clamp<std::uint32_t>(std::uint64_t(size)*density/65536,8,128));
        pxa_put_u8(&frame,align);pxa_put_u8(&frame,0);pxa_put_bytes(&frame,reinterpret_cast<const std::uint8_t*>(s.data()),s.size());
        if(clipped)pxa_put_bytes(&frame,reinterpret_cast<const std::uint8_t*>("…"),3);
        ++frame.records;
    }
    int paragraph(Rect r,std::string_view value,std::uint32_t c,int size=13,int leading=20){
        std::size_t at=0;int y=r.y;
        while(at<value.size()&&y+leading<=r.y+r.h){
            const auto start=at;int used=0;
            while(at<value.size()&&value[at]!='\n'){int n=advance(value,at,size);if(used+n>r.w)break;used+=n;at=next(value,at);}
            if(at==start&&value[at]!='\n')at=next(value,at);
            text({r.x,y,r.w,leading},value.substr(start,at-start),c,size);y+=leading;
            if(at<value.size()&&value[at]=='\n')++at;
        }
        return y;
    }
    void hit(Rect r,unsigned action){if(hit_count<hits.size()&&r.w>0&&r.h>0)hits[hit_count++]={r,action};}
    unsigned find(int x,int y)const{for(unsigned i=hit_count;i--;)if(hits[i].box.contains(x,y))return hits[i].action;return None;}
    void card(Rect r,const Palette& p,int radius=16){
        rect({r.x,r.y+3,r.w,r.h},p.dark?0x00000030:0x253c5814,radius);
        rect(r,p.surface,radius);
    }
    void button(Rect r,std::string_view title,const Palette& p,unsigned action,bool primary=true,bool enabled=true){
        const auto color=primary?p.primary:mix(p.surface,p.primary,20);
        rect(r,enabled?color:mix(p.surface,p.muted,35),r.h/2);
        text({r.x+8,r.y+(r.h-18)/2,r.w-16,20},title,primary?p.on_primary:p.primary,14,1);
        if(enabled)hit(r,action);
    }
    void icon(std::string_view name,int x,int y,std::uint32_t c,int s=20){
        const int k=std::max(1,s/10),a=s/2;
        if(name=="search"){ring(x-2,y-2,a-4,c,k);line(x+3,y+3,x+a,y+a,c,k);}
        else if(name=="back"){
            // LV_SYMBOL_LEFT is also used by the system Settings header.
            pxa_canvas_text_box_role(&frame,x-a,y-a-3,s,s+6,c,
                PXA_CANVAS_FONT_ICON,PXA_CANVAS_ALIGN_CENTER,
                PXA_CANVAS_TEXT_ALIGN_MIDDLE,"\xef\x81\x93",3);
        }
        else if(name=="plus"||name=="minus"){line(x-a+3,y,x+a-3,y,c,k);if(name=="plus")line(x,y-a+3,x,y+a-3,c,k);}
        else if(name=="locate"){ring(x,y,a-4,c,k);circle(x,y,2,c);line(x-a,y,x-a+4,y,c,k);line(x+a-4,y,x+a,y,c,k);line(x,y-a,x,y-a+4,c,k);line(x,y+a-4,x,y+a,c,k);}
        else if(name=="compass"){ring(x,y,a-2,c,k);line(x-3,y+4,x+3,y-5,c,2);circle(x+3,y-5,2,c);}
        else if(name=="settings"){ring(x,y,4,c,k);for(int i=0;i<8;++i){double t=i*3.14159265/4;line(x+int(std::cos(t)*7),y+int(std::sin(t)*7),x+int(std::cos(t)*10),y+int(std::sin(t)*10),c,3);}}
        else if(name=="route"){circle(x-6,y-6,3,c);circle(x+6,y+6,3,c);line(x-6,y-3,x-6,y+5,c,k);line(x-6,y+5,x+6,y-5,c,k);}
        else if(name=="book"){stroke({x-a+2,y-a+2,s-4,s-4},c,3,k);line(x-5,y-3,x+5,y-3,c,k);line(x-5,y+3,x+3,y+3,c,k);}
        else if(name=="swap"){line(x-4,y-8,x-4,y+8,c,k);line(x-4,y+8,x-8,y+4,c,k);line(x+4,y+8,x+4,y-8,c,k);line(x+4,y-8,x+8,y-4,c,k);}
        else if(name=="pin"){ring(x,y-3,5,c,k);line(x-4,y+1,x,y+8,c,k);line(x,y+8,x+4,y+1,c,k);}
        else if(name=="arrow"||name=="left"||name=="right"){
            int sign=name=="left"?-1:1;
            if(name=="arrow"){line(x,y+a-2,x,y-a+2,c,3);line(x,y-a+2,x-6,y-2,c,3);line(x,y-a+2,x+6,y-2,c,3);}
            else {line(x-sign*5,y+a-2,x-sign*5,y-5,c,3);line(x-sign*5,y-5,x+sign*8,y-5,c,3);line(x+sign*8,y-5,x+sign*3,y-10,c,3);line(x+sign*8,y-5,x+sign*3,y,c,3);}
        }
    }
    void icon_button(Rect r,std::string_view name,const Palette& p,unsigned action,bool enabled=true){card(r,p,r.h/2);icon(name,r.x+r.w/2,r.y+r.h/2,enabled?p.ink:p.muted);if(enabled)hit(r,action);}
    bool present(std::uint32_t node,const pxa_canvas_dirty_rect_t* dirty=nullptr,std::uint8_t count=0,bool force=false){
        if(frame.failed)return false;
        bool equal=!force&&previous_size==frame.length;
        // Keep this bounded comparison compact on Xtensa. The general WASI
        // memcmp pulls in a large implementation for this small frame cache.
        const volatile std::uint8_t* prior=previous.data();
        for(std::size_t i=0;equal&&i<frame.length;++i)equal=prior[i]==frame.data[i];
        if(equal)return true;
        std::uint8_t mounted=1;
        if(!pxa_canvas_present_regions(node,&frame,&generation,&mounted,0,nullptr,0,packet.data(),packet.size(),dirty,count))return false;
        std::memcpy(previous.data(),frame.data,frame.length);previous_size=frame.length;return true;
    }
};
} // namespace navui
