/* SPDX-License-Identifier: MIT. Input facade preserves the upstream SDL2 key map. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "def.h"
#include "input.h"
#include "hardware.h"
#include "digger-host.h"
enum { RIGHT=79,UP=82,LEFT=80,DOWN=81,F1=58,S=22,W=26,A=4,Z=29,
 TAB=43,T=23,PLUS=87,MINUS=86,F7=64,F9=66,F10=67,SPACE=44,N=17,F8=65,K=14,
 RETURN=40,ESCAPE=41 };
int keycodes[NKEYS][5]={{RIGHT,-2,-2,-2,-2},{UP,-2,-2,-2,-2},{LEFT,-2,-2,-2,-2},
 {DOWN,-2,-2,-2,-2},{F1,-2,-2,-2,-2},{S,-2,-2,-2,-2},{W,-2,-2,-2,-2},
 {A,-2,-2,-2,-2},{Z,-2,-2,-2,-2},{TAB,-2,-2,-2,-2},{T,-2,-2,-2,-2},
 {PLUS,-2,-2,-2,-2},{MINUS,-2,-2,-2,-2},{F7,-2,-2,-2,-2},{F9,-2,-2,-2,-2},
 {F10,-2,-2,-2,-2},{SPACE,-2,-2,-2,-2},{N,-2,-2,-2,-2},{F8,-2,-2,-2,-2},{K,-2,-2,-2,-2}};
static unsigned char held[256];
static struct {int16_t scan,sym;} queue[30];
static unsigned used;
static int symbol(int sc) {
 if(sc>=4&&sc<=29)return 'a'+sc-4;
 if(sc==RETURN)return 13;if(sc==ESCAPE)return 27;if(sc==SPACE)return 32;
 if(sc>=30&&sc<=38)return '1'+sc-30;if(sc==39)return '0';return 0;
}
void digger_host_key(int scan,int down) {
 if(scan<0||scan>=256)return;
 if(down&&!held[scan]) {
  if(used==30){memmove(queue,queue+1,29*sizeof *queue);--used;}
  queue[used].scan=(int16_t)scan;queue[used].sym=(int16_t)symbol(scan);++used;
 }
 held[scan]=down!=0;
}
void digger_host_release(void){memset(held,0,sizeof held);used=0;}
bool kbd_async_key_state(int key){return key>=0&&key<256&&held[key];}
void initkeyb(void){digger_host_release();}
void restorekeyb(void){digger_host_release();}
bool kbhit(void){return used!=0;}
int16_t getkey(bool scan) {
 while(!used)gethrt(true,1);
 int16_t result=scan?queue[0].scan:queue[0].sym;
 --used;memmove(queue,queue+1,used*sizeof *queue);return result;
}
void strupr(char *s){for(;*s;++s)*s=(char)toupper((unsigned char)*s);}

