/* SPDX-License-Identifier: MIT. Deliberate fixtures, not ordinary-input gameplay. */
#include "opr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned calls;
int opr_rand(void) { const int seq[]={3,3,4,4};return seq[(calls++)&3]; }
static int check(int ok,const char *s) { if(!ok)fprintf(stderr,"FAIL %s\n",s);return ok; }
int main(int argc,char **argv) {
    OPR_GameState g;memset(&g,0,sizeof g);
    if(argc!=2)return 90;
    if(!strcmp(argv[1],"single")) {
        opr_init(&g,0);int floors=0,exit_count=0;
        for(int y=0;y<16;y++)for(int x=0;x<16;x++)floors+=g.map.tiles[y][x]==0;
        if(!check(floors==36,"fixture generated exactly its single 6x6 room"))return 1;
        for(int i=0;i<g.entityCount;i++){
            if(!check(g.entities[i].x!=g.player.x||g.entities[i].y!=g.player.y,"single-room entity does not occupy player"))return 1;
            exit_count+=g.entities[i].type==OPR_STAIRS;
        }
        if(!check(exit_count==1,"single-room floor keeps one usable exit"))return 1;
        puts("PASS single-room floor");return 0;
    }
    if(!strcmp(argv[1],"boss")) {
        g.player.hp=g.player.maxHp=30;g.player.atk=100;g.player.level=1;g.player.xpNext=1000;
        g.entityCount=2;g.inCombat=1;g.combatIndex=0;
        g.entities[0].type=OPR_BOSS;g.entities[0].hp=1;g.entities[0].xp=1;
        g.entities[1].type=OPR_POTION;
        opr_combat_attack(&g);
        if(!check(g.victory==1&&!g.inCombat&&g.entityCount==1,"non-last boss kill must win before slot swap"))return 1;
        memset(&g,0,sizeof g);g.player.hp=g.player.maxHp=30;g.player.atk=100;g.player.level=1;g.player.xpNext=1000;
        g.entityCount=2;g.inCombat=1;g.combatIndex=0;
        g.entities[0].type=OPR_ENEMY;g.entities[0].hp=1;
        g.entities[1].type=OPR_BOSS;g.entities[1].hp=100;
        opr_combat_attack(&g);
        if(!check(!g.victory&&!g.inCombat&&g.entityCount==1&&g.entities[0].type==OPR_BOSS,"ordinary kill must not gain swapped boss victory"))return 1;
        puts("PASS boss identity before swap removal");return 0;
    }
    return 91;
}
