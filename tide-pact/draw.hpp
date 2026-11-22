#pragma once

#include "assets.hpp"
#include "game.hpp"
#include <pxa/pixels.hpp>

#include <algorithm>
#include <cmath>

namespace tide {

using Color = std::uint16_t;
constexpr Color rgb(unsigned v) {
    return ((v >> 8) & 0xf800) | ((v >> 5) & 0x07e0) | ((v >> 3) & 0x001f);
}

struct Rect {
    int x, y, w, h;
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

struct Draw {
    struct TextRun {
        Text text;
        Rect ink;
    };

    pxa::Rgb565Pixels pixels;
    Rect clip;
    const FontFace& font;

    explicit Draw(pxa::Rgb565Pixels image, const FontFace& face=StandardFace)
        : pixels(image), clip{0,0,int(image.width()),int(image.height())}, font(face) {}

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
        if (clip.contains({x,y})) (*pixels.row(y))[x] = color;
    }
    void fill(Rect rect, Color color) {
        const auto r = clipped(rect);
        if (!r.w) return;
        for (int y = r.y; y < r.bottom(); ++y) {
            auto row = *pixels.row(y);
            for (int x = r.x; x < r.right(); ++x) row[x] = color;
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
    void image(Image img, Rect src, Rect dst, bool transparent = false, bool flip = false) {
        if (dst.w<=0 || dst.h<=0 || src.w<=0 || src.h<=0) return;
        const auto r = clipped(dst);
        if (!r.w || !r.h) return;
        const int initial=(r.x-dst.x)*src.w;
        const int first=initial/dst.w, first_error=initial%dst.w;
        const int pitch=src.w/dst.w, step=src.w%dst.w;
        const int stride=img.width();
        const bool packed=img.packed();
        const bool distant=packed && dst.w*2<src.w;
        for (int y = r.y; y < r.bottom(); ++y) {
            const int sy = src.y + (y-dst.y)*src.h/dst.h;
            const int line=sy*stride;
            const bool latitude_line=packed && (sy-ChartNorth*ChartLatitudeScale)%(10*ChartLatitudeScale)==0;
            const auto* indexes=img.bytes+516;
            auto row = *pixels.row(y);
            int source=first, error=first_error;
            for (int x = r.x; x < r.right(); ++x) {
                const int sx=src.x+(flip?src.w-1-source:source), offset=line+sx;
                unsigned index;
                if (packed) {
                    const bool grid=latitude_line || (sx+ChartWest*ChartLongitudeScale)%(10*ChartLongitudeScale)==0;
                    const unsigned grain=unsigned(sx)*0x9e3779b1U ^ unsigned(sy)*0x85ebca77U;
                    index=Image::packed_terrain(indexes,offset)*4+(grid?3:distant?1:grain>>31);
                } else index=indexes[offset];
                if (!transparent || index != 255) row[x] = img.color(index);
                source+=pitch;
                error+=step;
                if (error>=dst.w) { error-=dst.w; ++source; }
            }
        }
    }
    void nine_slice(Image img, Rect src, Rect dst, int source_edge, int edge, bool transparent = false) {
        edge=std::min({edge,dst.w/3,dst.h/3});
        if (edge<1) return;
        const int x[]{dst.x,dst.x+edge,dst.right()-edge,dst.right()};
        const int y[]{dst.y,dst.y+edge,dst.bottom()-edge,dst.bottom()};
        const int u[]{src.x,src.x+source_edge,src.right()-source_edge,src.right()};
        const int v[]{src.y,src.y+source_edge,src.bottom()-source_edge,src.bottom()};
        for (int j=0; j<3; ++j)
            for (int i=0; i<3; ++i)
                image(img,{u[i],v[j],u[i+1]-u[i],v[j+1]-v[j]},
                      {x[i],y[j],x[i+1]-x[i],y[j+1]-y[j]},transparent);
    }
    void cover(Image img, Rect dst) {
        const int w = std::min(img.width(),img.height()*dst.w/std::max(1,dst.h));
        const int h = std::min(img.height(),img.width()*dst.h/std::max(1,dst.w));
        image(img,{(img.width()-w)/2,(img.height()-h)/2,w,h},dst);
    }
    void darken(Rect box, int strength) {
        auto r = clipped(box);
        for (int y = r.y; y < r.bottom(); ++y) {
            auto row = *pixels.row(y);
            for (int x = r.x; x < r.right(); ++x) row[x] = mix(row[x], rgb(0x08181d), strength);
        }
    }
    static Color mix(Color a, Color b, int opacity) {
        const int inv = 15-opacity;
        const auto blend = [&](int shift, int mask) {
            return ((((a >> shift) & mask)*inv + ((b >> shift) & mask)*opacity + 7) / 15) << shift;
        };
        return blend(11,31) | blend(5,63) | blend(0,31);
    }
    void label(int x, int y, Text text, Color color, int max_width = 10000, int scale = 1) {
        const int end = x+max_width;
        while (!text.empty()) {
            const auto code = next_char(text);
            const auto* g = font.glyph(code);
            const int advance = *g++ * scale;
            if (x+advance > end) return;
            if (code != ' ') {
                for (int j = 0; j < font.cell; ++j) {
                    for (int i = 0; i < font.cell; ++i) {
                        const auto packed = g[j*font.stride() + i/2];
                        const int alpha = i % 2 ? packed & 15 : packed >> 4;
                        if (!alpha) continue;
                        auto r = clipped({x+i*scale,y+j*scale,scale,scale});
                        if (!r.w) continue;
                        for (int yy = r.y; yy < r.bottom(); ++yy) {
                            auto row = *pixels.row(yy);
                            for (int xx = r.x; xx < r.right(); ++xx)
                                row[xx] = alpha == 15 ? color : mix(row[xx],color,alpha);
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
    void center_with_icon(Rect box, Text text, Color color, Image atlas, Rect sprite, int size) {
        constexpr int gap=4;
        const auto run=fit(text,box.w-size-gap,1);
        if (run.text.size()!=text.size()) {
            center(box,text,color);
            return;
        }
        const int width=size+gap+run.ink.w;
        const int x=box.x+(box.w-width)/2;
        image(atlas,sprite,{x,box.y+(box.h-size)/2,size,size},true);
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
