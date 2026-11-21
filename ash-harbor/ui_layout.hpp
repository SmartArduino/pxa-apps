// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>

namespace ash {
struct UiRect {float x,y,w,h;};
struct RenderExtent {int width,height;};
inline RenderExtent pixelSurface(int width,int height){
    if(width<640||height<480)return {width,height};
    // Keep the complete 320x240 UI before presentation scaling. Halving a
    // 480px square used to discard rows and columns from the 12px font.
    // The Host presents integer render scales, so keep the physical extent
    // when a half-size surface would be narrower than the complete UI.
    return {width/2,height/2};
}
struct CardGrid {int top=26,height=64,gap=6;int row(int index)const{return top+(index/2)*(height+gap);}};
inline CardGrid cardGrid(int lift,bool opponent=false){
    CardGrid g;g.top=opponent?24:26;g.gap=opponent?4:6;
    int bottom=(opponent?192:210)-lift-6;
    g.height=std::min(64,(bottom-g.top-g.gap)/2);return g;
}
struct HomeLayout {int actions=198,character=137,toolbar=106,audio=80,logoHeight=72;};
inline HomeLayout homeLayout(int menuLift){
    HomeLayout h;h.actions-=menuLift;h.character=std::min(137,h.actions-34);
    h.toolbar=std::min(106,h.character-30);h.audio=std::min(80,h.toolbar-26);
    h.logoHeight=std::min(72,h.toolbar-16);return h;
}
inline UiRect homeControl(int index,int left,int right,int y){
    if(right-left>=296){const int x[]={left,104,right-130,right-56},w[]={82,64,64,56};return {float(x[index]),float(y),float(w[index]),20};}
    int first=std::clamp((right-left-18)/4,60,82),w=(right-left-first-18)/3;
    int x=index?left+first+6+(index-1)*(w+6):left;
    return {float(x),float(y),float(index?index==3?right-x:w:first),20};
}
inline UiRect homeAction(int index,int left,int right,int y){
    int w=(right-left-38)/3;return {float(left+10+index*(w+9)),float(y),float(w),26};
}
struct RoomListLayout {int capacity=4,step=26,first=0;};
inline RoomListLayout roomListLayout(int lift,int rooms,int selected){
    RoomListLayout r;int available=210-lift-65-6;
    r.capacity=std::clamp(available/20,1,4);r.step=std::min(26,available/r.capacity);
    r.first=std::clamp(selected-r.capacity+1,0,std::max(0,rooms-r.capacity));return r;
}
struct AddressLayout {
    int columns=3,top=81,step=24,width=60,columnStep=65,left=65,inputY=62;
    bool prompt=true;
    UiRect key(int i)const{return {float(left+(i%columns)*columnStep),float(top+(i/columns)*step),float(width),float(step-2)};}
};
inline AddressLayout addressLayout(int lift){
    AddressLayout a;int footer=210-lift;
    if(footer<177){a.columns=4;a.width=54;a.columnStep=60;}
    if(footer<140){a.prompt=false;a.inputY=43;a.top=62;}
    a.step=std::min(24,(footer-a.top-4)/(12/a.columns));
    a.left=(320-((a.columns-1)*a.columnStep+a.width))/2;return a;
}
inline int addressNavigate(int cursor,int dx,int dy,int lift){
    auto a=addressLayout(lift);int rows=12/a.columns;
    if(dx)return cursor>=12?(cursor==12?13:12):(cursor/a.columns)*a.columns+(cursor%a.columns+dx+a.columns)%a.columns;
    if(cursor>=12)return (dy<0?rows-1:0)*a.columns+(cursor==13?0:a.columns-1);
    int row=cursor/a.columns+dy,col=cursor%a.columns;
    return row<0||row>=rows?(col<a.columns/2?13:12):row*a.columns+col;
}
struct CoverImage {float x,y,scale;};
inline CoverImage coverImage(float imageWidth,float imageHeight,UiRect viewport,float focusX=.5f){
    float scale=std::max(viewport.w/imageWidth,viewport.h/imageHeight);
    return {std::clamp(viewport.x+viewport.w*.5f-imageWidth*scale*focusX,
                      viewport.x+viewport.w-imageWidth*scale,viewport.x),
            viewport.y+(viewport.h-imageHeight*scale)*.5f,scale};
}
struct PracticeLayout {
    int rowTop=44,rowStep=26,rowHeight=22,resetY=130,resetHeight=24;
    int returnY=204,statsY=162;
    bool showStats=true;
};
inline PracticeLayout practiceLayout(int lift){
    PracticeLayout p;p.returnY=210-lift;
    p.showStats=p.returnY>=180;
    p.resetY=p.showStats?130:124;
    p.resetHeight=p.showStats?24:20;
    if(p.returnY<154){
        p.rowHeight=20;p.rowStep=24;p.rowTop=38;
        p.resetY=114;p.resetHeight=20;
    }
    if(p.returnY<138){
        p.rowHeight=20;p.rowStep=22;p.rowTop=32;
        p.resetY=std::min(100,p.returnY-24);p.resetHeight=20;
    }
    return p;
}
}
