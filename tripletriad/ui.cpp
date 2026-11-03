// SPDX-License-Identifier: BSD-2-Clause
// PXA raster UI; rule comparisons and CPU decisions remain upstream code.
#include "ui.hpp"
#include <cstring>
namespace pxa_tt {
namespace {
// Small original bitmap text, shared by this round's MIT PXA adapters.
const uint8_t letters[36][5]={
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}};
void rect(UI&u,int x,int y,int w,int h,uint8_t c){for(int j=y;j<y+h;j++)if(j>=0&&j<Height)for(int i=x;i<x+w;i++)if(i>=0&&i<Width)u.pixels[j*Width+i]=c;}
void text(UI&u,int x,int y,const char*s,int size=1,uint8_t c=255){for(;*s;s++,x+=6*size){unsigned ch=*s;if(ch>='a'&&ch<='z')ch-=32;unsigned k=ch>='0'&&ch<='9'?ch-'0':ch>='A'&&ch<='Z'?10+ch-'A':36;if(k<36)for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(letters[k][i]&(1<<j))rect(u,x+i*size,y+j*size,size,size,c);}}
void number(UI&u,int x,int y,unsigned value,int size=1,uint8_t color=255){char buffer[12];unsigned n=0;do{buffer[n++]=static_cast<char>('0'+value%10);value/=10;}while(value&&n<10);for(unsigned a=0,b=n-1;a<b;a++,b--){char c=buffer[a];buffer[a]=buffer[b];buffer[b]=c;}buffer[n]=0;text(u,x,y,buffer,size,color);}
void border(UI&u,int x,int y,int w,int h,uint8_t c){rect(u,x,y,w,2,c);rect(u,x,y+h-2,w,2,c);rect(u,x,y,2,h,c);rect(u,x+w-2,y,2,h,c);}
void button(UI&u,int x,int y,int w,const char*s){rect(u,x,y,w,32,38);border(u,x,y,w,32,109);text(u,x+(w-static_cast<int>(strlen(s))*12)/2,y+9,s,2);}
void face(UI&u,int x,int y,int size,int id){if(id<0||id>=111)return;for(int j=0;j<size;j++)for(int i=0;i<size;i++){uint8_t c=card_faces[id][j*64/size*64+i*64/size];if(c)rect(u,x+i,y+j,1,1,c);}}
void card(UI&u,int x,int y,const tt::Card&c,bool selected,bool name){
 uint8_t owner=c.player_id==tt::PLAYER_ID_1?23:225;rect(u,x,y,72,name?80:72,owner);face(u,x+4,y+4,64,c.id);border(u,x,y,72,name?80:72,selected?252:owner);
 static const int dx[]={32,59,32,7},dy[]={4,31,60,31};
 for(unsigned r=0;r<4;r++){rect(u,x+dx[r]-2,y+dy[r]-1,10,9,0);if(c.ranks[r]==10)text(u,x+dx[r],y+dy[r],"A");else number(u,x+dx[r],y+dy[r],c.ranks[r]);}
 if(name){char s[12];unsigned n=0;for(;n<11&&n<c.name.size();n++)s[n]=c.name[n];s[n]=0;rect(u,x+2,y+70,68,8,0);text(u,x+3,y+71,s);}
}
}
UI::UI(){game.start(seed,rules_mode);game.stage=Menu;}
void UI::start(){game.start(++seed,rules_mode);selected=0;cursor=4;board_focus=false;}
void UI::tick(uint32_t ms){game.tick(ms);if(selected>=game.hands[0].size())selected=0;frames++;}
void UI::pointer(int x,int y){
 if(x<0||y<0||x>=Width||y>=Height)return;
 if(y>=8&&y<40){if(x>=8&&x<104){game.stage=Menu;game.paused=false;}else if(x>=286&&x<392&&game.stage==Playing)game.paused=!game.paused;return;}
 if(game.stage==Menu){if(x>=100&&x<300){if(y>=130&&y<162)start();else if(y>=186&&y<218)rules_mode=(rules_mode+1)%3;else if(y>=242&&y<274){++seed;start();game.stage=Menu;}}return;}
 if(game.stage==Result){if(x>=100&&x<300&&y>=186&&y<218)start();return;}
 if(game.paused){if(x>=100&&x<300&&y>=186&&y<218)game.paused=false;return;}
 if(game.turn!=0||game.background)return;
 if(y>=312&&y<392&&x>=10&&x<395){unsigned k=(x-10)/77;if((x-10)%77<72&&k<game.hands[0].size()){selected=k;board_focus=true;}return;}
 if(x>=92&&x<308&&y>=72&&y<288){int bx=(x-92)/72,by=(y-72)/72;cursor=by*3+bx;if(game.place(0,selected,bx,by)){selected=0;board_focus=false;}}
}
void UI::buttons(uint32_t pressed){
 if(game.stage==Menu){if(pressed&16)start();if(pressed&32)rules_mode=(rules_mode+1)%3;return;}
 if(game.stage==Result){if(pressed&16)start();return;}
 if(pressed&32){if(board_focus)board_focus=false;else game.paused=!game.paused;return;}
 if(game.paused||game.background||game.turn!=0)return;
 if(board_focus){int x=cursor%3,y=cursor/3;if((pressed&1)&&y>0)y--;if((pressed&2)&&y<2)y++;if((pressed&4)&&x>0)x--;if((pressed&8)&&x<2)x++;cursor=y*3+x;
  if((pressed&16)&&game.place(0,selected,x,y)){selected=0;board_focus=false;}
 }else{if((pressed&(1|4))&&selected>0)selected--;if((pressed&(2|8))&&selected+1<game.hands[0].size())selected++;if(pressed&16)board_focus=true;}
}
void UI::draw(){
 memset(pixels,5,sizeof pixels);button(*this,8,8,96,"MENU");button(*this,286,8,106,game.paused?"RESUME":"PAUSE");text(*this,117,19,"TRIPLE TRIAD",1);
 text(*this,10,52,"YOU",2,23);number(*this,58,52,game.score[0],2,23);text(*this,320,52,"CPU",2,225);number(*this,368,52,game.score[1],2,225);
 for(int y=0;y<3;y++)for(int x=0;x<3;x++){int px=92+x*72,py=72+y*72;const auto&c=game.board.get(x,y);rect(*this,px,py,72,72,36);border(*this,px,py,72,72,109);if(c.id!=tt::BAD_CARD_ID)card(*this,px,py,c,false,false);else if(game.board.tile(x,y).element()){text(*this,px+24,py+30,"E",2,252);number(*this,px+36,py+30,game.board.tile(x,y).element(),2,252);}if(board_focus&&cursor==unsigned(y*3+x)&&game.turn==0)border(*this,px,py,72,72,252);}
 for(unsigned i=0;i<game.hands[1].size();i++){const auto&c=game.hands[1].cards[i];rect(*this,336,76+i*39,44,36,225);face(*this,341,78+i*39,32,c.id);}
 for(unsigned i=0;i<game.hands[0].size();i++)card(*this,10+i*77,312,game.hands[0].cards[i],selected==i,true);
 text(*this,96,295,game.turn==0?"SELECT CARD THEN A TILE":"CPU IS THINKING",1,252);
 if(game.stage==Menu){rect(*this,72,80,256,220,1);border(*this,72,80,256,220,109);text(*this,98,96,"TTCARDS PXA",3);button(*this,100,130,200,"START MATCH");button(*this,100,186,200,rules_mode==0?"OPEN":rules_mode==1?"ELEMENTAL":"SAME COMBO");button(*this,100,242,200,"NEW DECK");}
 if(game.stage==Result){rect(*this,72,112,256,134,1);border(*this,72,112,256,134,252);text(*this,game.winner==0?147:126,134,game.winner==0?"DRAW":game.winner==1?"YOU WIN":"CPU WINS",3);button(*this,100,186,200,"PLAY AGAIN");}
 if(game.paused&&game.stage==Playing){rect(*this,72,112,256,134,1);text(*this,128,138,"PAUSED",3);button(*this,100,186,200,"RESUME");}
 text(*this,10,404,"TTCARDS RULES AND AI   LOCAL LEARNING",1,182);
}
}
