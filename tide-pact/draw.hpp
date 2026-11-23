#pragma once

#include "assets.hpp"
#include "game.hpp"
#include <pxa/pixels.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace tide {

using Color = std::uint16_t;
constexpr Color rgb(unsigned v) {
    return ((v >> 8) & 0xf800) | ((v >> 5) & 0x07e0) | ((v >> 3) & 0x001f);
}

struct Rect {
    int x, y, w, h;
    bool operator==(const Rect&) const = default;
    int right() const { return x+w; }
    int bottom() const { return y+h; }
    bool contains(Point p) const {
        return p.x >= x && p.y >= y && p.x < right() && p.y < bottom();
    }
    Rect inset(int n) const { return {x+n,y+n,w-n*2,h-n*2}; }
};

inline int u16(const unsigned char* p) { return p[0] | (p[1] << 8); }

struct Image {
    const unsigned char* bytes;
    int width() const { return u16(bytes)&0x7fff; }
    bool packed() const { return (bytes[1]&0x80)!=0; }
    int height() const { return u16(bytes+2); }
    unsigned index(int x, int y) const {
        const int offset=y*width()+x;
        return packed()?packed_terrain(bytes+516,offset):bytes[516+offset];
    }
    static unsigned packed_terrain(const unsigned char* data, int offset) {
        return (data[offset/4]>>(2*(offset&3)))&3;
    }
    Color color(unsigned index) const { return u16(bytes+4 + 2*index); }
};

inline unsigned next_char(Text& text) {
    unsigned first = static_cast<unsigned char>(text.front());
    text.remove_prefix(1);
    if (first < 128) return first;
    int tail = first < 224 ? 1 : (first < 240 ? 2 : 3);
    auto code = first & (0x3f >> tail);
    for (int i = 0; i < tail && !text.empty(); ++i) {
        code = (code << 6) | (static_cast<unsigned char>(text.front()) & 0x3f);
        text.remove_prefix(1);
    }
    return code;
}

struct FontFace {
    const unsigned char* atlas;
    int cell;
    int stride() const { return (cell+1)/2; }

    const unsigned char* glyph(unsigned code) const {
        const int record=3+cell*stride()+4;
        const auto* start=atlas+2;
        if (code>=32 && code<127) return start+(code-32)*record+2;
        const int count=u16(atlas);
        int lo=0, hi=count;
        while (lo<hi) {
            const int mid=(lo+hi)/2;
            if (unsigned(u16(start+mid*record))<code) lo=mid+1; else hi=mid;
        }
        if (lo==count || unsigned(u16(start+lo*record))!=code) lo=int('?' - ' ');
        return start+lo*record+2;
    }
    Rect ink_bounds(const unsigned char* glyph) const {
        const auto* bounds=glyph+1+cell*stride();
        return {bounds[0],bounds[1],bounds[2]-bounds[0],bounds[3]-bounds[1]};
    }
};
inline constexpr FontFace StandardFace{art::glyphs,14}, CompactFace{art::glyphs_compact,12};
inline constexpr FontFace NativeFace{art::glyphs_native,24};

struct Draw {
    enum class SliceCenter { Scale, Tile, Transparent };
    struct TextRun {
        Text text;
        Rect ink;
    };

    pxa::Rgb565Pixels pixels;
    std::span<std::byte> frame_bytes;
    std::uint32_t stride_bytes;
    Rect clip;
    const FontFace& font;
    int pixel_scale;

    explicit Draw(pxa::Rgb565Pixels image,std::span<std::byte> bytes,std::uint32_t stride,
                  const FontFace& face=StandardFace,int density=1)
        : pixels(image), frame_bytes(bytes), stride_bytes(stride),
          clip{0,0,int(image.width())/density,int(image.height())/density}, font(face),
          pixel_scale(density) {}

    Rect pixel_rect(Rect rect) const {
        return {rect.x*pixel_scale,rect.y*pixel_scale,rect.w*pixel_scale,rect.h*pixel_scale};
    }
    Draw native_pixels() const {
        Draw draw(pixels,frame_bytes,stride_bytes,font);
        draw.clip=pixel_rect(clip);
        return draw;
    }

    Color* row(int y) const {
        // Mapped RGB565 frames and our terrain cache both use aligned,
        // little-endian rows. Use halfword stores in the raster loops.
        static_assert(std::endian::native==std::endian::little);
        return reinterpret_cast<Color*>(frame_bytes.data()+y*stride_bytes);
    }

    int measure(Text text) const {
        int result=0;
        while (!text.empty()) result+=*font.glyph(next_char(text));
        return result;
    }

    Rect clipped(Rect r) const {
        int x = std::max(r.x, clip.x), y = std::max(r.y, clip.y);
        return {x, y, std::max(0,std::min(r.right(),clip.right())-x),
                std::max(0,std::min(r.bottom(),clip.bottom())-y)};
    }
    void dot(int x, int y, Color color) {
        if (pixel_scale==1) {
            if (clip.contains({x,y})) row(y)[x]=color;
        } else fill({x,y,1,1},color);
    }
    void fill(Rect rect, Color color) {
        const auto r = pixel_rect(clipped(rect));
        if (!r.w || !r.h) return;
        if (r.w>=32) {
            const auto start=row(r.y)+r.x;
            std::fill_n(start,r.w,color);
            for (int y=r.y+1;y<r.bottom();++y)
                std::memcpy(row(y)+r.x,start,r.w*sizeof(Color));
        } else {
            for (int y=r.y;y<r.bottom();++y) std::fill_n(row(y)+r.x,r.w,color);
        }
    }
    void line(Point a, Point b, Color color, bool dashed = false) {
        const int dx = std::abs(a.x-b.x), dy = -std::abs(a.y-b.y);
        const int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
        int err = dx+dy, i = 0;
        while (true) {
            if (!dashed || (i++ & 7) < 5) dot(a.x,a.y,color);
            if (a.x == b.x && a.y == b.y) break;
            int e = err*2;
            if (e >= dy) { err += dy; a.x += sx; }
            if (e <= dx) { err += dx; a.y += sy; }
        }
    }
    void frame(Rect r, Color color) {
        fill({r.x,r.y,r.w,1},color);
        fill({r.x,r.bottom()-1,r.w,1},color);
        fill({r.x,r.y,1,r.h},color);
        fill({r.right()-1,r.y,1,r.h},color);
    }
    void diamond(Point p, int radius, Color color) {
        for (int dy = -radius; dy <= radius; ++dy) {
            const int extent = radius - std::abs(dy);
            fill({p.x-extent,p.y+dy,2*extent+1,1},color);
        }
    }
    static int relief_level(int x,int y) {
        constexpr int stride=8, columns=(ChartWidth+stride-1)/stride, rows=(ChartHeight+stride-1)/stride;
        const auto read=[](int column,int row) {
            const int index=row*columns+column;
            return int((art::relief[index/2]>>(4*(index&1)))&15);
        };
        const int column=x/stride, row=y/stride;
        const int right=std::min(column+1,columns-1), down=std::min(row+1,rows-1);
        const int dx=x&(stride-1), dy=y&(stride-1);
        const int top=read(column,row)*(stride-dx)+read(right,row)*dx;
        const int bottom=read(column,down)*(stride-dx)+read(right,down)*dx;
        return (top*(stride-dy)+bottom*dy+stride*stride/2)/(stride*stride);
    }

    void image(Image img, Rect src, Rect dst, bool transparent = false, bool flip = false) {
        auto target=native_pixels();
        target.blit(img,src,pixel_rect(dst),transparent,flip);
    }
    void blit(Image img, Rect src, Rect dst, bool transparent, bool flip) {
        if (dst.w<=0 || dst.h<=0 || src.w<=0 || src.h<=0) return;
        const auto r = clipped(dst);
        if (!r.w || !r.h) return;
        const int initial=(r.x-dst.x)*src.w;
        const int first=initial/dst.w, first_error=initial%dst.w;
        const int pitch=src.w/dst.w, step=src.w%dst.w;
        const int stride=img.width();
        const bool packed=img.packed();
        const bool distant=packed && dst.w*2<src.w;
        int previous_row=-1;
        for (int y = r.y; y < r.bottom(); ++y) {
            const int sy = src.y + (y-dst.y)*src.h/dst.h;
            auto* scan=row(y);
            if (!packed && !transparent && sy==previous_row) {
                auto* scanline=frame_bytes.data()+y*stride_bytes+r.x*sizeof(Color);
                std::memcpy(scanline,scanline-stride_bytes,r.w*sizeof(Color));
                continue;
            }
            previous_row=sy;
            const int line=sy*stride;
            const bool latitude_line=packed && (sy-ChartNorth*ChartLatitudeScale)%(10*ChartLatitudeScale)==0;
            const auto* indexes=img.bytes+516;
            int source=first, error=first_error;
            for (int x = r.x; x < r.right(); ++x) {
                const int sx=src.x+(flip?src.w-1-source:source), offset=line+sx;
                unsigned index;
                if (packed) {
                    const bool grid=latitude_line || (sx+ChartWest*ChartLongitudeScale)%(10*ChartLongitudeScale)==0;
                    const unsigned grain=unsigned(sx)*0x9e3779b1U ^ unsigned(sy)*0x85ebca77U;
                    const auto terrain=Image::packed_terrain(indexes,offset);
                    index=terrain*4+(grid?3:distant?1:grain>>31);
                    scan[x]=img.color(index);
                } else {
                    index=indexes[offset];
                    if (!transparent || index != 255) scan[x]=img.color(index);
                }
                source+=pitch;
                error+=step;
                if (error>=dst.w) { error-=dst.w; ++source; }
            }
        }
    }
    void tile(Image img, Rect src, Rect dst, bool transparent) {
        const auto r=clipped(dst);
        if (!r.w || !r.h) return;
        const int first=(r.x-dst.x)%src.w;
        int offset_y=(r.y-dst.y)%src.h;
        for (int y=r.y;y<r.bottom();++y) {
            auto* scan=row(y);
            const auto* source=img.bytes+516+(src.y+offset_y)*img.width()+src.x;
            for (int x=r.x,offset=first;x<r.right();offset=0) {
                const int count=std::min(src.w-offset,r.right()-x);
                for (int i=0;i<count;++i) {
                    const auto index=source[offset+i];
                    if (!transparent || index!=255) scan[x+i]=img.color(index);
                }
                x+=count;
            }
            if (++offset_y==src.h) offset_y=0;
        }
    }
    void nine_slice(Image img, Rect src, Rect dst, int source_edge, int edge, bool transparent = false,
                    SliceCenter center=SliceCenter::Scale) {
        edge=std::min({edge,dst.w/3,dst.h/3});
        const auto visible=clipped(dst);
        if (edge<1 || !visible.w || !visible.h) return;
        auto target=native_pixels();
        dst=pixel_rect(dst);
        edge*=pixel_scale;
        const int x[]{dst.x,dst.x+edge,dst.right()-edge,dst.right()};
        const int y[]{dst.y,dst.y+edge,dst.bottom()-edge,dst.bottom()};
        const int u[]{src.x,src.x+source_edge,src.right()-source_edge,src.right()};
        const int v[]{src.y,src.y+source_edge,src.bottom()-source_edge,src.bottom()};
        for (int j=0;j<3;++j) for (int i=0;i<3;++i) {
            const Rect from{u[i],v[j],u[i+1]-u[i],v[j+1]-v[j]};
            const Rect to{x[i],y[j],x[i+1]-x[i],y[j+1]-y[j]};
            if (i==1 && j==1 && center!=SliceCenter::Scale) {
                if (center==SliceCenter::Tile) target.tile(img,from,to,false);
            } else target.blit(img,from,to,transparent,false);
        }
    }
    void cover(Image img, Rect dst) {
        const int w = std::min(img.width(),img.height()*dst.w/std::max(1,dst.h));
        const int h = std::min(img.height(),img.width()*dst.h/std::max(1,dst.w));
        image(img,{(img.width()-w)/2,(img.height()-h)/2,w,h},dst);
    }
    void darken(Rect box, int strength) {
        const auto r = pixel_rect(clipped(box));
        for (int y = r.y; y < r.bottom(); ++y) {
            auto* scan=row(y);
            for (int x = r.x; x < r.right(); ++x) scan[x] = mix(scan[x], rgb(0x08181d), strength);
        }
    }
    static constexpr Color mix(Color a, Color b, int opacity) {
        const int inv = 15-opacity;
        const auto blend = [&](int shift, int mask) {
            return ((((a >> shift) & mask)*inv + ((b >> shift) & mask)*opacity + 7) / 15) << shift;
        };
        return blend(11,31) | blend(5,63) | blend(0,31);
    }
    void native_glyph(int x,int y,unsigned code,int advance,Color color,int scale) {
        const auto* glyph=NativeFace.glyph(code);
        const auto ink=NativeFace.ink_bounds(glyph);
        const int cell=font.cell*pixel_scale*scale;
        const int u=x*pixel_scale, v=y*pixel_scale;
        const int left=ink.x*cell/NativeFace.cell, top=ink.y*cell/NativeFace.cell;
        const int right=std::min(advance*pixel_scale,(ink.right()*cell+NativeFace.cell-1)/NativeFace.cell);
        const int bottom=(ink.bottom()*cell+NativeFace.cell-1)/NativeFace.cell;
        const auto target=native_pixels();
        const auto box=target.clipped({u+left,v+top,right-left,bottom-top});
        if (!box.w) return;
        const bool direct=cell==NativeFace.cell;
        for (int j=box.y;j<box.bottom();++j) {
            auto* scan=row(j);
            const int sy=direct?j-v:(j-v)*NativeFace.cell/cell;
            const auto* line=glyph+1+sy*NativeFace.stride();
            for (int i=box.x;i<box.right();++i) {
                const int sx=direct?i-u:(i-u)*NativeFace.cell/cell;
                const int alpha=(line[sx/2]>>((sx&1)?0:4))&15;
                if (alpha) scan[i]=alpha==15?color:mix(scan[i],color,alpha);
            }
        }
    }
    void label(int x, int y, Text text, Color color, int max_width = 10000, int scale = 1) {
        if (!clip.w || !clip.h) return;
        const int end = x+max_width;
        while (!text.empty()) {
            const auto code = next_char(text);
            const auto* glyph = font.glyph(code);
            const int advance = *glyph * scale;
            if (x+advance > end) return;
            if (code != ' ') {
                if (pixel_scale>1) {
                    native_glyph(x,y,code,advance,color,scale);
                    x+=advance;
                    continue;
                }
                const auto* g=glyph+1;
                if (scale==1) {
                    const auto ink=font.ink_bounds(glyph);
                    const auto box=clipped({x+ink.x,y+ink.y,ink.w,ink.h});
                    for (int j=box.y;j<box.bottom() && box.w;++j) {
                        auto* scan=row(j);
                        const auto* line=g+(j-y)*font.stride();
                        for (int i=box.x;i<box.right();++i) {
                            const int column=i-x;
                            const int alpha=(line[column/2]>>((column&1)?0:4))&15;
                            if (alpha) scan[i]=alpha==15?color:mix(scan[i],color,alpha);
                        }
                    }
                    x+=advance;
                    continue;
                }
                for (int j = 0; j < font.cell; ++j) {
                    for (int i = 0; i < font.cell; ++i) {
                        const auto packed = g[j*font.stride() + i/2];
                        const int alpha = i % 2 ? packed & 15 : packed >> 4;
                        if (!alpha) continue;
                        auto r = clipped({x+i*scale,y+j*scale,scale,scale});
                        if (!r.w) continue;
                        for (int yy = r.y; yy < r.bottom(); ++yy) {
                            auto* scan=row(yy);
                            for (int xx = r.x; xx < r.right(); ++xx)
                                scan[xx] = alpha == 15 ? color : mix(scan[xx],color,alpha);
                        }
                    }
                }
            }
            x += advance;
        }
    }
    TextRun fit(Text text, int max_width, int scale) const {
        Text rest=text;
        int advance=0;
        Rect ink{};
        while (!rest.empty()) {
            Text next=rest;
            const auto* glyph=font.glyph(next_char(next));
            const int width=*glyph*scale;
            if (advance+width>max_width) break;
            const Rect bounds=font.ink_bounds(glyph);
            if (bounds.w) {
                const Rect spot{advance+bounds.x*scale,bounds.y*scale,bounds.w*scale,bounds.h*scale};
                if (!ink.w) ink=spot;
                else {
                    const int left=std::min(ink.x,spot.x), top=std::min(ink.y,spot.y);
                    ink={left,top,std::max(ink.right(),spot.right())-left,
                                  std::max(ink.bottom(),spot.bottom())-top};
                }
            }
            advance+=width;
            rest=next;
        }
        return {text.substr(0,text.size()-rest.size()),ink};
    }
    void center_run(Rect box, const TextRun& run, Color color, int scale = 1) {
        label(box.x+(box.w-run.ink.w)/2-run.ink.x,box.y+(box.h-run.ink.h)/2-run.ink.y,
              run.text,color,box.w,scale);
    }
    void center(Rect box, Text text, Color color, int scale = 1) {
        center_run(box,fit(text,box.w,scale),color,scale);
    }
    void center_lines(Rect box, Text first, Color first_color, Text second, Color second_color) {
        const auto top=fit(first,box.w,1), bottom=fit(second,box.w,1);
        const int y=box.y+(box.h-top.ink.h-bottom.ink.h-1)/2;
        center_run({box.x,y,box.w,top.ink.h},top,first_color);
        center_run({box.x,y+top.ink.h+1,box.w,bottom.ink.h},bottom,second_color);
    }
    void center_with_icon(Rect box, Text text, Color color, Image atlas, Rect sprite, int size, bool flip=false) {
        constexpr int gap=4;
        const auto run=fit(text,box.w-size-gap,1);
        if (run.text.size()!=text.size()) {
            center(box,text,color);
            return;
        }
        const int width=size+gap+run.ink.w;
        const int x=box.x+(box.w-width)/2;
        image(atlas,sprite,{x,box.y+(box.h-size)/2,size,size},true,flip);
        label(x+size+gap-run.ink.x,box.y+(box.h-run.ink.h)/2-run.ink.y,text,color);
    }
    int wrap(Rect r, Text text, Color color, int spacing = 17, bool paint = true) {
        int y = r.y;
        while (!text.empty() && y + font.cell <= r.bottom()) {
            Text rest = text;
            int width = 0;
            Text before_last = text;
            while (!rest.empty()) {
                const Text previous = rest;
                const auto c = next_char(rest);
                if (c == '\n') { rest = previous; break; }
                int next = width + *font.glyph(c);
                if (next > r.w) { rest = previous; break; }
                width = next;
                before_last = previous;
            }
            if (!rest.empty() && before_last.size() < text.size()) {
                auto lookahead = rest;
                constexpr std::u32string_view closing = U"、。，！？：）】》”’";
                if (closing.find(next_char(lookahead)) != std::u32string_view::npos)
                    rest = before_last;
            }
            auto slice = text.substr(0,text.size()-rest.size());
            if (paint) label(r.x,y,slice,color,r.w);
            if (rest.size() == text.size()) (void)next_char(rest);
            if (!rest.empty() && rest.front() == '\n') rest.remove_prefix(1);
            text = rest;
            y += std::max(font.cell,spacing-(StandardFace.cell-font.cell));
        }
        return y;
    }
};

} // namespace tide
