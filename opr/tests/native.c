/* SPDX-License-Identifier: MIT. Real core, frontend and ordinary-action campaign. */
#include "ui.h"
#include "portable-rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static int checks;
#define CHECK(c) do{checks++;if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
static void capture(const opr_ui *u,const char *name) {
    unsigned char pixels[400*416];opr_ui_render(u,pixels);FILE *f=fopen(name,"wb");CHECK(f!=NULL);fprintf(f,"P6\n400 416\n255\n");
    for(unsigned i=0;i<sizeof pixels;i++){CHECK(pixels[i]<16);uint16_t c=opr_ui_palette[pixels[i]];unsigned char p[3]={(unsigned char)(((c>>11)&31)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};CHECK(fwrite(p,1,3,f)==3);}CHECK(fclose(f)==0);
}
static int distance_map(const OPR_GameState *g,int allow_end,int dist[256],int first[256]) {
    int queue[256],head=0,tail=0;for(int i=0;i<256;i++){dist[i]=-1;first[i]=-1;}
    int from=g->player.y*16+g->player.x;queue[tail++]=from;dist[from]=0;
    while(head<tail){int at=queue[head++];for(int a=0;a<4;a++){
        int x=at%16+dx[a],y=at/16+dy[a],blocked=0;if(x<0||x>=16||y<0||y>=16||g->map.tiles[y][x])continue;
        if(!allow_end)for(int i=0;i<g->entityCount;i++)if(g->entities[i].x==x&&g->entities[i].y==y&&(g->entities[i].type==OPR_STAIRS||g->entities[i].type==OPR_BOSS))blocked=1;
        int next=y*16+x;if(blocked||dist[next]>=0)continue;dist[next]=dist[at]+1;first[next]=at==from?a:first[at];queue[tail++]=next;
    }}return tail;
}
static void invariants(const OPR_GameState *g) {
    CHECK(g->player.x>=0&&g->player.x<16&&g->player.y>=0&&g->player.y<16);
    CHECK(!g->map.tiles[g->player.y][g->player.x]);CHECK(!g->map.fog[g->player.y][g->player.x]);CHECK(g->entityCount>=1&&g->entityCount<=64);
    int dist[256],first[256],floors=0,ends=0;int connected=distance_map(g,1,dist,first);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++)floors+=!g->map.tiles[y][x];CHECK(connected==floors);
    for(int i=0;i<g->entityCount;i++){
        const OPR_Entity *e=&g->entities[i];CHECK(e->x>=0&&e->x<16&&e->y>=0&&e->y<16);CHECK(!g->map.tiles[e->y][e->x]);CHECK(e->x!=g->player.x||e->y!=g->player.y);
        for(int j=0;j<i;j++)CHECK(e->x!=g->entities[j].x||e->y!=g->entities[j].y);
        ends+=(e->type==(g->player.floor<5?OPR_STAIRS:OPR_BOSS));
    }CHECK(ends==1);
}
static int touch_for_action(opr_ui *u,int a) {static const int x[]={330,330,285,375,330,330,330},y[]={138,230,185,185,288,333,378};unsigned before=u->session.actions;opr_ui_pointer(u,x[a],y[a]);return u->session.actions!=before;}
static int campaign(uint32_t seed,int cls,int save) {
    opr_ui u;opr_ui_init(&u);u.seed=seed;opr_ui_pointer(&u,120,150+cls*50);
    CHECK(u.session.active&&u.scene==OPR_UI_PLAY&&u.session.game.player.classId==cls);
    FILE *actions=save?fopen("campaign-actions.txt","w"):NULL;if(save)CHECK(actions!=NULL);
    int maxfloor=1,kills=0,potions=0,floors=0,last_count=u.session.game.entityCount;
    for(int step=0;step<8000&&!u.session.game.gameOver&&!u.session.game.victory;step++){
        OPR_GameState *g=&u.session.game;int a=-1;
        if(g->inCombat){
            OPR_Entity *e=&g->entities[g->combatIndex];int incoming=e->atk-(g->player.def+g->player.armorDef)+1;if(incoming<1)incoming=1;
            int minhit=g->player.atk+g->player.weaponAtk-e->def;if(minhit<1)minhit=1;
            if(e->hp>minhit&&g->player.potions>0&&g->player.hp<=g->player.maxHp-(8+g->player.level*2)&&g->player.hp<=incoming*3+2){a=OPR_ACT_POTION;potions++;}
            else a=OPR_ACT_ATTACK;
        }else{
            int dist[256],first[256],best=100000,target=-1;
            for(int pass=0;pass<2&&target<0;pass++){
                distance_map(g,pass,dist,first);
                for(int i=0;i<g->entityCount;i++){
                    OPR_Entity *e=&g->entities[i];int at=e->y*16+e->x;if(dist[at]<1)continue;
                    int priority=(e->type==OPR_ENEMY?10000:(e->type==OPR_STAIRS||e->type==OPR_BOSS)?20000:0);
                    int score=priority+dist[at]*10+(e->type==OPR_SHRINE?0:e->type==OPR_CHEST?1:2);
                    if(score<best){best=score;target=i;a=first[at];}
                }
            }
        }
        if(a<0||a>6){if(actions)fclose(actions);return 0;}
        if(actions)fprintf(actions,"%d\n",a);
        int wascombat=g->inCombat,oldfloor=g->player.floor,oldcount=g->entityCount;
        if(!touch_for_action(&u,a)){if(actions)fclose(actions);return 0;}
        if(wascombat&&!g->inCombat&&g->entityCount<oldcount)kills++;
        if(g->player.floor!=oldfloor){floors++;if(save)printf("CAMPAIGN floor=%d hp=%d level=%d potions=%d actions=%u\n",g->player.floor,g->player.hp,g->player.level,g->player.potions,u.session.actions);}
        if(g->player.floor>maxfloor)maxfloor=g->player.floor;last_count=g->entityCount;
    }
    if(actions)CHECK(fclose(actions)==0);
    if(save){printf("CAMPAIGN_RESULT seed=%u class=%d victory=%d dead=%d maxfloor=%d level=%d hp=%d actions=%u kills=%d potion_actions=%d descents=%d hash=%u entities=%d state_injections=0\n",seed,cls,u.session.game.victory,u.session.game.gameOver,maxfloor,u.session.game.player.level,u.session.game.player.hp,u.session.actions,kills,potions,floors,opr_state_hash(&u.session.game),last_count);capture(&u,"campaign-final.ppm");}
    return u.session.game.victory;
}
static void api_fixtures(void) {
    opr_session s;opr_session_start(&s,0,1);OPR_GameState *g=&s.game;
    memset(g,0,sizeof(*g));g->player.hp=g->player.maxHp=30;g->player.level=1;g->player.xpNext=10;g->player.atk=5;g->player.def=3;
    g->player.x=1;g->player.y=1;g->entityCount=1;g->entities[0]=(OPR_Entity){.x=2,.y=1,.type=OPR_POTION};
    CHECK(opr_session_action(&s,OPR_ACT_RIGHT));CHECK(g->player.potions==1&&g->entityCount==0&&g->player.x==2);
    g->entities[0]=(OPR_Entity){.x=3,.y=1,.type=OPR_SHRINE};g->entityCount=1;g->player.hp=1;
    CHECK(opr_session_action(&s,OPR_ACT_RIGHT));CHECK(g->player.hp==30&&g->player.atk==6&&g->player.def==4);
    g->player.xp=26;opr_check_levelup(g);CHECK(g->player.level==3&&g->player.xp==0&&g->player.maxHp==38&&g->player.hp==38);
    g->player.floor=1;g->player.hp=1;opr_descend(g);CHECK(g->player.floor==2&&g->player.hp==9);invariants(g);
    memset(g,0,sizeof(*g));g->player.hp=1;g->player.maxHp=30;g->player.level=1;g->player.xpNext=10;g->inCombat=1;g->combatIndex=0;g->entityCount=1;g->entities[0]=(OPR_Entity){.type=OPR_ENEMY,.hp=100,.atk=100};
    CHECK(opr_session_action(&s,OPR_ACT_ATTACK));CHECK(g->gameOver&&g->player.hp==0&&!g->inCombat);
    memset(g,0,sizeof(*g));g->player.hp=5;g->player.maxHp=30;g->player.level=1;g->player.xpNext=999;g->player.potions=1;
    g->inCombat=1;g->combatIndex=0;g->entityCount=1;g->entities[0]=(OPR_Entity){.type=OPR_ENEMY,.hp=100,.atk=2};
    CHECK(opr_session_action(&s,OPR_ACT_POTION));CHECK(g->player.potions==0&&g->player.hp>=12&&g->player.hp<=13&&g->inCombat);
    uint32_t unchanged=opr_state_hash(g);CHECK(!opr_session_action(&s,OPR_ACT_POTION));CHECK(unchanged==opr_state_hash(g));
    g->player.hp=30;g->player.spd=0;opr_rng_seed(1);CHECK(opr_session_action(&s,OPR_ACT_FLEE));CHECK(!g->inCombat&&g->entityCount==1);
    g->inCombat=1;g->combatIndex=0;opr_rng_seed(UINT32_C(0x12345678));CHECK(opr_session_action(&s,OPR_ACT_FLEE));CHECK(g->inCombat&&g->player.hp<30);
    for(int branch=0;branch<4;branch++){
        uint32_t seed=1;for(;seed<1000;seed++){opr_rng_seed(seed);if(opr_rand()%4==branch)break;}CHECK(seed<1000);
        memset(g,0,sizeof(*g));g->player.hp=g->player.maxHp=30;g->player.level=1;g->player.xpNext=999;g->player.x=1;g->player.y=1;
        g->entityCount=1;g->entities[0]=(OPR_Entity){.x=2,.y=1,.type=OPR_CHEST};opr_rng_seed(seed);CHECK(opr_session_action(&s,OPR_ACT_RIGHT));
        CHECK(g->entityCount==0&&g->player.x==2);
        if(branch==0)CHECK(g->player.weaponAtk==1);else if(branch==1)CHECK(g->player.armorDef==1);else if(branch==2)CHECK(g->player.potions>=1&&g->player.potions<=2);else CHECK(g->player.xp>=5&&g->player.xp<=10);
    }
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    printf("sizeof_game=%zu sizeof_session=%zu sizeof_ui=%zu\n",sizeof(OPR_GameState),sizeof(opr_session),sizeof(opr_ui));
    for(uint32_t seed=1;seed<=256;seed++)for(int cls=0;cls<3;cls++){
        opr_session s;opr_session_start(&s,cls,seed);for(int floor=1;floor<=5;floor++){if(floor>1)opr_descend(&s.game);invariants(&s.game);}
    }
    puts("PASS map invariants over 256 seeds x 3 classes x 5 floors (fixtures, not campaigns)");
    api_fixtures();puts("PASS focused interaction/progression/death fixtures");
    opr_ui u;opr_ui_init(&u);capture(&u,"select.ppm");opr_ui_pointer(&u,120,150);uint32_t initial=opr_state_hash(&u.session.game);CHECK(u.scene==OPR_UI_PLAY);printf("INITIAL seed=1 class=0 hash=%u x=%d y=%d hp=%d entities=%d\n",initial,u.session.game.player.x,u.session.game.player.y,u.session.game.player.hp,u.session.game.entityCount);capture(&u,"play.ppm");
    opr_ui_pointer(&u,350,20);CHECK(u.scene==OPR_UI_PAUSE);for(int i=0;i<32;i++){opr_ui_pointer(&u,330,138);opr_ui_controller(&u,1);opr_ui_controller(&u,0);}CHECK(initial==opr_state_hash(&u.session.game));capture(&u,"pause.ppm");opr_ui_pointer(&u,150,175);CHECK(u.scene==OPR_UI_PLAY);
    opr_ui_background(&u,1);for(int i=0;i<32;i++){opr_ui_pointer(&u,330,138);opr_ui_controller(&u,1);}CHECK(initial==opr_state_hash(&u.session.game));opr_ui_background(&u,0);
    opr_ui_controller(&u,1);uint32_t held=opr_state_hash(&u.session.game);opr_ui_controller(&u,1);CHECK(held==opr_state_hash(&u.session.game));opr_ui_controller(&u,0);
    opr_ui_pointer(&u,50,20);uint32_t paused=opr_state_hash(&u.session.game),rng=opr_rng_state();opr_ui_pointer(&u,80,305);opr_ui_pointer(&u,90,360);CHECK(u.scene==OPR_UI_PLAY&&paused==opr_state_hash(&u.session.game)&&rng==opr_rng_state());
    opr_ui_pointer(&u,50,20);opr_ui_pointer(&u,270,305);opr_ui_pointer(&u,120,150);CHECK(initial==opr_state_hash(&u.session.game));puts("PASS production touch/controller edge, pause, background, resume, replay");
    uint32_t winning=0;int winning_class=0;
    for(uint32_t seed=1;seed<=256&&!winning;seed++)for(int cls=0;cls<3&&!winning;cls++)if(campaign(seed,cls,0)){winning=seed;winning_class=cls;}
    CHECK(winning!=0);CHECK(campaign(winning,winning_class,1));printf("PASS campaign seed=%u class=%d; raw CHECK count=%d includes image IO/per-pixel bounds, not independent gameplay tests\n",winning,winning_class,checks);
    return 0;
}
