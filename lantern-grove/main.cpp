// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-play/music.hpp"
#include "core.hpp"
#include <stdio.h>
namespace {
using namespace pocket;
grove::State s;
pocket_music::Effects effects;
void cue(pocket_music::Cue value) {effects.set_enabled(s.sound!=0);effects.cue(value);}
uint64_t mono=0;
uint32_t animation=0,autosave=0,cooldown=0,toast_ms=0;
unsigned journal_page=0;
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
void panel(int x,int y,int w,int h,uint16_t fill,uint16_t edge=INK2) { rect(x+2,y,w-4,h,edge);rect(x,y+2,w,h-4,edge);rect(x+2,y+2,w-4,h-4,fill); }
void badge(int x,int y,int w,const char *label,bool selected) { panel(x,y,w,20,selected?GOLD:PAPER);center(x,y+5,w,label); }
void dot(int x,int y,uint16_t c) { rect(x-1,y-1,3,3,c);pixel(x,y-2,c);pixel(x,y+2,c); }
void cloud(int x,int y,uint16_t c) {rect(x,y,21,5,c);rect(x+5,y-4,11,4,c);rect(x-5,y+3,31,3,c);}
void tree(int x,int base,unsigned age,unsigned phase,bool night) {
    int sway=int((animation/450+phase)%3)-1;
    uint16_t shade=night?rgb(30,73,75):MOSS,leaf=night?rgb(50,112,104):MINT;
    int h=18+int(age)*7,w=14+int(age)*6;
    rect(x-w/2,base+1,w,3,night?rgb(21,46,59):rgb(94,136,101));
    rect(x-3,base-h/2,6,h/2+3,rgb(110,77,67));rect(x-1,base-h/2,2,h/2+2,rgb(192,135,91));
    line(x,base-9,x-7,base-18,rgb(110,77,67));line(x,base-13,x+8,base-22,rgb(110,77,67));
    int top=base-h-12;
    for(int row=0;row<4;++row) {
        int ww=w-8+row*4;int yy=top+row*7;
        rect(x+sway-ww/2,yy+3,ww,10,shade);rect(x+sway-ww/2+3,yy,ww-6,9,leaf);
        rect(x+sway-ww/2+4,yy+3,ww/3,2,night?rgb(74,147,124):rgb(198,224,162));
    }
    unsigned count=1+age;
    for(unsigned i=0;i<count;++i) {
        int xx=x+int((i*13+phase*7)%unsigned(w-4))-w/2+2;
        int yy=top+7+int(i*9%23);
        rect(xx-2,yy-2,5,7,night?rgb(131,137,85):rgb(179,169,107));rect(xx-1,yy-1,3,4,GOLD);pixel(xx,yy,PAPER);
    }
    if(age>=4) { rect(x-2,top-8,5,6,GOLD);line(x,top-10,x+4,top-14,MINT); }
}
void lantern(int x,int y,unsigned level,bool night) {
    rect(x,y-31,3,33,rgb(103,89,90));rect(x-9,y-31,12,2,INK2);
    rect(x-11,y-29,6,3,PURPLE);rect(x-13,y-26,10,12,night?rgb(137,136,92):PURPLE);
    rect(x-11,y-24,6,8,GOLD);rect(x-9,y-23,2,6,PAPER);rect(x-12,y-14,8,2,PURPLE);
    if(level>=2) {rect(x+3,y-27,12,2,INK2);rect(x+8,y-25,7,11,GOLD);rect(x+10,y-23,2,7,PAPER);}
    if(level>=3) {dot(x+1,y-39,GOLD);line(x-10,y-37,x+13,y-37,GOLD);}
}
void cabin(int x,int y,unsigned level) {
    int width=26+int(level)*6;
    rect(x,y,width,24,rgb(166,122,91));rect(x+2,y+2,width-4,21,rgb(207,162,109));
    for(int i=0;i<4;++i) rect(x-4+i*3,y-4-i*3,width+8-i*6,4,PURPLE);
    rect(x+width/2-3,y+10,7,14,INK2);rect(x+5,y+6,6,7,GOLD);pixel(x+7,y+8,PAPER);
    if(level>=2) {rect(x+width-9,y+6,6,7,GOLD);rect(x+width-4,y-20,5,13,INK2);cloud(x+width+1,y-24,rgb(179,188,186));}
    rect(x-4,y+24,width+8,3,rgb(117,91,81));
}
void scene() {
    bool night=s.cycle_ms>=60000;
    uint16_t sky=night?rgb(23,39,66):rgb(170,207,195),ground=night?rgb(35,70,72):rgb(124,168,115);
    rect(0,43,288,111,sky);
    for(int j=0;j<5;++j) {int x=j*67-12;rect(x,89+j%2*6,48,37,night?rgb(36,67,74):rgb(120,163,135));rect(x+10,79+j%2*6,30,12,night?rgb(36,67,74):rgb(120,163,135));}
    if(night) {rect(228,51,14,14,PAPER);rect(234,48,13,14,sky);for(int i=0;i<14;++i) pixel((i*43+17)%280,48+(i*17)%39,PAPER);}
    else {rect(228,51,16,16,GOLD);cloud(34,63,PAPER);cloud(169,58,PAPER);}
    rect(0,121,288,33,ground);rect(0,149,288,5,night?rgb(41,82,80):MOSS);
    // Curved stream and stepping stones frame the central grove.
    for(int y=125;y<150;y+=3) {int x=118+(y-125)/3;rect(x,y,31,3,night?rgb(54,114,137):BLUE);rect(x+3,y,12,1,PAPER);}
    rect(118,139,8,4,PAPER);rect(135,146,7,3,PAPER);
    tree(55,136,s.level[0],0,night);tree(101,143,s.level[0]>1?s.level[0]-1:1,1,night);tree(160,130,s.level[0]>2?s.level[0]-2:1,2,night);
    if(s.level[1]) {rect(21,139,19,10,INK2);rect(23,140,15,7,BLUE);rect(20,137,21,3,PAPER);rect(21,127,2,11,INK2);rect(38,127,2,11,INK2);rect(19,124,23,4,PURPLE);if(s.level[1]>=2) rect(23,121,15,3,GOLD);}
    if(s.level[3]) cabin(214,114,s.level[3]);
    else {rect(226,135,18,5,rgb(115,112,89));text(223,139,"HOME",INK2);}
    if(s.level[2]) {lantern(192,146,s.level[2],night);if(s.level[2]>=4) lantern(269,140,2,night);}
    for(unsigned i=0;i<12+s.level[2]*3u;++i) {int x=15+int((i*47+animation/70)%260),y=77+int((i*29+animation/180)%67);if((i+animation/320)%4) dot(x,y,night?GOLD:rgb(242,228,157));}
    for(int i=0;i<16;++i) {int x=7+i*18,y=148-i%3;rect(x,y,2,3,MOSS);pixel(x+2,y-1,(i%3==0)?GOLD:MINT);}
}
void heading(const char *label) {
    clear(PAPER);rect(0,0,264,20,INK2);text(7,23,label,INK2);
    if(s.view) {panel(188,20,76,13,PAPER,PURPLE);center(188,22,76,tr("< BACK","< 返回"),PURPLE);}
    else text(180,23,s.cycle_ms<60000?tr("MORNING","晨光"):tr("MOONLIGHT","月色"),MOSS);
}
void resources() {
    const char *labels[]={tr("L","光"),tr("D","露"),tr("S","种")};
    for(unsigned i=0;i<3;++i) {
        char b[24];snprintf(b,sizeof(b),"%s %u",labels[i],s.resources[i]);
        rect(int(i)*96,33,95,10,i==0?GOLD:i==1?BLUE:MINT);text(int(i)*96+5,33,b,INK2);
    }
}
void home_draw() {
    heading(tr("LANTERN GROVE","流萤林地"));resources();scene();
    char b[96];snprintf(b,sizeof(b),tr("BASKET  %u L  %u D  %u S","篮 %uL %uD %uS"),s.pending[0],s.pending[1],s.pending[2]);center(0,158,288,b);
    badge(8,174,84,tr("COLLECT","收集"),s.selected==0);badge(100,174,84,tr("BUILD","建造"),s.selected==1);badge(192,174,88,tr("JOURNAL","手记"),s.selected==2);
    text(8,201,tr("A TEND  L/R PAGES","A 照料  L/R 翻页"),INK2);text(221,201,tr("? GUIDE","? 指南"),PURPLE);
    text(7,213,toast_ms?toast_text():tr("AUTOMATIC LIGHT. A GROWING LITTLE WORLD.","微光自动汇聚，林地慢慢成长。"),INK2);
}
void build_draw() {
    heading(tr("GROW YOUR WOODLAND","林地建设"));resources();
    for(unsigned i=0;i<4;++i) {
        int y=48+int(i)*34;panel(8,y,272,31,s.selected==i?rgb(232,220,176):PAPER,s.selected==i?MOSS:rgb(183,181,150));
        char b[96];snprintf(b,sizeof(b),tr("%s  LV %u","%s %u级"),upgrade_name(i),s.level[i]);text(17,y+4,b,INK2);
        grove::Price p=grove::price(s,i);
        snprintf(b,sizeof(b),tr("COST %u L %u D %u S","花费 %uL %uD %uS"),p.value[0],p.value[1],p.value[2]);text(17,y+16,b,INK2);
        bool max=s.level[i]>=unsigned(i==0?5:i==3?3:4);text(249,y+5,max?tr("MAX","满"):tr("BUY","买"),max?PURPLE:MOSS);
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
    const char *lines[]={tr("L LUMEN   D DEW   S SEED","L 微光  D 露水  S 种子"),tr("1. LIGHT GATHERS IN YOUR BASKET.","1. 微光自动汇入篮子。"),tr("2. TAP COLLECT / A TO BRING IT HOME.","2. 点收集或按A，收入仓库。"),tr("   TENDING ADDS 1 L EVERY 0.65S.","照料每0.65秒可额外获得1L。"),tr("3. BUILD: COSTS ABOVE, BENEFIT BELOW.","3. 建造页显示花费与每级收益。"),tr("4. JOURNAL: CLAIM EACH OF 8 REWARDS.","4. 手记有八项奖励，依次领取。"),tr("TIME: UP TO 8H AWAY; BASKET IS CAPPED.","离线最多积累8小时，篮满即停。"),tr("UTC INVALID? REOPEN BONUS UNAVAILABLE.","时钟无效时，无法领取离线收益。"),tr("L/R PAGES  X BUILD  Y JOURNAL","L/R 翻页 X 建造 Y 手记"),tr("START MENU. RESET KEEPS YOUR GROVE.","START 菜单；重开保留林地成长。")};
    for(unsigned i=0;i<10;++i) text(9,43+int(i)*14,lines[i],i==0?MOSS:INK2);
    badge(146,188,132,s.sound?tr("AUDIO ON","音效 开"):tr("AUDIO OFF","音效 关"),true);badge(8,188,128,tr("BACK TO GROVE","返回林地"),false);
    text(9,213,tr("CLOCK: STANDARD WASI REALTIME + MONOTONIC","时间由系统时钟记录"),INK2);
}
void set_view(unsigned v) {s.view=uint8_t(v);s.selected=0;touch=-1;if(v==2) {unsigned n=next_goal();journal_page=n<8?n/4:1;s.selected=uint8_t(n<8?n%4:3);}cue(pocket_music::Cue::Select);record("page");}
void act(unsigned item) {
    sync_clock();
    if(s.view==0) {
        if(item==0) {if(cooldown) {notice("LET THE FIREFLIES SETTLE... 0.65 SECONDS.","稍等流萤落定，间隔0.65秒。");return;}grove::collect(s);cooldown=650;notice("BASKET COLLECTED! +1 L FOR TENDING.","已收好篮中资源，照料额外+1L。");cue(pocket_music::Cue::Collect);record("collect");}
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
    if(s.view==0) {if(y>=174&&y<194) return x<96?0:x<188?1:2;if(y>=197&&x>=215) return 3;}
    else if(s.view==1&&y>=48&&y<184) return (y-48)/34;
    else if(s.view==2) {if(y>=51&&y<179) return (y-51)/32;if(y>=185&&y<212&&x>=240) return 5;}
    else if(s.view==3&&y>=188&&y<208) return x<140?0:1;
    return -1;
}
}
namespace game {
const char *title() {return tr("Lantern Grove","流萤林地");}
const char *app_id() {return "pxa-lantern-grove";}
void init() {grove::fresh(s);effects.reset(s.sound!=0);mono=grove::monotonic_ms();animation=autosave=cooldown=toast_ms=0;toast_en[0]=toast_zh[0]=0;touch=-1;record("new");}
void reset() {sync_clock();s.view=0;s.selected=0;touch=-1;notice("BACK IN THE GROVE. ALL GROWTH IS SAFE.","回到林地，所有成长都已保留。");cue(pocket_music::Cue::Confirm);record("reset");}
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
    if(phase==Phase::UP) {int at=touch;touch=-1;if(at>=0&&at==hit(x,y)) {if(at==6) set_view(0);else if(at==5) {journal_page^=1;redraw();}else act(unsigned(at));}}
}
void tick(uint32_t elapsed) {
    if(elapsed>100) elapsed=100;
    if(!grove::monotonic_ms()) grove::produce(s,elapsed); // honest foreground fallback
    sync_clock();animation+=elapsed;autosave+=elapsed;
    cooldown=elapsed>=cooldown?0:cooldown-elapsed;toast_ms=elapsed>=toast_ms?0:toast_ms-elapsed;
    if(autosave>=12000) {autosave=0;save();}
    redraw();
}
void draw() {if(s.view==0) home_draw();else if(s.view==1) build_draw();else if(s.view==2) journal_draw();else help_draw();}
size_t serialize(uint8_t *data,size_t size) {sync_clock();grove::stamp(s,grove::wall_seconds());return grove::encode(s,data,size);}
void load(const uint8_t *data,size_t size) {
    grove::State candidate;
    if(!grove::decode(candidate,data,size)) {log("lantern-grove rejected save");return;}
    grove::Away away=grove::offline(candidate,grove::wall_seconds());s=candidate;effects.set_enabled(s.sound!=0);mono=grove::monotonic_ms();touch=-1;
    if(away.valid&&away.seconds) {char en[128],zh[128];snprintf(en,sizeof(en),"WELCOME BACK! %uS OF LIGHT IN THE BASKET.",away.seconds);snprintf(zh,sizeof(zh),"欢迎回来！篮中积累了%u秒的微光。",away.seconds);notice(en,zh);}
    else if(!away.valid) notice("WELCOME HOME. LIVE LIGHT IS GROWING.","欢迎回家，微光正在慢慢汇聚。");
    record("loaded");
}
}
