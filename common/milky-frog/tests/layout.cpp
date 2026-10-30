// SPDX-License-Identifier: MIT
#include "../layout.hpp"
#include <assert.h>
#include <stdio.h>
using namespace milky;
static void inside(Rect r){auto b=bounds();assert(r.w>0&&r.h>0&&r.x>=b.x&&r.y>=b.y&&r.right()<=b.right()&&r.bottom()<=b.bottom());}
static bool overlaps(Rect a,Rect b){return a.x<b.right()&&a.right()>b.x&&a.y<b.bottom()&&a.bottom()>b.y;}
int main(){
    const int sizes[][2]={{200,200},{240,240},{296,240},{480,320},{480,480},{800,480},{390,844},{1920,1080},{412,412}};
    for(const auto &size:sizes)for(int shape=0;shape<3;++shape){if(shape==2&&std::min(size[0],size[1])<320)continue;
        fprintf(stderr,"layout %dx%d shape=%d\n",size[0],size[1],shape);int insets[]={0,0,0,0};if(shape==1){insets[0]=12;insets[2]=8;}
        screen=fit_screen(size[0],size[1],insets,shape==1?20:0,shape==2);
        assert(screen.width<=480&&screen.height<=480&&screen.width*screen.height<=480*320);
        inside(menu_button());inside(back_button());inside(pause_panel());
        auto s=settings_layout();for(auto r:{s.effects,s.music,s.preview,s.back})inside(r);
        auto b=board_layout();inside(b.board);inside(b.score);inside(b.best);assert(b.board.w>=75);
        for(auto a:b.actions){inside(a);assert(!overlaps(a,b.board));}assert(!overlaps(b.score,b.board)&&!overlaps(b.best,b.board));
        for(bool miner:{false,true}){auto h=home_layout(miner);for(auto r:{h.hero,h.copy,h.start,h.secondary[0],h.secondary[1]})inside(r);if(miner)inside(h.difficulty);assert(!overlaps(h.hero,h.start));}
        auto g=gallery_layout();for(int i=0;i<g.rows*g.columns;++i)inside(gallery_cell(g,i));inside(g.previous);inside(g.next);
    }
    puts("PASS: responsive geometry at nine sizes, safe insets and round screens; controls remain inside and separate from the board.");
}
