// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-play/music.hpp"
#include "core.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "scene.inc"
#include "../common/pocket-studio/particles.hpp"
#include <stdio.h>
namespace {
using namespace pocket;
grove::State s;
pocket_music::Effects effects;
void cue(pocket_music::Cue value) {effects.set_enabled(s.sound!=0);effects.cue(value);}
uint64_t mono=0;
uint32_t animation=0,autosave=0,cooldown=0,toast_ms=0;
unsigned journal_page=0;
uint32_t paint_ms=0;
studio::Particles<> sparkles;
int touch=-1;
char toast_en[128]="",toast_zh[128]="";
const char *toast_text() {return tr(toast_en,toast_zh);}
constexpr uint16_t PAPER=rgb(249,238,204),INK2=rgb(31,51,56),GOLD=rgb(246,209,111);
constexpr uint16_t MOSS=rgb(64,116,88),MINT=rgb(168,211,153),BLUE=rgb(121,190,205),PURPLE=rgb(95,81,128);
void sync_clock() {
    uint64_t now=grove::monotonic_ms();
    if(now&&mono&&now>=mono) grove::produce(s,now-mono);
    mono=now;
}
void notice(const char *en,const char *zh) {snprintf(toast_en,sizeof(toast_en),"%s",en);snprintf(toast_zh,sizeof(toast_zh),"%s",zh);toast_ms=3200;redraw();}
void record(const char *tag) {
    char line[224];snprintf(line,sizeof(line),"lantern-grove %s view=%u levels=%u/%u/%u/%u resources=%u/%u/%u pending=%u/%u/%u goals=%u collected=%u wall=%llu",tag,s.view,s.level[0],s.level[1],s.level[2],s.level[3],s.resources[0],s.resources[1],s.resources[2],s.pending[0],s.pending[1],s.pending[2],s.goals,s.collected,(unsigned long long)s.last_wall);
    log(line);save();redraw();
}
const char *upgrade_name(unsigned i) {const char *zh[]={"树冠","露井","灯笼","守林屋"};return i<4?tr(grove::upgrade_name(i),zh[i]):"";}
const char *goal_title(unsigned g) {const char *zh[]={"初见流萤","树冠新生","一口小井","共享收获","照亮小路","守林人的家","幽深林地","灯笼盛会"};return g<8?tr(grove::goal_title(g),zh[g]):"";}
const char *goal_detail(unsigned g) {const char *zh[]={"累计收集10","树冠升到2级","露井升到1级","累计收集60","灯笼升到1级","守林屋升到1级","树冠4级 + 露井2级","灯笼3 屋2 累计收集1800"};return g<8?tr(grove::goal_detail(g),zh[g]):"";}
void center(int x,int y,int width,const char *value,uint16_t color=INK2) { text(x+(width-text_width(value))/2,y,value,color); }
void panel(int x,int y,int w,int h,uint16_t fill,uint16_t edge=INK2) { studio::card(x,y,w,h,fill,edge,5); }
void badge(int x,int y,int w,const char *label,bool selected) { studio::button(x,y,w,20,label,touch>=0&&touch<3&&x/96==touch?studio::mix(MOSS,INK2,48):selected?MOSS:PAPER,selected?PAPER:INK2,selected); }
unsigned fly_count(){return 8+s.level[0]*3u+s.level[2]*2u;}
void fly_position(unsigned i,int &x,int &y){
    int phase=int(animation/13+i*173);
    x=(i?18+int(i*47%250):72)+int(4*sinf(phase*.008f));
    y=(i?76+int(i*23%65):99)+int(3*cosf(phase*.011f));
}
int fly_hit(int x,int y){int best=-1,distance=197;for(unsigned i=0;i<fly_count();++i){int fx,fy;fly_position(i,fx,fy);int dx=x-fx,dy=y-fy,d=dx*dx+dy*dy;if(d<distance){distance=d;best=int(i);}}return best<0?-1:40+best;}
void scene() {
    const bool night=s.cycle_ms>=60000;
    unsigned phase=s.cycle_ms;int darkness=phase<40000?0:phase<60000?int((phase-40000)*64/20000):phase<105000?64:int((120000-phase)*64/15000);
    studio::picture(0,43,288,111,scene_image,darkness);
    // Living details are separate from the painted environment and reflect growth.
    for(unsigned i=0;i<fly_count();++i){
        int phase=int(animation/13+i*173),x,y;fly_position(i,x,y);
        int pulse=65+int(35*sinf(phase*.014f));
        studio::glow(x,y,i?5:8,GOLD,night?pulse+30:pulse);pixel(x,y,GOLD);if(!i){pixel(x-2,y-1,PAPER);pixel(x+2,y-1,PAPER);}
        if(pulse>84)pixel(x,y-1,PAPER);
    }
    if(s.level[1]){
        for(unsigned i=0;i<2u+s.level[1];++i){int x=118+int(i)*2,y=104+int((animation/260+i*3)%5);pixel(x,y,BLUE);}
        studio::glow(123,106,6,BLUE,70);
    }
    if(s.level[2])for(unsigned i=0;i<2u+s.level[2];++i){
        int x=161+int(i)*21,y=82+int(i%2)*7;line(x,y-6,x,y,INK2);
        studio::glow(x,y+2,9,GOLD,night?130:75);studio::round(x-2,y-1,5,7,1,GOLD);rect(x,y,1,4,PAPER);
    }
    if(s.level[3]){
        for(unsigned i=0;i<s.level[3];++i){int y=52-int((animation/320+i*5)%11);studio::glow(250+int(i)*2,y,3,rgb(179,190,177),55);}
        for(unsigned i=0;i<s.level[3]*2u;++i){int x=211+int(i)*8;rect(x,139,4,3,MOSS);pixel(x+2,138,i%2?GOLD:PAPER);}
    }
    line(0,153,287,153,rgb(69,105,87));
}

void heading(const char *label) {
    clear(PAPER);
    studio::gradient(0,0,288,224,PAPER,rgb(222,228,201));
    rect(0,0,288,20,INK2);text(32,5,tr("LANTERN GROVE","灯火森林"),PAPER);
    text(8,23,label,INK2);
    if(s.view){studio::round(188,21,76,12,3,rgb(219,224,196));studio::label(188,22,76,tr("< Back","< 返回"),MOSS);}
    else studio::label(184,23,78,s.cycle_ms<60000?tr("SUNSET","林间暮色"):tr("MOONLIGHT","月下微光"),MOSS);
}

void resources() {
    const char *labels[]={tr("LIGHT","微光"),tr("DEW","露水"),tr("SEED","种子")};
    for(unsigned i=0;i<3;++i) {
        char b[24];snprintf(b,sizeof(b),"%s %u",labels[i],s.resources[i]);
        rect(int(i)*96,33,96,10,i==0?GOLD:i==1?BLUE:MINT);studio::label(int(i)*96+3,33,90,b,INK2);
    }
}
unsigned next_goal();
void home_draw() {
    unsigned mission=next_goal();char title[80];snprintf(title,sizeof(title),tr("Goal: %s >","目标：%s >"),mission<8?goal_title(mission):tr("Lantern festival","灯笼盛会"));
    heading(title);resources();scene();
    char b[96];snprintf(b,sizeof(b),tr("BASKET  %u L · %u D · %u S","待收集  %u光 · %u露 · %u种"),s.pending[0],s.pending[1],s.pending[2]);center(0,158,288,b);
    unsigned goal=next_goal();bool reward=goal<8&&grove::achieved(s,goal);
    badge(8,174,84,cooldown?tr("SETTLING","收集中…"):tr("COLLECT","收集"),s.selected==0);badge(100,174,84,tr("BUILD","建造"),s.selected==1);badge(192,174,88,reward?tr("CLAIM REWARD","可领奖"):tr("JOURNAL","手记"),s.selected==2);
    unsigned progress=mission<8?grove::goal_progress(s,mission):1000;studio::round(8,198,204,2,1,rgb(185,195,163));if(progress)studio::round(8,198,int(204*progress/1000),2,1,MOSS);
    text(8,201,tr("A TEND  L/R PAGES","A 照料  L/R 翻页"),INK2);text(221,201,tr("? GUIDE","? 指南"),PURPLE);
    text(7,213,toast_ms?toast_text():tr("Tap a firefly to gather your basket.","轻点流萤，也能收集微光与篮中资源。"),INK2);
}
void build_draw() {
    heading(tr("GROW YOUR WOODLAND","林地建设"));resources();
    for(unsigned i=0;i<4;++i) {
        int y=48+int(i)*34;panel(8,y,272,31,touch==int(i)?rgb(216,210,169):s.selected==i?rgb(232,220,176):PAPER,s.selected==i?MOSS:rgb(183,181,150));
        char b[96];snprintf(b,sizeof(b),tr("%s  LV %u","%s %u级"),upgrade_name(i),s.level[i]);text(17,y+4,b,INK2);
        grove::Price p=grove::price(s,i);
        snprintf(b,sizeof(b),tr("Light %u / Dew %u / Seeds %u","微光 %u · 露水 %u · 种子 %u"),p.value[0],p.value[1],p.value[2]);text(17,y+16,b,INK2);
        bool max=s.level[i]>=unsigned(i==0?5:i==3?3:4),afford=true;for(unsigned r=0;r<3;++r)if(s.resources[r]<p.value[r])afford=false;
        studio::label(222,y+5,49,max?tr("MAX","满级"):afford?tr("BUY","建造"):tr("NEED","待积累"),max?PURPLE:afford?MOSS:rgb(140,135,114));
    }
    const char *effects[]={tr("+1.0 L/S +0.1 D/S +0.08 S/S","每秒 +1L +0.1D +0.08S"),tr("+0.65 D/S +1500 BASKET CAP","每秒 +0.65D 篮容量+1500"),tr("+0.7 L/S +0.06 S/S","每秒 +0.7L +0.06S"),tr("+0.3 L/S +0.22 S/S +3000 CAP","每秒 +0.3L +0.22S 容量+3000")};
    text(9,188,effects[s.selected],MOSS);text(9,202,tr("UP/DOWN SELECT  A BUY  B BACK","上下选择 A 购买 B 返回"),INK2);
    text(7,213,toast_ms?toast_text():tr("UPGRADES CHANGE THE WOODLAND ITSELF.","升级会改变眼前的林地。"),INK2);
}
unsigned next_goal() {unsigned g=0;while(g<8&&(s.goals&(1u<<g))) ++g;return g;}
void journal_draw() {
    heading(tr("GROVE JOURNAL","林地手记"));
    char b[96];snprintf(b,sizeof(b),tr("CHAPTER %u/2   COLLECTED %u","第%u/2页 累计收集%u"),journal_page+1,s.collected);text(8,37,b,MOSS);
    for(unsigned row=0;row<4;++row) {
        unsigned g=journal_page*4+row;int y=51+int(row)*32;
        bool done=s.goals&(1u<<g),ready=grove::achieved(s,g)&&g==next_goal();
        panel(8,y,272,29,done?MINT:s.selected==row?rgb(239,225,177):PAPER);
        snprintf(b,sizeof(b),"%u %s",g+1,goal_title(g));text(16,y+4,b,INK2);
        text(16,y+16,goal_detail(g),INK2);text(246,y+4,done?tr("OK","成"):ready?tr("GET","领"):"...",MOSS);
    }
    unsigned g=journal_page*4+s.selected;
    snprintf(b,sizeof(b),tr("REWARD %u L %u D %u S","奖励 %uL %uD %uS"),(g+1)*5,(g+1)*2,(g+1)*2);text(9,184,b,MOSS);
    text(9,199,tr("A CLAIM  B HOME   L/R PAGES","A 领取 B 返回 L/R 翻页"),INK2);text(249,199,tr("NEXT","下页"),MOSS);
    text(7,213,toast_ms?toast_text():tr("EIGHT LITTLE STEPS TO A LANTERN FESTIVAL.","八段小小旅程，迎来灯笼盛会。"),INK2);
}
void help_draw() {
    heading(tr("A SMALL GUIDE","林间指南"));
    const char *lines[]={tr("L LUMEN   D DEW   S SEED","L 微光  D 露水  S 种子"),tr("1. LIGHT GATHERS IN YOUR BASKET.","1. 微光自动汇入篮子。"),tr("2. TAP A FIREFLY OR COLLECT / A.","2. 点流萤、收集或按A，收入仓库。"),tr("   TENDING ADDS 1 L EVERY 0.65S.","照料每0.65秒可额外获得1L。"),tr("3. BUILD: COSTS ABOVE, BENEFIT BELOW.","3. 建造页显示花费与每级收益。"),tr("4. JOURNAL: CLAIM EACH OF 8 REWARDS.","4. 手记有八项奖励，依次领取。"),tr("TIME: UP TO 8H AWAY; BASKET IS CAPPED.","离线最多积累8小时，篮满即停。"),tr("OFFLINE REWARDS NEED THE CORRECT TIME.","离线收获需要设备时间准确。"),tr("L/R PAGES  X BUILD  Y JOURNAL","L/R 翻页 X 建造 Y 手记"),tr("START MENU. RESET KEEPS YOUR GROVE.","START 菜单；重开保留林地成长。")};
    for(unsigned i=0;i<10;++i) text(9,43+int(i)*14,lines[i],i==0?MOSS:INK2);
    badge(146,188,132,s.sound?tr("AUDIO ON","音效 开"):tr("AUDIO OFF","音效 关"),true);badge(8,188,128,tr("BACK TO GROVE","返回林地"),false);
    text(9,213,tr("A little care, every day.","每天照料一点，让森林慢慢生长。"),INK2);
}
void set_view(unsigned v) {s.view=uint8_t(v);s.selected=0;touch=-1;if(v==2) {unsigned n=next_goal();journal_page=n<8?n/4:1;s.selected=uint8_t(n<8?n%4:3);}cue(pocket_music::Cue::Select);record("page");}
void act(unsigned item) {
    sync_clock();
    if(s.view==0) {
        if(item==0) {if(cooldown) {redraw();return;}grove::collect(s);sparkles.burst(44,156,GOLD,16);cooldown=650;notice("BASKET COLLECTED! +1 L FOR TENDING.","已收好篮中资源，照料额外+1L。");cue(pocket_music::Cue::Collect);record("collect");}
        else if(item<3) set_view(item);
        else set_view(3);
    } else if(s.view==1) {
        s.selected=uint8_t(item);
        if(grove::buy(s,item)) {notice("A NEW LITTLE DETAIL IN YOUR WOODLAND!","林地又添了一处小小风景！");cue(pocket_music::Cue::Upgrade);record("upgrade");}
        else {notice("SAVE UP THE SHOWN COST, OR TRY ANOTHER.","资源不足或已满级，试试其他建设。");cue(pocket_music::Cue::Error);}
    } else if(s.view==2) {
        s.selected=uint8_t(item);unsigned g=journal_page*4+item;
        if(grove::claim(s,g)) {notice("JOURNAL REWARD COLLECTED. KEEP GROWING!","手记奖励已领取，继续培育林地！");cue(pocket_music::Cue::Goal);record("goal");}
        else {notice("MEET THIS GOAL AND CLAIM EARLIER GOALS.","先达成目标，并领取前面的奖励。");cue(pocket_music::Cue::Error);}
    } else if(item==1) {s.sound^=1;effects.set_enabled(s.sound!=0);cue(pocket_music::Cue::Confirm);record("sound");}
    else set_view(0);
    redraw();
}
int hit(int x,int y) {
    if(x<4||x>=284||y<20||y>=212) return -1;
    if(s.view!=0&&x>=184&&x<264&&y<43) return 6;
    if(s.view==0) {if(y>=43&&y<154)return fly_hit(x,y);if(y>=20&&y<33&&x<184)return 2;if(y>=174&&y<194) return x<96?0:x<188?1:2;if(y>=197&&x>=215) return 3;}
    else if(s.view==1&&y>=48&&y<184) return (y-48)/34;
    else if(s.view==2) {if(y>=51&&y<179) return (y-51)/32;if(y>=185&&y<212&&x>=240) return 5;}
    else if(s.view==3&&y>=188&&y<208) return x<140?0:1;
    return -1;
}
}
namespace game {
const char *title() {return tr("Lantern Grove","灯火森林");}
const char *app_id() {return "pxa-lantern-grove";}
void init() {sparkles.clear();grove::fresh(s);effects.reset(s.sound!=0);mono=grove::monotonic_ms();animation=autosave=cooldown=toast_ms=0;toast_en[0]=toast_zh[0]=0;touch=-1;record("new");}
void reset() {sparkles.clear();sync_clock();s.view=0;s.selected=0;touch=-1;notice("BACK IN THE GROVE. ALL GROWTH IS SAFE.","回到林地，所有成长都已保留。");cue(pocket_music::Cue::Confirm);record("reset");}
void key(Key k) {
    if(k==Key::SELECT) {set_view(s.view==3?0:3);return;}
    if(k==Key::B) {set_view(0);return;}
    if(k==Key::X) {set_view(1);return;}
    if(k==Key::Y) {set_view(2);return;}
    if(k==Key::L||k==Key::R) {set_view((s.view+(k==Key::R?1:2))%3);return;}
    if(k==Key::A) {act(s.view==3?1:s.selected);return;}
    if(s.view==2&&(k==Key::LEFT||k==Key::RIGHT)) {journal_page^=1;s.selected=0;redraw();return;}
    if(k==Key::LEFT||k==Key::UP) s.selected=uint8_t((s.selected+(s.view==0?2:3))%(s.view==0?3:4));
    if(k==Key::RIGHT||k==Key::DOWN) s.selected=uint8_t((s.selected+1)%(s.view==0?3:4));
    redraw();
}
void pointer(Phase phase,int x,int y) {
    if(phase==Phase::CANCEL) {touch=-1;return;}
    if(phase==Phase::DOWN) {touch=hit(x,y);return;}
    if(phase==Phase::UP) {int at=touch;touch=-1;if(at>=0&&at==hit(x,y)) {if(at==6) set_view(0);else if(at==5) {journal_page^=1;redraw();}else if(at>=40){int fx,fy;fly_position(unsigned(at-40),fx,fy);bool ready=!cooldown;act(0);if(ready)sparkles.burst(fx,fy,GOLD,18);}else act(unsigned(at));}}
}
void tick(uint32_t elapsed) {
    if(elapsed>100) elapsed=100;
    if(!grove::monotonic_ms()) grove::produce(s,elapsed); // honest foreground fallback
    if(sparkles.tick(elapsed))redraw();
    sync_clock();animation+=elapsed;autosave+=elapsed;paint_ms+=elapsed;
    cooldown=elapsed>=cooldown?0:cooldown-elapsed;toast_ms=elapsed>=toast_ms?0:toast_ms-elapsed;
    if(autosave>=12000) {autosave=0;save();}
    if(paint_ms>=50){paint_ms=0;redraw();}
}
void draw() {if(s.view==0) home_draw();else if(s.view==1) build_draw();else if(s.view==2) journal_draw();else help_draw();sparkles.draw();}
size_t serialize(uint8_t *data,size_t size) {sync_clock();grove::stamp(s,grove::wall_seconds());return grove::encode(s,data,size);}
void load(const uint8_t *data,size_t size) {
    grove::State candidate;
    if(!grove::decode(candidate,data,size)) {log("lantern-grove rejected save");return;}
    grove::Away away=grove::offline(candidate,grove::wall_seconds());sparkles.clear();s=candidate;effects.set_enabled(s.sound!=0);mono=grove::monotonic_ms();touch=-1;
    if(away.valid&&away.seconds) {char en[128],zh[128];snprintf(en,sizeof(en),"WELCOME BACK! %uS OF LIGHT IN THE BASKET.",away.seconds);snprintf(zh,sizeof(zh),"欢迎回来！篮中积累了%u秒的微光。",away.seconds);notice(en,zh);}
    else if(!away.valid) notice("WELCOME HOME. LIVE LIGHT IS GROWING.","欢迎回家，微光正在慢慢汇聚。");
    record("loaded");
}
}
