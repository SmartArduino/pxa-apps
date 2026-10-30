// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <stdint.h>
namespace milky {
struct Rect {
    int x=0,y=0,w=0,h=0;
    bool contains(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}
    int right()const{return x+w;}int bottom()const{return y+h;}
};
struct Screen {
    int width=288,height=224;
    Rect safe{0,0,288,224};
};
// Preserve aspect ratio within a fixed pixel budget, independent of DPI.
inline Screen fit_screen(int native_w,int native_h,const int *insets=nullptr,int corner=0,bool round=false){
    native_w=std::max(1,native_w);native_h=std::max(1,native_h);
    const int maximum=std::max(native_w,native_h);int side=std::min(maximum,480);
    Screen s;
    do{s.width=std::max(1,native_w*side/maximum);s.height=std::max(1,native_h*side/maximum);--side;}
    while(s.width*s.height>480*320&&side>0);
    int l=0,t=0,r=0,b=0;
    if(insets){t=std::max(0,insets[0])*s.height/native_h;r=std::max(0,insets[1])*s.width/native_w;b=std::max(0,insets[2])*s.height/native_h;l=std::max(0,insets[3])*s.width/native_w;}
    const int radius=corner*293/1000;
    l=std::max(l,radius*s.width/native_w);r=std::max(r,radius*s.width/native_w);
    t=std::max(t,radius*s.height/native_h);b=std::max(b,radius*s.height/native_h);
    if(round){const int diameter=std::min(s.width,s.height),square=diameter*707/1000;
        l=std::max(l,(s.width-square)/2);r=std::max(r,(s.width-square)/2);
        t=std::max(t,(s.height-square)/2);b=std::max(b,(s.height-square)/2);}
    l=std::min(l,s.width/3);r=std::min(r,s.width/3);t=std::min(t,s.height/3);b=std::min(b,s.height/3);
    s.safe={l,t,s.width-l-r,s.height-t-b};return s;
}
inline Screen screen;
inline Rect bounds(){return screen.safe;}
inline int gap(){return std::clamp(std::min(bounds().w,bounds().h)/32,8,14);}
inline int body_size(){return std::clamp(std::min(screen.width,screen.height)/22,11,16);}
inline int text_height(int scale=1){const int body=body_size();return scale==1?body+2:scale==2?body+10:body+17;}
inline int touch_height(){return std::clamp(std::min(bounds().w,bounds().h)/8,30,46);}
inline Rect menu_button(){auto b=bounds();int n=touch_height();return {b.right()-gap()-n,b.y+gap()/2,n,n};}
inline int header_bottom(){return bounds().y+gap()/2+touch_height()+gap()/2;}
inline Rect back_button(){auto b=bounds();return {b.x+gap(),b.y+gap()/2,touch_height(),touch_height()};}
inline Rect pause_panel(){auto b=bounds();int w=std::min(b.w-2*gap(),320),h=4*(touch_height()+6)+text_height(2)+3*gap();h=std::min(h,b.h-2*gap());return {b.x+(b.w-w)/2,b.y+(b.h-h)/2,w,h};}
inline Rect pause_row(unsigned i){auto p=pause_panel();int top=p.y+text_height(2)+gap()*2,step=(p.bottom()-gap()-top)/4;return {p.x+gap(),top+int(i)*step,p.w-2*gap(),step-4};}
struct SettingsLayout{Rect effects,music,preview,back;};
inline SettingsLayout settings_layout(){auto b=bounds();const int w=std::min(360,b.w-2*gap()),x=b.x+(b.w-w)/2,top=header_bottom()+gap();
    const int step=std::min(touch_height()+16,std::max(20,(b.bottom()-gap()-top)/4));
    return {{x,top,w,step-6},{x,top+step,w,step-6},{x,top+2*step,w,step-6},{x,top+3*step,w,step-6}};}
struct BoardLayout{Rect board,score,best,actions[4];bool stacked,compact;};
inline BoardLayout board_layout(){auto b=bounds();const int g=gap(),top=header_bottom()+g/2,available=b.bottom()-g-top;BoardLayout out{};
    out.stacked=b.w<280||b.w*100<b.h*118;
    out.compact=out.stacked&&b.h<300;
    if(!out.stacked){const int side=std::min(available,b.w*67/100);out.board={b.x+g,top,side,side};int x=out.board.right()+g,w=b.right()-g-x;
        const int metrics=std::max(32,(side-2*(touch_height()+6)-6)/2);out.score={x,top,w,metrics-4};out.best={x,top+metrics,w,metrics-4};
        int y=top+metrics*2+4,step=(side-metrics*2-4)/2,cw=(w-5)/2;
        for(int i=0;i<4;++i)out.actions[i]={x+(i%2)*(cw+5),y+(i/2)*step,cw,step-5};
    }else{int metric_h=std::clamp(b.h/10,28,44),metric_w=(b.w-3*g)/2;out.score={b.x+g,top,metric_w,metric_h};out.best={b.x+2*g+metric_w,top,metric_w,metric_h};
        const int row=touch_height(),toolbar=b.bottom()-g-row;
        int board_top=top+metric_h+g;
        if(out.compact){int x=b.x+g+53,w=(menu_button().x-g-x-4)/2;out.score={x,b.y+g/2,w,row};out.best={x+w+4,b.y+g/2,w,row};board_top=top;}
        int side=std::min(b.w-2*g,toolbar-g/2-board_top);out.board={b.x+(b.w-side)/2,board_top+(toolbar-g/2-board_top-side)/2,side,side};
        int w=(b.w-2*g-3*4)/4;for(int i=0;i<4;++i)out.actions[i]={b.x+g+i*(w+4),toolbar,w,row};
    }return out;}
struct HomeLayout{Rect hero,copy,difficulty,start,secondary[2];};
inline HomeLayout home_layout(bool miner){auto b=bounds();int g=gap(),top=header_bottom()+g,button=touch_height();HomeLayout out{};
    int footer=b.bottom()-g-button;out.secondary[0]={b.x+g,footer,(b.w-3*g)/2,button};out.secondary[1]={out.secondary[0].right()+g,footer,(b.w-3*g)/2,button};
    out.start={b.x+g,footer-g-button,b.w-2*g,button};
    if(b.w>=360&&b.w*100>=b.h*125){
        out.hero={b.x+g,top,b.w*40/100,b.bottom()-g-top};
        const int x=out.hero.right()+g,w=b.right()-g-x;
        out.start={x,footer-g-button,w,button};out.secondary[0]={x,footer,(w-g)/2,button};out.secondary[1]={x+(w+g)/2,footer,(w-g)/2,button};
        int bottom=out.start.y-g;
        if(miner){out.difficulty={x,bottom-button,w,button};bottom=out.difficulty.y-g;}
        out.copy={x,top,w,std::max(24,bottom-top)};return out;
    }
    int content_bottom=out.start.y-g;
    if(miner){out.difficulty={b.x+g,content_bottom-button,b.w-2*g,button};content_bottom=out.difficulty.y-g;}
    const int content_h=std::max(20,content_bottom-top);
    if(b.w>=b.h||content_h<150){int hw=std::min(b.w/3,content_h*3/4);out.hero={b.x+g,top,hw,content_h};out.copy={out.hero.right()+g,top,b.right()-g-out.hero.right()-g,content_h};}
    else{int hh=content_h*60/100;out.hero={b.x+b.w/4,top,b.w/2,hh};out.copy={b.x+g,top+hh+g/2,b.w-2*g,content_h-hh-g/2};}
    return out;}
struct GalleryLayout{Rect content,previous,next;int columns,rows;};
inline GalleryLayout gallery_layout(){auto b=bounds();int g=gap(),top=header_bottom()+g/2;int columns=b.w>=360?4:3;int rows=b.h>=360?4:3;
    int h=touch_height();return {{b.x+g,top,b.w-2*g,b.bottom()-g-h-g-top},{b.x+g,b.bottom()-g-h,(b.w-3*g)/2,h},{b.x+2*g+(b.w-3*g)/2,b.bottom()-g-h,(b.w-3*g)/2,h},columns,rows};}
inline Rect gallery_cell(const GalleryLayout &l,int index){int g=gap()/2,w=(l.content.w-(l.columns-1)*g)/l.columns,h=(l.content.h-(l.rows-1)*g)/l.rows;return {l.content.x+(index%l.columns)*(w+g),l.content.y+(index/l.columns)*(h+g),w,h};}
}
