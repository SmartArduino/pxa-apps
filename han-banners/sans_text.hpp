#pragma once

#include <array>
#include <cmath>
#include <pxa/game_upload.hpp>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace han {
#include "sans_font_bytes.hpp"

class SansText {
public:
    static constexpr int banks=3;
    struct Glyph {
        int index=0, pixels=0;
        int left=0, top=0, width=0, height=0;
        int bank=0, x=0, y=0;
    };

    bool init() {return stbtt_InitFont(&font,sansFontBytes,0)!=0;}

    int advance(uint32_t code,int pixels) const {
        int width=0;
        stbtt_GetGlyphHMetrics(&font,index(code),&width,nullptr);
        return int(std::round(width*em(pixels)));
    }

    void reset() {
        count=0;
        for (auto& bank:pages) {
            bank.x=bank.y=bank.row=0;
            bank.dirty=false;
        }
    }

    const Glyph* glyph(uint32_t code,int pixels,bool antialias) {
        const int id=index(code);
        for (int i=0;i<count;++i)
            if (entries[i].index==id && entries[i].pixels==pixels) return &entries[i];
        if (count==int(entries.size())) return nullptr;

        auto& entry=entries[count];
        entry={.index=id,.pixels=pixels};
        const float scale=em(pixels);
        int right=0,bottom=0;
        stbtt_GetGlyphBitmapBox(&font,id,scale,scale,&entry.left,&entry.top,&right,&bottom);
        entry.width=right-entry.left;
        entry.height=bottom-entry.top;
        if (!entry.width || !entry.height) return nullptr;
        for (int i=0;i<banks;++i) {
            auto& bank=pages[i];
            if (!bank.place(entry)) continue;
            entry.bank=i;
            auto dst=bank.texels().subspan(entry.y*edge+entry.x);
            auto* bytes=reinterpret_cast<unsigned char*>(dst.data());
            stbtt_MakeGlyphBitmap(&font,bytes,entry.width,entry.height,edge,scale,scale,id);
            if (!antialias) {
                for (int y=0;y<entry.height;++y)
                    for (int x=0;x<entry.width;++x)
                        bytes[y*edge+x]=bytes[y*edge+x]>=125?255:0;
            }
            ++count;
            return &entry;
        }
        return nullptr;
    }

    pxa::Result<void> upload(pxa::game::Renderer& renderer,int first) {
        for (int i=0;i<banks;++i) {
            auto& bank=pages[i];
            if (!bank.dirty) continue;
            const int height=bank.y+bank.row;
            auto result=pxa::game::Upload(renderer,bank.data).texture({uint8_t(first+i)},edge,
                                        uint16_t(height),bank.texels().first(edge*height));
            if (!result) return result;
            bank.dirty=false;
        }
        return {};
    }

private:
    static constexpr int edge=256;
    struct Bank {
        std::array<std::byte,edge*edge+pxa::game::Upload::header_bytes> data{};
        int x=0, y=0, row=0;
        bool dirty=false;

        std::span<std::byte> texels() {
            return std::span(data).subspan<pxa::game::Upload::header_bytes>();
        }

        bool place(Glyph& entry) {
            const bool wrap=x+entry.width>edge;
            const int nextY=wrap?y+row:y;
            if (entry.width>edge || nextY+entry.height>edge) return false;
            if (wrap) {x=0;y=nextY;row=0;}
            entry.x=x;
            entry.y=y;
            x+=entry.width;
            row=std::max(row,entry.height);
            dirty=true;
            return true;
        }
    };

    int index(uint32_t code) const {
        const int id=stbtt_FindGlyphIndex(&font,int(code));
        return id?id:stbtt_FindGlyphIndex(&font,'?');
    }
    float em(int pixels) const {return stbtt_ScaleForMappingEmToPixels(&font,float(pixels));}

    stbtt_fontinfo font{};
    std::array<Bank,banks> pages{};
    std::array<Glyph,512> entries{};
    int count=0;
};
}
