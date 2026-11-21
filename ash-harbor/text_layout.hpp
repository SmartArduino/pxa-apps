// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "ui_layout.hpp"
#include <array>
#include <string_view>

namespace ash {
inline constexpr std::string_view pixelCharacters="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-:/.!?><=()abcdefghijklmnopqrstuvwxyz";
struct PixelText {
    std::string_view text;
    const art::ChineseLabel* label=nullptr;
    unsigned count=0;
    bool valid=true;
    explicit PixelText(std::string_view s):text(s){
        if(!s.empty()&&uint8_t(s[0])>=128){
            for(const auto& item:art::chinese)if(s==item.text){label=&item;break;}
            valid=label!=nullptr;count=label?label->count:0;
        }else count=s.size();
    }
    unsigned advance(unsigned i)const{return label?art::label_glyphs[label->first+i].advance:6;}
    art::Ink glyph(unsigned i)const{
        if(label)return art::label_glyphs[label->first+i].ink;
        auto index=pixelCharacters.find(text[i]);
        if(index==pixelCharacters.npos)return {};
        auto ink=art::glyph_ink[index];
        // ASCII has the existing one-pixel drop shadow; Chinese is a solid mask.
        if(ink.w&&ink.h){++ink.w;++ink.h;}
        return ink;
    }
    art::Ink ink(unsigned first,unsigned n)const{
        int offset=0,l=0,t=0,r=0,b=0;bool any=false;
        for(unsigned i=first;i<first+n;++i){auto g=glyph(i);
            if(g.w&&g.h){if(!any){l=offset+g.x;t=g.y;r=l+g.w;b=t+g.h;any=true;}
                else{l=std::min(l,offset+g.x);t=std::min<int>(t,g.y);r=std::max(r,offset+g.x+g.w);b=std::max<int>(b,g.y+g.h);}}
            offset+=advance(i);
        }
        return {int16_t(l),int16_t(t),int16_t(r-l),int16_t(b-t)};
    }
};
struct TextLine {unsigned first=0,count=0;art::Ink ink{};};
struct TextPlan {
    std::array<TextLine,8> lines{};
    unsigned count=0;
    int top=0,height=0;
    bool elided=false;
};
inline TextPlan planText(const PixelText& run,UiRect box,int maxLines=1){
    TextPlan out;unsigned first=0;
    for(int row=0;first<run.count&&row<std::clamp(maxLines,1,8);++row){
        unsigned n=0;
        while(first+n<run.count){auto ink=run.ink(first,n+1);if(ink.w>box.w)break;++n;}
        if(!n)break;
        auto ink=run.ink(first,n);out.lines[out.count++]={first,n,ink};first+=n;
    }
    out.elided=first<run.count;
    while(out.count){int t=0,b=0;bool any=false;
        for(unsigned i=0;i<out.count;++i){auto g=out.lines[i].ink;if(!g.h)continue;
            int y=i*14+g.y;if(!any){t=y;b=y+g.h;any=true;}else{t=std::min(t,y);b=std::max<int>(b,y+g.h);}}
        out.top=t;out.height=b-t;
        if(out.height<=box.h)break;
        --out.count;out.elided=true;
    }
    return out;
}
inline UiRect textOrigin(UiRect box,art::Ink ink,int align=0){
    return {box.x+(align==1?std::floor((box.w-ink.w)*.5f):align==2?box.w-ink.w:0)-ink.x,
            box.y+std::floor((box.h-ink.h)*.5f)-ink.y,float(ink.w),float(ink.h)};
}
} // namespace ash
