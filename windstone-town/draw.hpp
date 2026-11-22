#pragma once

#include "game.hpp"
#include <pxa/pixels.hpp>

#include <algorithm>
#include <cstdlib>

namespace wind {
namespace art {
extern const unsigned char title[], sprites[], font[], terrain[], plants[], actors[];
extern const unsigned char monsters[], ui[], icons[], effects[], relics[], caves[];
extern const unsigned char walls[], stairs[], backdrop[], tools[], markers[], waterfall[], glints[];
extern const unsigned char expedition[], folio[];
extern const unsigned char walking[], combat[];
extern const unsigned char items[];
extern const unsigned char combat_body[], held_weapons[], worn_gear[];
}

using Color=uint16_t;
constexpr Color rgb(unsigned value) {
    return ((value>>8)&0xf800)|((value>>5)&0x07e0)|((value>>3)&0x001f);
}
constexpr Color mix(Color a, Color b, unsigned x) {
    const unsigned r=(((a>>11)*(15-x)+(b>>11)*x)+7)/15;
    const unsigned g=((((a>>5)&63)*(15-x)+((b>>5)&63)*x)+7)/15;
    const unsigned bl=(((a&31)*(15-x)+(b&31)*x)+7)/15;
    return Color(r<<11 | g<<5 | bl);
}

inline constexpr Color Ink=rgb(0x263b34), Quiet=rgb(0x655c50),
    Paper=rgb(0xfff2d8), Brass=rgb(0xe9ce8c), Mint=rgb(0x365f51), Rust=rgb(0x943e36),
    TextInk=rgb(0x2d3b32), LightHint=rgb(0xd8ccad), PanelShade=rgb(0x1e2e2a),
    DeepShade=rgb(0x202c2e), AllyHealth=rgb(0x8bab77), EnemyHealth=rgb(0xdfad6e),
    RailDivider=rgb(0x65765f);
enum Skin { MarblePaper, WheatButton, TealButton, AmberButton, InactiveButton, TitleRail,
            SelectedRow, FocusedRow, StatusRail, DockTile, DockAction, MessagePlaque };

struct Rect {
    int x,y,w,h;
    int right() const { return x+w; }
    int bottom() const { return y+h; }
    Point center() const { return {x+w/2,y+h/2}; }
    bool contains(Point p) const { return p.x>=x && p.x<right() && p.y>=y && p.y<bottom(); }
    Rect inset(int n) const { return {x+n,y+n,w-2*n,h-2*n}; }
};

inline int read16(const unsigned char* p) {return p[0] | (p[1]<<8);}
inline uint32_t read32(const unsigned char* p) {return uint32_t(read16(p)) | (uint32_t(read16(p+2))<<16);}
inline Color indexed_color(const unsigned char* atlas,unsigned index) {
    return Color(read16(atlas+4+2*index));
}
inline const unsigned char* atlas_row(const unsigned char* atlas,int x,int y) {
    return atlas+516+y*read16(atlas)+x;
}

// Native coordinates in the two axes of the diamond. The masks shape roads
// and banks; every color comes from an already projected atlas pixel.
constexpr int TileWidth=36,TileHeight=20;
inline constexpr auto diamond_coordinates=[] {
    std::array<uint16_t,TileWidth*TileHeight> result{};
    for (int y=0;y<TileHeight;++y)
        for (int x=0;x<TileWidth;++x) {
            const int dx=2*(x-18)+1,dy=2*y+1;
            const int u=dy*18+dx*10,v=dy*18-dx*10;
            result[y*TileWidth+x]=u>=0 && u<720 && v>=0 && v<720
                ?uint16_t((u/30)|((v/30)<<5)):uint16_t(0xffff);
        }
    return result;
}();
inline unsigned character(Text& text) {
    unsigned ch=static_cast<unsigned char>(text.front());
    text.remove_prefix(1);
    if (ch<128) return ch;
    const int left=ch<224?1:(ch<240?2:3);
    ch&=0x3f>>left;
    for (int i=0;i<left && !text.empty();++i) {
        ch=(ch<<6)|(static_cast<unsigned char>(text.front())&63);
        text.remove_prefix(1);
    }
    return ch;
}

struct Glyph {
    int advance,left,top,width,height;
    const unsigned char* bitmap;
};
inline Glyph glyph(unsigned code,int size) {
    const int slot=size==24?3:size==16?2:size==12?1:0;
    const auto* base=art::font+read32(art::font+slot*4);
    constexpr int stride=11;
    const int count=read16(base);
    int low=0,high=count;
    const auto* data=base+2;
    while (low<high) {
        const int mid=(low+high)/2;
        if (unsigned(read16(data+mid*stride))<code) low=mid+1;
        else high=mid;
    }
    if (low==count || unsigned(read16(data+low*stride))!=code) low='?'-32;
    const auto* entry=data+low*stride;
    return {entry[2],int8_t(entry[3]),int8_t(entry[4]),entry[5],entry[6],data+count*stride+read32(entry+7)};
}

struct TextLine {
    Text value;
    Rect ink;
    int size;
};

class Draw {
public:
    pxa::Rgb565Pixels pixels;
    Rect clip;
    Draw(pxa::Rgb565Pixels data): pixels(data),clip{0,0,int(data.width()),int(data.height())} {}

    void fill(Rect r, Color c) {
        const int x=std::max(clip.x,r.x), end=std::min(clip.right(),r.right());
        if (x>=end) return;
        for (int y=std::max(clip.y,r.y);y<std::min(clip.bottom(),r.bottom());++y) {
            auto row=*pixels.row(y);
            for (int xx=x;xx<end;++xx) row[xx]=c;
        }
    }
    void shade(Rect r,Color color,unsigned amount) {
        const int left=std::max(r.x,clip.x),right=std::min(r.right(),clip.right());
        for (int y=std::max(r.y,clip.y);y<std::min(r.bottom(),clip.bottom());++y) {
            auto row=*pixels.row(y);
            for (int x=left;x<right;++x) row[x]=mix(row[x],color,amount);
        }
    }
    void blit(const unsigned char* atlas,Rect src,Point destination,bool flip=false) {
        const int left=std::max(clip.x,destination.x),right=std::min(clip.right(),destination.x+src.w);
        if (left>=right) return;
        for (int y=std::max(clip.y,destination.y);y<std::min(clip.bottom(),destination.y+src.h);++y) {
            const auto* source=atlas_row(atlas,src.x,src.y+y-destination.y);
            auto row=*pixels.row(y);
            for (int x=left;x<right;++x) {
                const int dx=x-destination.x;
                const unsigned index=source[flip?src.w-1-dx:dx];
                if (index!=255) row[x]=indexed_color(atlas,index);
            }
        }
    }
    void blit_double(const unsigned char* atlas,Rect src,Point destination,bool flip=false) {
        // This is the only enlarged sprite path. Each source pixel owns an
        // exact 2 × 2 block, including at the clip edges.
        const int left=std::max(clip.x,destination.x),right=std::min(clip.right(),destination.x+2*src.w);
        if (left>=right) return;
        for (int y=std::max(clip.y,destination.y);y<std::min(clip.bottom(),destination.y+2*src.h);++y) {
            const auto* source=atlas_row(atlas,src.x,src.y+(y-destination.y)/2);
            auto row=*pixels.row(y);
            for (int x=left;x<right;++x) {
                const int dx=(x-destination.x)/2;
                const auto index=source[flip?src.w-1-dx:dx];
                if (index!=255) row[x]=indexed_color(atlas,index);
            }
        }
    }
    void crop(const unsigned char* atlas,Rect src,Rect destination,bool align_bottom=false) {
        const int w=std::min(src.w,destination.w),h=std::min(src.h,destination.h);
        if (w<=0 || h<=0) return;
        src.x+=(src.w-w)/2;
        src.y+=align_bottom?src.h-h:(src.h-h)/2;
        blit(atlas,{src.x,src.y,w,h},{destination.x+(destination.w-w)/2,
            destination.y+(align_bottom?destination.h-h:(destination.h-h)/2)});
    }
    template<class Sample>
    void tile(Point p,Sample sample) {
        const int left=std::max(clip.x,p.x),right=std::min(clip.right(),p.x+TileWidth);
        if (left>=right) return;
        for (int y=std::max(clip.y,p.y);y<std::min(clip.bottom(),p.y+TileHeight);++y) {
            auto row=*pixels.row(y);
            const int sy=y-p.y;
            const auto* coords=diamond_coordinates.data()+sy*TileWidth;
            for (int x=left;x<right;++x) {
                const int sx=x-p.x;
                const auto uv=coords[sx];
                if (uv!=0xffff) sample(row[x],sx,sy,int(uv&31),int(uv>>5));
            }
        }
    }
    void road_tile(Rect grass,Rect stone,Point p,unsigned links,bool square) {
        tile(p,[&](pxa::Rgb565Row::Pixel pixel,int sx,int sy,int x,int y) {
            const int lo=square?3:7,hi=24-lo;
            const bool across=y>=7 && y<17 && ((links&8 && x<12) || (links&2 && x>=12));
            const bool along=x>=7 && x<17 && ((links&1 && y<12) || (links&4 && y>=12));
            const bool center=x>=lo && x<hi && y>=lo && y<hi;
            const bool clipped_corner=(x==lo || x==hi-1) && (y==lo || y==hi-1);
            const auto& source=(across || along || (center && !clipped_corner))?stone:grass;
            const unsigned color=*atlas_row(art::terrain,source.x+sx,source.y+sy);
            if (color!=255) pixel=indexed_color(art::terrain,color);
        });
    }
    void river_tile(Rect water,Rect banks,Point p,unsigned shore) {
        tile(p,[&](pxa::Rgb565Row::Pixel pixel,int sx,int sy,int x,int y) {
            int distance=24;
            if (shore&1) distance=std::min(distance,y);
            if (shore&2) distance=std::min(distance,23-x);
            if (shore&4) distance=std::min(distance,23-y);
            if (shore&8) distance=std::min(distance,x);
            const unsigned color=*atlas_row(art::terrain,water.x+sx,water.y+sy);
            if (color==255) return;
            if (distance<2) {
                const auto bank=*atlas_row(art::terrain,banks.x+sx,banks.y+sy);
                if (bank!=255) {
                    pixel=mix(indexed_color(art::terrain,color),indexed_color(art::terrain,bank),distance==0?10:4);
                    return;
                }
            }
            pixel=indexed_color(art::terrain,color);
        });
    }
    void shadow(const unsigned char* atlas,Rect source,Point p,Color color,unsigned strength) {
        const int left=std::max(clip.x,p.x),right=std::min(clip.right(),p.x+source.w);
        for (int y=std::max(clip.y,p.y);y<std::min(clip.bottom(),p.y+source.h);++y) {
            auto row=*pixels.row(y);
            const auto* mask=atlas_row(atlas,source.x,source.y+y-p.y);
            for (int x=left;x<right;++x)
                if (mask[x-p.x]!=255) row[x]=mix(row[x],color,strength);
        }
    }
    void repeat(const unsigned char* atlas,Rect src,Rect dst) {
        if (src.w<=0 || src.h<=0) return;
        for (int y=dst.y;y<dst.bottom();y+=src.h)
            for (int x=dst.x;x<dst.right();x+=src.w)
                blit(atlas,{src.x,src.y,std::min(src.w,dst.right()-x),
                                        std::min(src.h,dst.bottom()-y)},{x,y});
    }
    void book(Rect r) {
        constexpr int width=72,height=52,cap=8;
        const int a=std::min(cap,r.w/2),b=std::min(cap,r.h/2);
        const int sx[]{0,a,width-a,width},sy[]{0,b,height-b,height};
        const int dx[]{r.x,r.x+a,r.right()-a,r.right()},dy[]{r.y,r.y+b,r.bottom()-b,r.bottom()};
        for (int j=0;j<3;++j) for (int i=0;i<3;++i) {
            const Rect src{sx[i],sy[j],sx[i+1]-sx[i],sy[j+1]-sy[j]};
            const Rect dst{dx[i],dy[j],dx[i+1]-dx[i],dy[j+1]-dy[j]};
            repeat(art::folio,src,dst);
        }
    }
    void skin(Rect r,int variant) {
        if (r.w<=0 || r.h<=0) return;
        if (variant==MarblePaper) {book(r); return;}
        const int source_x=variant%4*40,source_y=variant/4*30;
        const int source_height=variant==MessagePlaque?18:30;
        const int frame=variant==MessagePlaque || variant==SelectedRow || variant==FocusedRow?3:4;
        const int edge=std::min({frame,r.w/2,r.h/2});
        const auto sample=[&](int x,int y) {
            return *atlas_row(art::ui,source_x+x,source_y+y);
        };
        const auto center=sample(20,source_height/2);
        if (center!=255) fill(r.inset(edge),indexed_color(art::ui,center));
        for (int i=0;i<edge;++i) {
            for (const bool far:{false,true}) {
                const int sy=far?source_height-edge+i:i;
                const int y=far?r.bottom()-edge+i:r.y+i;
                const auto index=sample(14,sy);
                if (index!=255) fill({r.x+edge,y,r.w-edge*2,1},indexed_color(art::ui,index));
                const int sx=far?40-edge+i:i;
                const int x=far?r.right()-edge+i:r.x+i;
                const auto border=sample(sx,source_height/2);
                if (border!=255) fill({x,r.y+edge,1,r.h-edge*2},indexed_color(art::ui,border));
            }
        }
        for (const bool bottom:{false,true}) {
            for (const bool right:{false,true}) {
                const Rect source{source_x+(right?40-edge:0),source_y+(bottom?source_height-edge:0),edge,edge};
                blit(art::ui,source,{right?r.right()-edge:r.x,bottom?r.bottom()-edge:r.y});
            }
        }
    }
    static Color paper_color() {return indexed_color(art::folio,*atlas_row(art::folio,36,26));}
    static Color paper_rule() {return indexed_color(art::ui,*atlas_row(art::ui,156,106));}
    static constexpr int FolderTabHeight=19;
    static constexpr int FolderRim=2;
    void folder_page(Rect r,Rect active) {
        repeat(art::folio,{8,8,56,36},r);
        const auto* edge=atlas_row(art::ui,0,90+FolderTabHeight-1);
        // Continue the two native pixels at the foot of the tab around the
        // page. Each line meets the corresponding edge of the opening.
        for (int i=0;i<FolderRim;++i) {
            const auto rim=indexed_color(art::ui,edge[i]);
            const auto inner=r.inset(i);
            fill({inner.x,inner.y,1,inner.h},rim);
            fill({inner.right()-1,inner.y,1,inner.h},rim);
            fill({inner.x,inner.bottom()-1,inner.w,1},rim);
            fill({inner.x,inner.y,active.x-r.x+1,1},rim);
            fill({active.right()-1-i,inner.y,r.right()-active.right()+1,1},rim);
        }
    }
    void folder_tab(Rect r,bool selected,bool focused) {
        constexpr int width=78;
        const int edge=std::min(12,r.w/2);
        const int top=90+(focused?FolderTabHeight:0);
        const int sx=selected?0:width;
        blit(art::ui,{sx,top,edge,r.h},{r.x,r.y});
        repeat(art::ui,{sx+edge,top,width-2*edge,r.h},{r.x+edge,r.y,r.w-2*edge,r.h});
        blit(art::ui,{sx+width-edge,top,edge,r.h},{r.right()-edge,r.y});
    }
    static int measure(Text value,int size=12) {
        int w=0;
        while (!value.empty()) w+=glyph(character(value),size).advance;
        return w;
    }
    void text(int x,int y,Text value,Color color=Paper,int size=12,int max_width=9999) {
        const int right=x+max_width;
        while (!value.empty()) {
            const auto g=glyph(character(value),size);
            const int gx0=x+g.left,gy0=y+g.top;
            if (gx0+g.width>right || gx0>=clip.right()) break;
            const int left=std::max(0,clip.x-gx0),end=std::min(g.width,clip.right()-gx0);
            for (int gy=std::max(0,clip.y-gy0);gy<std::min(g.height,clip.bottom()-gy0);++gy) {
                auto row=*pixels.row(gy+gy0);
                const auto* data=g.bitmap+gy*((g.width+1)/2);
                for (int gx=left;gx<end;++gx) {
                    const unsigned a=(gx&1)?data[gx/2]&15:data[gx/2]>>4;
                    if (a) row[gx0+gx]=mix(row[gx0+gx],color,a);
                }
            }
            x+=g.advance;
        }
    }
    static TextLine fit(Rect r,Text value,int size=12) {
        for (;;) {
            Rect ink{};
            int pen=0;
            Text rest=value;
            size_t length=0;
            while (!rest.empty()) {
                const auto g=glyph(character(rest),size);
                if (g.width && g.height) {
                    Rect next{pen+g.left,g.top,g.width,g.height};
                    if (ink.h) {
                        const int x=std::min(ink.x,next.x),y=std::min(ink.y,next.y);
                        next={x,y,std::max(ink.right(),next.right())-x,std::max(ink.bottom(),next.bottom())-y};
                    }
                    if (next.w>r.w || next.h>r.h) break;
                    ink=next;
                }
                pen+=g.advance;
                length=value.size()-rest.size();
            }
            if (length==value.size() || size==10) return {value.substr(0,length),ink,size};
            size=size==24?16:size==16?12:10;
        }
    }
    void line(Rect r,TextLine text_line,Color color,bool centered=false) {
        const auto& ink=text_line.ink;
        if (!ink.h) return;
        const int x=r.x-ink.x+(centered?(r.w-ink.w)/2:0);
        text(x,r.y+(r.h-ink.h)/2-ink.y,text_line.value,color,text_line.size);
    }
    void label(Rect r,Text value,Color c=Paper,int size=12) {
        line(r,fit(r,value,size),c);
    }
    void center(Rect r,Text value,Color c=Paper,int size=12) {
        line(r,fit(r,value,size),c,true);
    }
    void bar(Rect r,int have,int total,bool enemy=false) {
        fill(r,DeepShade);
        const Rect progress{r.x+1,r.y+1,(r.w-2)*std::clamp(have,0,total)/total,r.h-2};
        fill(progress,enemy?EnemyHealth:AllyHealth);
    }
};

enum Icon { Coin, Star, Sun, Hammer, People, Compass, Book, Map, Gem, Play, Fast, Stop, CloseIcon, Chest, Crystal, Medicine };
inline void item_icon(Draw& d,Item item,Point p,bool large=false) {
    if (!valid_item(item)) return;
    const int i=item_index(item);
    const Rect src{i%4*18,i/4*18,18,18};
    if (large) d.blit_double(art::items,src,p);
    else d.blit(art::items,src,p);
}
inline void icon(Draw& d,Icon type,int x,int y,int size=10) {
    if (type==Medicine) {
        d.blit(art::expedition,{4*10,56,10,10},{x,y});
        return;
    }
    const int row=size==10?0:size==12?10:size==14?22:36;
    d.blit(art::icons,{int(type)*18,row,size,size},{x,y});
}
inline void commission_icon(Draw& d,CommissionKind kind,Point p) {
    if (kind==CommissionKind::Hunt) icon(d,Compass,p.x+2,p.y+2,16);
    else d.blit(art::expedition,{(int(kind)-1)*42+11,36,20,20},p);
}
}
