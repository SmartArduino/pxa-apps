/* SPDX-License-Identifier: MIT. Asset-free PXA UI; OPR provides actual world state. */
#include "ui.h"
#include <string.h>
#include "font.h"
enum { W=400,H=416, BG=0, PANEL=1, WHITE=2, DIM=3, WALL=4, FLOOR=5, HERO=6, RED=7, GOLD=8, PURPLE=9, GREEN=10, BLUE=11, BLACK=12, GHOST=13 };
const uint16_t opr_ui_palette[16]={0x0842,0x1947,0xffff,0x9cf3,0x3a2c,0x3186,0x07ff,0xf9c7,0xfde0,0xc29f,0x47e8,0x5c9f,0x0000,0x1083,0x7bef,0xffff};
static unsigned char *out;
static void rect(int x,int y,int w,int h,unsigned char c) { for(int j=y;j<y+h;j++) if(j>=0&&j<H) for(int i=x;i<x+w;i++)if(i>=0&&i<W)out[j*W+i]=c; }
static void text(int x,int y,const char *s,int scale,int c) {
    for(;*s;s++,x+=6*scale) {
        unsigned k=*s>='0'&&*s<='9'?*s-'0':*s>='A'&&*s<='Z'?10+*s-'A':36;
        if(k<36) { for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(letters[k][i]&(1<<j))rect(x+i*scale,y+j*scale,scale,scale,c); }
        else if(*s=='-') rect(x,y+3*scale,5*scale,scale,c);
        else if(*s=='+') {rect(x,y+3*scale,5*scale,scale,c);rect(x+2*scale,y+scale,scale,5*scale,c);}
        else if(*s=='/')for(int i=0;i<5;i++)rect(x+i*scale,y+(5-i)*scale,scale,scale,c);
    }
}
static int number(int x,int y,uint32_t n,int scale,int c) {char d[11];unsigned i=10;d[10]=0;do{d[--i]=(char)('0'+n%10);n/=10;}while(n);text(x,y,d+i,scale,c);return x+(10-(int)i)*6*scale;}
static void button(int x,int y,int w,int h,const char*s,int enabled) {rect(x,y,w,h,enabled?PANEL:GHOST);text(x+(w-(int)strlen(s)*12)/2,y+(h-14)/2,s,2,enabled?WHITE:DIM);}
static void stat(int x,int y,const char*s,int a,int b) { text(x,y,s,1,DIM);x+=(int)strlen(s)*6+6;x=number(x,y,a<0?0:(uint32_t)a,1,WHITE);if(b>=0){text(x,y,"/",1,DIM);number(x+6,y,(uint32_t)b,1,WHITE);} }
static void tile_icon(int type,int x,int y) {
    if(type==OPR_ENEMY||type==OPR_BOSS){rect(x+3,y+3,9,9,RED);rect(x+5,y+5,2,2,BLACK);rect(x+9,y+5,2,2,BLACK);if(type==OPR_BOSS){rect(x+2,y+1,2,4,GOLD);rect(x+6,y,3,4,GOLD);rect(x+11,y+1,2,4,GOLD);}}
    else if(type==OPR_CHEST){rect(x+2,y+4,11,8,GOLD);rect(x+2,y+7,11,1,BLACK);rect(x+7,y+6,2,4,WHITE);}
    else if(type==OPR_POTION){rect(x+6,y+1,3,3,WHITE);rect(x+4,y+4,7,8,PURPLE);rect(x+5,y+5,2,4,WHITE);}
    else if(type==OPR_STAIRS){for(int i=0;i<4;i++)rect(x+2+i*2,y+2+i*3,10-i*2,2,WHITE);}
    else if(type==OPR_SHRINE){rect(x+6,y+2,3,11,GREEN);rect(x+2,y+6,11,3,GREEN);}
}
static void help(void) {
    rect(15,24,370,368,PANEL);text(62,40,"HOW TO PLAY",3,WHITE);
    const char *lines[]={"WARRIOR ROGUE OR MAGE - FIVE FLOORS","MOVE ONE TILE WITH U D L R","WALK INTO ENEMIES TO ENTER COMBAT","ATTACK - EXCHANGE BLOWS","POTION - HEAL THEN ENEMY ATTACKS","FLEE - SPEED BASED ESCAPE CHANCE","CHESTS GIVE GEAR POTIONS OR EXPERIENCE","SHRINES FULLY HEAL AND RAISE STATS","STAIRS DESCEND AND PARTLY HEAL","DEFEAT THE FLOOR FIVE BOSS TO WIN","PAUSE AND BACKGROUND STOP ALL ACTIONS","ORIGINAL OPR CORE - MIT - ONDA DEV"};
    for(unsigned i=0;i<sizeof(lines)/sizeof(lines[0]);i++)text(28,91+(int)i*20,lines[i],1,i==11?DIM:WHITE);
    button(80,352,240,32,"BACK",1);
}
void opr_ui_render(const opr_ui *u,unsigned char pixels[400*416]) {
    out=pixels;memset(out,BG,W*H);
    if(u->scene==OPR_UI_HELP){help();return;}
    if(u->scene==OPR_UI_SELECT){
        text(56,24,"OPEN DUNGEON",4,WHITE);text(74,66,"PROCEDURAL ROGUELITE",2,BLUE);text(120,100,"SEED",1,DIM);number(156,100,u->seed,1,WHITE);
        button(40,130,320,44,"WARRIOR",1);button(40,180,320,44,"ROGUE",1);button(40,230,320,44,"MAGE",1);
        rect(28,143+u->selected_class*50,5,18,HERO);button(40,290,155,38,"NEXT SEED",1);button(205,290,155,38,"SEED 1",1);
        button(40,342,155,40,"RESUME",u->session.active);button(205,342,155,40,"HELP",1);text(92,398,"OPR CORE - ONDA DEV",1,DIM);return;
    }
    const OPR_GameState *g=&u->session.game;const OPR_Player *p=&g->player;
    button(8,8,84,30,"NEW",1);text(112,14,"DUNGEON",3,WHITE);button(306,8,86,30,"PAUSE",1);
    text(8,48,p->classId==0?"WARRIOR":p->classId==1?"ROGUE":"MAGE",1,HERO);stat(84,48,"FLOOR",p->floor,-1);stat(170,48,"LV",p->level,-1);stat(247,48,"POT",p->potions,-1);
    stat(8,64,"HP",p->hp,p->maxHp);stat(128,64,"XP",p->xp,p->xpNext);stat(268,64,"ATK",p->atk+p->weaponAtk,-1);
    stat(8,79,"GEAR",p->weaponAtk,p->armorDef);stat(128,79,"DEF",p->def+p->armorDef,-1);stat(268,79,"TURNS",u->session.actions,-1);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++){
        int xx=8+x*15,yy=98+y*15;
        if(g->map.fog[y][x]){rect(xx,yy,14,14,GHOST);continue;}
        rect(xx,yy,14,14,g->map.tiles[y][x]?WALL:FLOOR);
        if(g->map.tiles[y][x]) {rect(xx+1,yy+6,12,1,DIM);rect(xx+6,yy+1,1,5,DIM);}
    }
    for(int i=0;i<g->entityCount;i++){const OPR_Entity *e=&g->entities[i];if(e->x>=0&&e->x<16&&e->y>=0&&e->y<16&&!g->map.fog[e->y][e->x])tile_icon(e->type,8+e->x*15,98+e->y*15);}
    int px=8+p->x*15,py=98+p->y*15;rect(px+5,py+2,5,5,HERO);rect(px+3,py+8,9,5,HERO);
    text(274,100,g->inCombat?"COMBAT":"EXPLORE",2,g->inCombat?RED:BLUE);
    button(309,119,46,40,"U",!g->inCombat);button(268,164,41,40,"L",!g->inCombat);button(355,164,41,40,"R",!g->inCombat);button(309,209,46,40,"D",!g->inCombat);
    if(g->inCombat&&g->combatIndex>=0&&g->combatIndex<g->entityCount)stat(269,254,g->entities[g->combatIndex].type==OPR_BOSS?"BOSS HP":"ENEMY HP",g->entities[g->combatIndex].hp,-1);
    button(268,270,128,38,"ATTACK",g->inCombat);button(268,314,128,38,"POTION",g->inCombat&&p->potions>0);button(268,358,128,38,"FLEE",g->inCombat);
    text(8,350,"CHEST",1,GOLD);text(65,350,"POTION",1,PURPLE);text(124,350,"SHRINE",1,GREEN);text(186,350,"STAIRS",1,WHITE);
    for(unsigned i=0;i<2;i++){char line[42];unsigned from=i*40,j=0;while(j<40&&u->message[from+j]){line[j]=u->message[from+j];j++;}line[j]=0;text(8,373+(int)i*12,line,1,WHITE);if(j<40)break;}
    text(8,406,"SEED",1,DIM);number(42,406,u->session.seed,1,DIM);
    if(g->gameOver||g->victory){rect(42,142,316,132,PANEL);text(g->victory?94:82,158,g->victory?"VICTORY":"GAME OVER",3,g->victory?GOLD:RED);text(85,194,g->victory?"FIVE FLOORS CLEARED":"TRY A DIFFERENT ROUTE",1,WHITE);button(80,215,240,42,"REPLAY SEED",1);}
    if(u->scene==OPR_UI_PAUSE){rect(55,92,290,247,PANEL);text(116,113,"PAUSED",4,WHITE);button(80,155,240,44,"CONTINUE",1);button(80,215,240,44,"NEW RUN",1);button(80,275,240,44,"HELP",1);}
}
