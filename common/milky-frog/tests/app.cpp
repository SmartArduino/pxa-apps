// SPDX-License-Identifier: MIT
// Real rendering and touch hooks, with only the Host transport replaced.
#include "../runtime.cpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern "C" int32_t pxa_call(uint32_t,uint64_t,const uint8_t *,uint32_t){return 0;}
extern "C" int32_t pxa_io(uint64_t,uint32_t,uint8_t *,uint32_t){return PXA_STATUS_WOULD_BLOCK;}
static void draw_checked(){
    canvas=logical_frame;system_text_begin();game::draw();
    assert(system_text_count>0&&system_text_count<96);
    for(unsigned i=0;i<system_text_count;++i){const auto &t=system_texts[i];if(!t.visible)continue;
        assert(t.x>=0&&t.x+t.w<=pocket::WIDTH&&t.y>=0&&t.y<pocket::HEIGHT);
        const char *p=t.text;while(*p)assert(pocket_ui::next_utf8(p)!=0xfffd);
    }
    assert(system_text_present());canvas=nullptr;
}
int main(){
    running=true;background=paused=exit_requested=false;epoch=last_tick=now_us();
    uint8_t initial[3900],saved[3900],after[3900];game::init();draw_checked();size_t n=game::serialize(initial,sizeof(initial));assert(n&&n<=960);
    game::pointer(pocket::Phase::DOWN,100,100);game::pointer(pocket::Phase::CANCEL,100,100);game::pointer(pocket::Phase::UP,150,100);assert(game::serialize(after,sizeof(after))==n&&!memcmp(initial,after,n));
    for(unsigned i=0;i<1000;++i){game::key(pocket::Key(i%4));game::tick(20);if(i%50==0)draw_checked();}
    game::serialize(saved,sizeof(saved));game::reset();game::load(saved,n);assert(game::serialize(after,sizeof(after))==n&&!memcmp(saved,after,n));
    for(size_t i=0;i<n;++i){uint8_t invalid[3900];memcpy(invalid,saved,n);invalid[i]^=0x80;game::load(invalid,n);game::serialize(after,sizeof(after));assert(!memcmp(saved,after,n));}
    const int sizes[][2]={{200,200},{240,240},{296,240},{480,320},{480,480},{800,480},{390,844}};
    uint32_t safe[4]{},corners[4]{};
    for(const auto &size:sizes){milky::configure_screen(size[0],size[1],65536,safe,0,corners);viewport=pocket_ui::fit_viewport(size[0],size[1],pocket::WIDTH,pocket::HEIGHT);
        draw_checked();game::key(pocket::Key::Y);draw_checked();game::key(pocket::Key::B);game::reset();draw_checked();
    }
    printf("PASS: %s real rendering at seven sizes, UTF-8 labels, pointer cancellation, save roundtrip and byte corruption.\n",game::app_id());
}
