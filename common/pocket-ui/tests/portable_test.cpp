// SPDX-License-Identifier: CC0-1.0
#include "../i18n.hpp"
#include "../preferences.hpp"
#include "../viewport.hpp"
#include "pxa_surface_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <utility>

static void test_viewports() {
    struct Case { int dw, dh, w, h, x, y; };
    const Case cases[] = {{296,240,296,230,0,5}, {320,240,308,240,6,0},
        {480,320,411,320,34,0}, {240,240,240,186,0,27},
        {160,128,160,124,0,2}, {800,480,616,480,92,0},
        {1280,720,924,720,178,0}, {1920,1080,1388,1080,266,0}};
    std::vector<uint16_t> source(288*224);
    for (int y=0; y<224; ++y) for (int x=0; x<288; ++x) source[y*288+x]=uint16_t(y*288+x);
    for (const auto &c : cases) {
        auto v = pocket_ui::fit_viewport(c.dw,c.dh);
        assert(v.physical_width()==c.w && v.physical_height()==c.h && v.x==c.x && v.y==c.y);
        assert(v.width<=480 && v.height<=320);
        assert(v.host_scale==int(pxa_surface_fit_scale(v.width,v.height,c.dw,c.dh)));
        std::vector<uint16_t> out(v.pixels()+2, 0xa55a);
        pocket_ui::resample(source.data(),288,224,out.data()+1,v.width,v.height);
        assert(out.front()==0xa55a && out.back()==0xa55a);
        // Every physical pixel maps to exactly the logical pixel drawn there,
        // including repeats introduced by the Host's 2x/4x integer scale.
        for (int py=0; py<c.h; ++py) for (int px=0; px<c.w; ++px) {
            int lx=v.map_x(c.x+px), ly=v.map_y(c.y+py);
            assert(lx>=0 && lx<288 && ly>=0 && ly<224);
            assert(v.contains(c.x+px,c.y+py));
            assert(out[1+(py/v.host_scale)*v.width+px/v.host_scale]==source[ly*288+lx]);
        }
        assert(!v.contains(c.x-1,c.y));
        assert(!v.contains(c.x,c.y-1));
        assert(!v.contains(c.x+c.w,c.y));
        assert(!v.contains(c.x,c.y+c.h));
        assert(v.map_x(c.x-1)<0 && v.map_y(c.y-1)<0);
        assert(v.map_x(c.x+c.w)==288 && v.map_y(c.y+c.h)==224);
    }
    for (int dw : {1,2,7,17,100,240,296,320,480,800,1920,4096,32767})
        for (int dh : {1,3,13,127,224,240,320,720,2160,32767}) {
            auto v=pocket_ui::fit_viewport(dw,dh);
            assert(v.width>0 && v.width<=480 && v.height>0 && v.height<=320);
            assert(v.x>=0 && v.y>=0 && v.x+v.physical_width()<=dw && v.y+v.physical_height()<=dh);
            assert(v.pixels()<=pocket_ui::MAX_SURFACE_PIXELS);
        }
}
static void test_fill() {
    for (auto size : {std::pair<int,int>{240,284},{240,320},{480,320},{320,480},
                      {240,240},{296,240},{800,480},{1280,720},{1920,1080}}) {
        auto v=pocket_ui::fill_viewport(size.first,size.second);
        assert(v.x==0 && v.y==0 && v.physical_width()==size.first && v.physical_height()==size.second);
        assert(v.width<=pocket_ui::MAX_SURFACE_WIDTH && v.height<=pocket_ui::MAX_SURFACE_HEIGHT);
        assert(v.host_scale==int(pxa_surface_fit_scale(v.width,v.height,size.first,size.second)));
        const int logical_w=288,logical_h=int(int64_t(288)*size.second/size.first);
        for(int y=0;y<size.second;++y)for(int x=0;x<size.first;++x){
            assert(v.contains(x,y));
            assert(v.map_x(x,logical_w)==int(int64_t(x/v.host_scale)*logical_w/v.width));
            assert(v.map_y(y,logical_h)==int(int64_t(y/v.host_scale)*logical_h/v.height));
        }
    }
}
static void test_utf8() {
    using namespace pocket_ui;
    assert(text_width(nullptr)==0 && text_width("中",0)==0 && text_width("中",5)==0);
    assert(text_width("Hello")==30 && text_width("中文")==20);
    assert(text_width("A中B")==22 && text_width("A中B",4)==88);
    assert(text_width("中A\nABCD")==24 && text_width("中\n",2)==20);
    const char *s="中A\xf0\x9f\x98\x80";
    assert(next_utf8(s)==0x4e2d && next_utf8(s)=='A' && next_utf8(s)==0x1f600 && next_utf8(s)==0);
    assert(text_width("\xff")==10 && text_width("\xf0")==10);
    assert(text_width("\xc0\xaf")==20 && text_width("\xed\xa0\x80")==30);
    assert(text_width("\xe0\x80\x80")==30 && text_width("\xf4\x90\x80\x80")==40);
    assert(glyph(0x4e2d) && !glyph(0x1f600));
    uint16_t bitmap[40*40]{};
    draw_text(0,0,"A中\nB",1,[&](int x,int y){ assert(x>=0 && x<16 && y>=0 && y<17); bitmap[y*40+x]=1; });
    unsigned ascii=0,cjk=0,second=0;
    for (int y=0;y<40;++y) for(int x=0;x<40;++x) if(bitmap[y*40+x]) {
        if(y>=10) ++second; else if(x<6) ++ascii; else ++cjk;
    }
    assert(ascii>0 && cjk>0 && second>0);
    unsigned unknown=0, space=0;
    draw_text(0,0,"\xf0\x9f\x98\x80",1,[&](int x,int y){assert(x==0||x==9||y==0||y==9);++unknown;});
    draw_text(0,0," ",1,[&](int,int){++space;});
    assert(unknown==36 && space==0);
    // The common em baseline keeps 一 in the middle and 。 at the bottom.
    // Per-character ink-top alignment would incorrectly raise both to row 0.
    const uint16_t *one=glyph(0x4e00), *period=glyph(0x3002);
    assert(one && period && !one[0] && !one[1] && !one[2]);
    assert(one[4] || one[5] || one[6]);
    assert(!period[0] && !period[1] && !period[2] && !period[3] && !period[4]);
    assert(period[7] || period[8] || period[9]);
    uint32_t previous=0;
    for (const auto &g : cjk_glyphs) {
        assert(g.codepoint>previous); previous=g.codepoint;
        bool ink=false;
        for (uint16_t row : g.rows) {assert(row<=0x3ff);ink|=row!=0;}
        assert(ink && glyph(g.codepoint)==g.rows);
    }
}
static void test_locale() {
    using namespace pocket;
    assert(!english() && strcmp(tr("EN","中文"),"中文")==0);
    set_english(true); assert(english() && strcmp(tr("EN","中文"),"EN")==0);
    uint8_t data[4]; pocket_ui::encode_locale(data,locale);
    Locale decoded=Locale::ZH;
    assert(pocket_ui::decode_locale(data,sizeof(data),decoded) && decoded==Locale::EN);
    data[3]=2; assert(!pocket_ui::decode_locale(data,sizeof(data),decoded));
    data[3]=0; data[2]=2; assert(!pocket_ui::decode_locale(data,sizeof(data),decoded));
    assert(!pocket_ui::decode_locale(nullptr,4,decoded));
    assert(!pocket_ui::decode_locale(data,3,decoded));
    set_english(false);
}
int main() {
    for (uint32_t density : {65536u, 98304u, 124928u, 131072u, 196608u}) {
        for (int origin = 0; origin < 480; ++origin) for (int length = 1; length <= 480 - origin; ++length) {
            auto cover = pocket_ui::cover_pixel_span(origin, length, density);
            // Match the Host's independent floor conversions, rather than
            // converting an endpoint using the same helper under test.
            const int first = int(int64_t(cover.origin) * density / 65536);
            const int count = int(int64_t(cover.length) * density / 65536);
            assert(first <= origin && first + count >= origin + length);
        }
    }
    test_viewports(); test_fill(); test_utf8(); test_locale();
    puts("Pocket UI: viewport / inverse touch / resampling / UTF-8 / glyphs / locale passed");
}
