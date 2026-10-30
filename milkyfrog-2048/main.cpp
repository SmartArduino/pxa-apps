// SPDX-License-Identifier: MIT
#include "../common/milky-frog/media.hpp"
#include "sprites.hpp"
#include "core.hpp"
#include <stdio.h>
#include <string.h>
namespace {
using namespace pocket;using namespace milky;
const Sprite *const tiles[]={&art::tile_1,&art::tile_2,&art::tile_3,&art::tile_4,&art::tile_5,&art::tile_6,&art::tile_7,&art::tile_8,&art::tile_9,&art::tile_10,&art::tile_11,&art::tile_12};
const char *const names[]={"迷你奶娃","奶油奶娃","微笑奶娃","苹果奶娃","奶茶奶娃","白大褂奶娃","厨师奶娃","奶友人奶娃","朋克奶娃","绅士奶娃","女王奶娃","公主奶娃"};
const char *const recordings[]={"assets/laugh-1.s16","assets/laugh-2.s16","assets/laugh-3.s16","assets/laugh-4.s16","assets/ice-1.s16","assets/ice-2.s16","assets/ice-3.s16","assets/andy.s16","assets/lose.s16"};
struct State{uint8_t board[16],undo_board[16];uint32_t score,best,moves,undo_score;uint16_t unlocked;bool undo;};
State state{};
unsigned page,flash,auto_wait,gallery_page;bool dragging,automatic,first_merge;int down_x,down_y,spawned=-1;
void spawn(){unsigned empty[16],n=0;for(unsigned i=0;i<16;++i)if(!state.board[i])empty[n++]=i;
    if(!n)return;spawned=int(empty[pocket::random()%n]);state.board[spawned]=pocket::random()%10?1:2;flash=180;}
void new_game(){memset(state.board,0,16);state.score=state.moves=0;state.undo=false;spawn();spawn();dragging=false;first_merge=false;automatic=false;page=0;redraw();save();}
void move(unsigned direction){
    if(page||frog2048::over(state.board))return;
    uint8_t previous[16];memcpy(previous,state.board,16);uint32_t old=state.score;
    const auto result=frog2048::move(state.board,direction);
    if(!result.changed){play(4,1);return;}
    memcpy(state.undo_board,previous,16);state.undo_score=old;state.undo=true;++state.moves;
    state.score+=result.gained;if(state.score>state.best)state.best=state.score;
    if(result.largest){
        play(result.largest<=3?0:result.largest<=5?1:result.largest<=7?2:result.largest<=9?3:result.largest<11?5:6);
        if(!first_merge){play(7,2);first_merge=true;}
    }else play(4,1);
    uint16_t previous_unlocked=state.unlocked;
    for(auto value:state.board)if(value)state.unlocked|=uint16_t(1u<<(value-1));
    if(previous_unlocked!=state.unlocked){play(7,2);log("Milky2048: new character unlocked");}
    spawn();if(frog2048::over(state.board))play(8);
    char line[100];snprintf(line,sizeof(line),"Milky2048: move=%lu score=%lu merged=%u",(unsigned long)state.moves,(unsigned long)state.score,result.largest);log(line);
    redraw();save();
}
void undo(){if(!state.undo)return;memcpy(state.board,state.undo_board,16);state.score=state.undo_score;if(state.moves)--state.moves;state.undo=false;flash=0;play(1);redraw();save();log("Milky2048: undo");}
void tap(int x,int y){
    if(page==2){int result=settings_touch(x,y);if(result==2)page=0;if(result)save();redraw();return;}
    if(page==1){auto l=gallery_layout();unsigned capacity=unsigned(l.columns*l.rows),pages=(12+capacity-1)/capacity;
        if(back_button().contains(x,y))page=0;
        else if(l.previous.contains(x,y))gallery_page=(gallery_page+pages-1)%pages;
        else if(l.next.contains(x,y))gallery_page=(gallery_page+1)%pages;
        redraw();return;}
    auto l=board_layout();
    if(l.actions[0].contains(x,y))undo();
    else if(l.actions[1].contains(x,y)){page=1;gallery_page=0;automatic=false;}
    else if(l.actions[2].contains(x,y)){page=2;automatic=false;}
    else if(l.actions[3].contains(x,y)){automatic=!automatic;auto_wait=0;}
    else if(frog2048::over(state.board)&&l.board.contains(x,y)){if(x<l.board.x+l.board.w/2)undo();else new_game();}
    redraw();
}
}
namespace game {
const char *title(){return "奶蛙2048超级版";}const char *app_id(){return "pxa-milkyfrog-2048";}
void init(){sounds(recordings,9);state={};state.unlocked=3;volume(100,0);new_game();}
void reset(){new_game();play(1);}
void key(Key key){if(key==Key::B){if(page){page=0;redraw();}else undo();return;}
    if(key==Key::X){reset();return;}if(key==Key::Y){page=page?0:2;automatic=false;redraw();return;}
    if(key==Key::A&&page==2){audio_test();return;}if(unsigned(key)<4)move(unsigned(key));}
void pointer(Phase phase,int x,int y){
    if(phase==Phase::DOWN){down_x=x;down_y=y;dragging=true;}
    else if(phase==Phase::CANCEL)dragging=false;
    else if(phase==Phase::UP&&dragging){dragging=false;const int dx=x-down_x,dy=y-down_y,ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        if(!page&&board_layout().board.contains(down_x,down_y)&&(ax>=14||ay>=14))move(ax>ay?(dx>0?1:3):(dy>0?2:0));
        else if(ax<12&&ay<12)tap(x,y);
    }
}
void tick(uint32_t elapsed){
    if(flash){flash=elapsed>=flash?0:flash-elapsed;redraw();}
    if(!automatic||page)return;
    if(frog2048::over(state.board)){automatic=false;return;}
    auto_wait+=elapsed;if(auto_wait<550)return;auto_wait=0;
    int best=-1;unsigned direction=0;
    for(unsigned d=0;d<4;++d){uint8_t trial[16];memcpy(trial,state.board,16);const auto result=frog2048::move(trial,d);if(!result.changed)continue;
        int rating=int(result.gained);for(auto v:trial)if(!v)rating+=100;
        if(rating>best){best=rating;direction=d;}}
    if(best>=0)move(direction);
}
void draw(){
    clear(CREAM);
    if(page==2){settings_draw();return;}
    if(page==1){header(tr("Frog collection","奶蛙图鉴"),true);auto l=gallery_layout();unsigned capacity=unsigned(l.columns*l.rows),pages=(12+capacity-1)/capacity;
        gallery_page%=pages;
        for(unsigned k=0;k<capacity;++k){unsigned i=gallery_page*capacity+k;if(i>=12)break;auto r=gallery_cell(l,int(k));bool known=state.unlocked&(1u<<i);
            panel(r,known?PAPER:SOFT,false);int caption=text_height()+3;
            if(known)thumbnail(*tiles[i],{r.x+4,r.y+3,r.w-8,r.h-caption-3});
            else{int size=std::min(28,std::min(r.w-8,r.h-caption-4));icon(Icon::Lock,{r.x+(r.w-size)/2,r.y+(r.h-caption-size)/2,size,size},SUBTLE);}
            label({r.x+3,r.bottom()-caption,r.w-6,caption},known?names[i]:tr("Locked","未解锁"),known?BROWN:SUBTLE,1,1);
        }
        action(l.previous,tr("Previous","上一页"),false,Icon::Back);char value[40];snprintf(value,sizeof(value),"%s %u/%u",tr("Next","下一页"),gallery_page+1,pages);action(l.next,value,false,Icon::Next);return;
    }
    auto l=board_layout();if(l.compact){auto b=bounds();label({b.x+gap(),b.y+gap()/2,49,touch_height()},"2048",BROWN,2);}else header(tr("Milky Frog 2048","奶蛙 2048"));
    metric(l.score,tr("Score","得分"),state.score,true);metric(l.best,tr("Best","最高"),state.best);
    panel(l.board,SOFT,false);const int padding=std::max(4,l.board.w/40),spacing=std::max(3,l.board.w/55),cell=(l.board.w-2*padding-3*spacing)/4;
    static constexpr uint16_t colors[]={0,pocket::rgb(255,238,176),pocket::rgb(255,224,138),pocket::rgb(255,215,94),pocket::rgb(255,201,77),pocket::rgb(255,185,59),pocket::rgb(255,166,46),pocket::rgb(255,148,34),pocket::rgb(249,127,31),pocket::rgb(239,106,28),pocket::rgb(226,84,26),pocket::rgb(201,58,23),pocket::rgb(139,70,214)};
    for(int i=0;i<16;++i){Rect r{l.board.x+padding+(i%4)*(cell+spacing),l.board.y+padding+(i/4)*(cell+spacing),cell,cell};unsigned v=state.board[i];
        rounded(r.x,r.y,r.w,r.h,v?colors[v]:CREAM,std::max(3,cell/8));
        if(flash&&i==spawned){outline(r.x+1,r.y+1,r.w-2,r.h-2,HONEY);}
        if(!v)continue;const int caption=std::min(text_height()+1,cell/3+2);
        portrait(*tiles[v-1],{r.x+2,r.y+2,r.w-4,r.h-caption-2});
        char value[12];snprintf(value,sizeof(value),"%u",1u<<v);label({r.x+1,r.bottom()-caption,r.w-2,caption},value,v>6?PAPER:BROWN,1,1);
    }
    action(l.actions[0],tr("Undo","撤销"),state.undo);action(l.actions[1],tr("Dex","图鉴"));action(l.actions[2],tr("Sound","声音"));action(l.actions[3],automatic?tr("Auto on","自动开"):tr("Auto","自动"),automatic);
    if(frog2048::over(state.board)){Rect box{l.board.x+8,l.board.y+l.board.h/2-42,l.board.w-16,84};panel(box);label({box.x+4,box.y+4,box.w-8,30},tr("Game over","游戏结束"),BROWN,2,1);label({box.x+4,box.y+38,box.w/2-6,36},tr("Undo","撤销"),SAGE,1,1);label({box.x+box.w/2,box.y+38,box.w/2-4,36},tr("New game","新一局"),BROWN,1,1);}
}
size_t serialize(uint8_t *p,size_t capacity){if(capacity<64)return 0;memset(p,0,64);memcpy(p,"MF48",4);p[4]=1;p[5]=state.undo;p[6]=uint8_t(effects_volume());p[7]=uint8_t(music_volume());
    put32(p+8,state.score);put32(p+12,state.best);put32(p+16,state.moves);memcpy(p+20,state.board,16);memcpy(p+36,state.undo_board,16);put32(p+52,state.undo_score);p[56]=uint8_t(state.unlocked);p[57]=uint8_t(state.unlocked>>8);put32(p+60,hash(p,60));return 64;}
void load(const uint8_t *p,size_t n){if(n!=64||memcmp(p,"MF48",4)||p[4]!=1||p[5]>1||p[6]>100||p[7]>100||p[58]||p[59]||get32(p+60)!=hash(p,60)||get32(p+12)<get32(p+8))return;
    bool nonempty=false;for(int i=0;i<16;++i){if(p[20+i]>12||p[36+i]>12)return;nonempty|=p[20+i]!=0;}if(!nonempty||(p[57]&0xf0))return;
    memcpy(state.board,p+20,16);memcpy(state.undo_board,p+36,16);state.score=get32(p+8);state.best=get32(p+12);state.moves=get32(p+16);state.undo_score=get32(p+52);state.undo=p[5];state.unlocked=uint16_t(p[56])|uint16_t(p[57])<<8;volume(p[6],p[7]);flash=0;dragging=false;redraw();}
}
