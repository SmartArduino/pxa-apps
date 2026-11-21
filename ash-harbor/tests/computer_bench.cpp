// SPDX-License-Identifier: MIT
// Same deterministic opponents/seeds can be compiled against an older combat header.
#ifndef ASH_COMBAT_HEADER
#define ASH_COMBAT_HEADER "../combat.hpp"
#endif
#include ASH_COMBAT_HEADER
#include <cstdio>
using namespace ash;
struct Stats {int damage=0,taken=0,blocks=0,combo=0,fire=0,rush=0,throwing=0,rise=0;};
int main(){
    std::puts("{\"levels\":[");
    for(int level=0;level<3;++level){if(level)std::puts(",");std::puts("[");
        for(int c=0;c<4;++c){Stats s;
            for(int bot=0;bot<4;++bot)for(unsigned seed=1;seed<=12;++seed){
                Arena a;a.reset(false,bot,c);a.difficulty=Difficulty(level);a.rng=seed*19073;
                Action previous=Action::idle;
                for(int n=0;n<2400;++n){
                    if(a.phase==Phase::match_end)break;
                    auto& p=a.f[0];auto& q=a.f[1];uint32_t toward=p.x<q.x?Right:Left,buttons=0;
                    const float d=std::abs(p.x-q.x);
                    if(bot==1)buttons=(d>30?toward:0u)|(n%28==0?uint32_t(Punch):0u);
                    if(bot==2)buttons=(d<90?(toward==Right?Left:Right):0u)|(n%75==0?uint32_t(Skill):0u);
                    if(bot==3)buttons=(d>38?toward:0u)|(n%85==0?uint32_t(Up):0u)|(!p.grounded()&&p.vy<2&&n%10==0?uint32_t(Kick):0u);
                    a.feed(0,buttons);int hp0=p.hp,hp1=q.hp,blocks=a.blocks;a.step();
                    s.damage+=std::max(0,hp0-p.hp);s.taken+=std::max(0,hp1-q.hp);s.blocks+=a.blocks-blocks;
                    s.combo=std::max(s.combo,q.combo);
                    if(q.action!=previous){s.fire+=q.action==Action::fire;s.rush+=q.action==Action::rush;
                        s.throwing+=q.action==Action::throwing;s.rise+=q.action==Action::rise;}
                    previous=q.action;
                }
            }
            if(c)std::puts(",");
            std::printf("{\"character\":%d,\"damage\":%d,\"taken\":%d,\"blocks\":%d,\"max_combo\":%d,\"fire\":%d,\"rush\":%d,\"throws\":%d,\"rise\":%d}",c,s.damage,s.taken,s.blocks,s.combo,s.fire,s.rush,s.throwing,s.rise);
        }std::puts("]");
    }std::puts("]}");
}
