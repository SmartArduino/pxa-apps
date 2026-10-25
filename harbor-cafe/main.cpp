// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-play/music.hpp"
#include "core.hpp"
#include <stdio.h>
namespace {
using namespace pocket;
using cafe::Event;
cafe::State s;
pocket_music::Effects effects;
void cue(pocket_music::Cue value) {effects.set_enabled(s.sound!=0);effects.cue(value);}
const char *recipe_name(unsigned r) {const char *zh[]={"浓缩","拿铁","莓果汽水"};return r<3?tr(cafe::RECIPES[r],zh[r]):"";}
constexpr uint16_t NAVY=rgb(30,53,67), CREAM=rgb(255,241,205), WOOD=rgb(154,94,68), LIGHT=rgb(216,160,107), MINT=rgb(157,198,166), SEA=rgb(63,148,169), SKY=rgb(179,221,220), PEACH=rgb(246,173,140), GOLD=rgb(244,196,91), RED=rgb(202,84,75), WHITE=rgb(255,251,232);
uint32_t anim=0,paint_ms=0,save_ms=0,toast_ms=0,walk_ms=0;
int pressed=-1,waiter_x=80;
const char *toast_en="",*toast_zh="";
void notice(const char *en,const char *zh) {toast_en=en;toast_zh=zh;}
const char *toast_text() {return tr(toast_en,toast_zh);}
void centered(int x,int y,int w,const char *v,uint16_t color=NAVY) { text(x+(w-text_width(v))/2,y,v,color); }
void frame(int x,int y,int w,int h,uint16_t color) { rect(x+2,y,w-4,h,NAVY);rect(x,y+2,w,h-4,NAVY);rect(x+2,y+2,w-4,h-4,color); }
void button(int x,int y,int w,int h,const char *v,bool on=false) { frame(x,y,w,h,on?GOLD:CREAM);centered(x,y+(h-10)/2,w,v); }
void cup(int x,int y,unsigned r,int z=1) {
 uint16_t c=r==0?WOOD:r==1?CREAM:PEACH;
 rect(x,y,7*z,8*z,WHITE);rect(x+z,y+2*z,5*z,4*z,c);outline(x+6*z,y+z,4*z,4*z,WHITE);rect(x-z,y+8*z,10*z,z,NAVY);
 if(r==2) { rect(x+2*z,y-3*z,z,5*z,MINT);rect(x+4*z,y+3*z,2*z,2*z,RED); }
}
void flower(int x,int y) { rect(x,y,8,7,PEACH);rect(x+2,y-9,2,10,MINT);rect(x-1,y-11,8,4,RED);rect(x+1,y-13,4,8,GOLD); }
void person(int x,int y,unsigned who,bool guest) {
 const uint16_t skins[]={rgb(246,194,148),rgb(185,121,85),rgb(231,165,114),rgb(118,77,60)};
 const uint16_t clothes[]={SEA,PEACH,MINT,GOLD};
 rect(x+2,y,9,8,skins[who%4]);rect(x+1,y-2,11,4,who%2?WOOD:NAVY);rect(x+9,y+4,1,1,NAVY);
 rect(x,y+8,14,10,guest?clothes[who%4]:NAVY);rect(x+3,y+9,8,9,guest?clothes[who%4]:CREAM);
 rect(x+1,y+18,4,5,NAVY);rect(x+9,y+18,4,5,NAVY);rect(x-2,y+9,3,7,skins[who%4]);rect(x+13,y+9,3,7,skins[who%4]);
 if(!guest) { rect(x+1,y-4,12,3,WHITE);rect(x+3,y-7,8,4,WHITE); }
}
const char *action_name() {
 if(s.summary) return tr("OPEN NEXT DAY","次日开店");
 if(s.cook_recipe!=cafe::NONE) return tr("BREWING...","制作中...");
 if(s.ready_recipe!=cafe::NONE) return tr("SERVE / A","上菜 / A");
 unsigned stage=s.customers[s.table].stage;
 return stage==1?tr("ACCEPT / A","接单 / A"):stage==2?tr("MAKE / A","制作 / A"):tr("WAIT FOR GUEST","等待客人");
}
void record(Event e) {
 if(e==Event::NONE) return;
 switch(e) {
 case Event::ARRIVED:notice("A GUEST HAS A NEW ORDER","客人来了，点桌位查看订单。");break;
 case Event::ACCEPTED:notice("ORDER TAKEN. MAKE AT THE COUNTER","已接单，在吧台制作饮品。");cue(pocket_music::Cue::Order);break;
 case Event::COOKING:notice("INGREDIENTS USED. WATCH THE COFFEE BAR","原料已投入，留意吧台进度。");cue(pocket_music::Cue::Cook);break;
 case Event::READY:notice("READY! SELECT MATCHING TABLE AND SERVE","饮品做好了，选对应桌位上菜。");cue(pocket_music::Cue::Ready);break;
 case Event::SOLD:s.debt?notice("SERVED! SALES ARE REPAYING YOUR CREDIT","已上菜！收入先偿还赊账。"):notice("SERVED! COINS RECEIVED. THANK YOU!","已上菜，金币到账，谢谢惠顾！");walk_ms=1800;cue(pocket_music::Cue::Serve);break;
 case Event::FAILED:notice("GUEST LEFT. NO FINE. TAKE THE NEXT ORDER","客人离开了，不罚款，继续接单。");cue(pocket_music::Cue::Error);break;
 case Event::CLOSED:notice("CLOSED. REVIEW TODAY AND OPEN TOMORROW","打烊了，查看日结后迎接明天。");cue(pocket_music::Cue::Day);break;
 case Event::NEXT_DAY:notice("A NEW DAY! STOCK UP AND WELCOME GUESTS","新的一天！备好原料，欢迎客人。");cue(pocket_music::Cue::Day);break;
 case Event::BOUGHT:notice("FOUR INGREDIENTS DELIVERED TO THE SHELF","四份原料已经送到货架。");cue(pocket_music::Cue::Restock);break;
 case Event::UPGRADED:notice("UPGRADE INSTALLED! SEE YOUR CAFE GROW","升级完成！咖啡馆又添新风景。");cue(pocket_music::Cue::Upgrade);break;
 case Event::CREDIT:notice("RECOVERY CRATE. SALES REPAY CREDIT","应急补给已到，后续销售偿还赊账。");cue(pocket_music::Cue::Restock);break;
 case Event::DISCARDED:notice("TRAY CLEARED. INGREDIENT COST IS KEPT","托盘已清空，原料成本仍保留。");cue(pocket_music::Cue::Clear);break;
 case Event::BUSY:notice("LET THE MACHINE FINISH BEFORE SERVING","请等机器完成，再为客人上菜。");cue(pocket_music::Cue::Error);break;
 case Event::EMPTY:notice("CHOOSE A GUEST WITH AN ORDER BUBBLE","请选择带订单气泡的客人。");cue(pocket_music::Cue::Error);break;
 case Event::STOCK:notice("OUT OF STOCK. SUPPLY / X TO RESTOCK","原料不足，点备料或按X进货。");cue(pocket_music::Cue::Error);break;
 case Event::POOR:notice("NOT ENOUGH CASH. SERVE OR USE RECOVERY","金币不足，先营业或领取应急补给。");cue(pocket_music::Cue::Error);break;
 case Event::WRONG:notice("WRONG TABLE! MATCH THE BUBBLE OR B:BIN","桌位不符！选对应桌，或按B清盘。");cue(pocket_music::Cue::Error);break;
 case Event::MAXED:notice("MAXIMUM REACHED OR CRATE NOT NEEDED","已达上限，或暂时不需要补给。");cue(pocket_music::Cue::Error);break;
 default:break;
 }
 toast_ms=3000;
 char line[224];snprintf(line,sizeof(line),"harbor event=%u day=%u ms=%u table=%u recipe=%u coins=%u stock=%u/%u/%u served=%u failed=%u debt=%u levels=%u/%u/%u",unsigned(e),s.day,unsigned(s.day_ms),s.table,s.recipe,unsigned(s.coins),s.stock[0],s.stock[1],s.stock[2],s.served,s.failed,s.debt,s.level[0],s.level[1],s.level[2]);log(line);
 if(e<=Event::DISCARDED) save();
 redraw();
}
void change_view(cafe::View v) { s.view=v;s.shop=0;pressed=-1;cue(pocket_music::Cue::Select);save();redraw(); }
void scene() {
 rect(0,35,288,111,CREAM);rect(0,77,288,19,MINT);
 // Window: coastline, lighthouse, sailboat, waves and birds are original pixel art.
 frame(130,39,150,46,WOOD);rect(134,43,142,35,s.day_ms>120000?PEACH:SKY);
 rect(134,61,142,18,SEA);rect(134,76,142,5,MINT);
 for(int i=0;i<8;++i) { int x=136+i*18+int(anim/180%6);rect(x,67+i%2*5,9,1,CREAM); }
 int bx=163+int(anim/600%60);rect(bx,63,21,3,NAVY);rect(bx+3,66,15,2,WHITE);line(bx+10,48,bx+10,63,WOOD);
 for(int i=0;i<12;++i) rect(bx+11,49+i,1+i/2,1,WHITE);
 rect(250,48,5,16,CREAM);rect(248,47,9,3,RED);rect(251,44,3,3,GOLD);rect(252,53,2,3,NAVY);
 int bird=139+int(anim/160%85);line(bird,50,bird+3,52,NAVY);line(bird+3,52,bird+6,50,NAVY);
 rect(204,41,3,42,WOOD);rect(132,80,146,4,LIGHT);
 if(s.level[1]) { rect(130,36,150,5,SEA);for(int i=0;i<10;++i) rect(133+i*15,36,7,7,WHITE); }
 rect(0,96,288,50,LIGHT);
 for(int y=98;y<146;y+=12) { line(0,y,287,y,WOOD);for(int x=(y%24?20:44);x<288;x+=48) rect(x,y,1,11,WOOD); }
 rect(9,49,104,3,WOOD);rect(15,39,11,10,WHITE);rect(17,42,7,5,WOOD);rect(34,39,10,10,MINT);rect(51,41,8,8,PEACH);rect(68,40,10,9,GOLD);
 text(60,61,tr("COFFEE CLUB","港湾小馆"),NAVY);
 // An espresso machine gains a taller copper tower, gauge and polished group heads.
 frame(17,80,38,24,s.level[0]?SEA:WOOD);rect(22,76,28,5,s.level[0]>1?GOLD:LIGHT);rect(23,85,5,5,GOLD);rect(40,85,5,5,PEACH);
 if(s.level[0]>1) { rect(18,66,8,13,WOOD);rect(19,65,6,2,GOLD);outline(29,80,10,7,WHITE); }
 if(s.level[0]>2) { rect(47,72,5,12,WHITE);rect(16,97,41,2,GOLD); }
 if(s.level[2]) flower(112,88);
 if(s.level[2]>1) { flower(3,86);rect(86,40,30,12,SEA);text(89,41,tr("CAFE","小馆"),CREAM); }
 if(s.level[2]>2) for(int i=0;i<5;++i) { line(5+i*24,54,26+i*24,54,WOOD);rect(15+i*24,54,3,3,GOLD); }
 person(waiter_x,86,0,false);
 rect(9,107,115,25,WOOD);rect(7,104,119,5,CREAM);rect(11,110,111,3,LIGHT);
 for(int x=14;x<120;x+=18) rect(x,115,1,13,LIGHT);
 rect(52,106,25,2,NAVY);
 if(s.cook_recipe!=cafe::NONE||s.ready_recipe!=cafe::NONE) {
  unsigned r=s.ready_recipe!=cafe::NONE?s.ready_recipe:s.cook_recipe;cup(61,95,r);
  if(s.cook_recipe!=cafe::NONE) { unsigned duration=2400-500*s.level[0];rect(16,136,103,4,NAVY);rect(17,137,int(101*(duration-s.cook_ms)/duration),2,GOLD);int yy=int(anim/140%3);line(63,92-yy,65,88-yy,WHITE); }
  else { text(32,135,tr("READY / BIN","完成 / 清盘"),NAVY); }
 }
 for(unsigned i=0;i<3;++i) {
  int x=145+int(i)*48;const auto &c=s.customers[i];
  if(s.level[1]>1) { rect(x-7,112,9,16,SEA);rect(x+23,112,9,16,SEA); }
  else { rect(x-5,116,5,12,WOOD);rect(x+24,116,5,12,WOOD); }
  if(c.stage) person(x+5,108,c.skin,true);
  rect(x-3,126,34,4,WOOD);rect(x,123,28,5,CREAM);rect(x+1,130,3,10,NAVY);rect(x+23,130,3,10,NAVY);
  if(s.level[2]>1) { rect(x+12,117,4,6,MINT);rect(x+10,116,8,3,PEACH); }
  if(i==s.table) outline(x-10,86,44,59,GOLD);
  if(c.stage) {
   frame(x-9,87,43,19,c.stage==2?MINT:WHITE);centered(x-8,90,41,c.recipe==0?tr("ESP","浓缩"):c.recipe==1?tr("LATTE","拿铁"):tr("SODA","汽水"));
   rect(x-5,101,34,2,WOOD);unsigned p=c.patience*34/cafe::patience_limit(s);rect(x-5,101,int(p>34?34:p),2,c.patience<10000?RED:SEA);
  } else { text(x+8,113,"...",NAVY); }
 }
 if(s.level[1]>2) { rect(134,141,145,4,SEA);rect(139,141,7,4,WHITE);rect(190,141,7,4,WHITE);rect(242,141,7,4,WHITE); }
}
void main_view() {
 scene();char b[64];
 if(s.summary) {
  frame(24,45,240,119,CREAM);centered(24,53,240,tr("THE HARBOR CLOSES FOR THE NIGHT","港湾夜深，今日打烊"));
  snprintf(b,sizeof(b),tr("SERVED %u / GOAL %u   LOST %u","服务%u / 目标%u 流失%u"),s.served,cafe::target(s),s.failed);text(34,70,b,NAVY);
  snprintf(b,sizeof(b),tr("SALES G%u  INGREDIENTS G%u","销售%uG 原料%uG"),unsigned(s.revenue),unsigned(s.cogs));text(34,83,b,NAVY);
  snprintf(b,sizeof(b),tr("RENT G%u  GOAL BONUS G%u","租金%uG 奖励%uG"),s.fee,s.bonus);text(34,96,b,NAVY);
  int net=int(s.revenue)-int(s.cogs)-int(s.fee);snprintf(b,sizeof(b),tr("NET %+d   CASH G%u","净利%+d 现金%uG"),net,unsigned(s.coins));text(34,109,b,NAVY);
  snprintf(b,sizeof(b),tr("CREDIT LEFT G%u / STARS %u","赊账%uG 星%u"),s.debt,s.stars);text(34,122,b,NAVY);
  centered(24,141,240,tr("GROW: THREE LINES, THREE LEVELS","三条路线，各有三级成长"));button(58,177,172,28,tr("OPEN NEXT DAY / A","次日开店 / A"),true);return;
 }
 rect(0,147,288,22,NAVY);
 snprintf(b,sizeof(b),tr("%s COSTG%u SELLG%u  %u/%u/%u","%s 成本%uG 售%uG %u/%u/%u"),recipe_name(s.recipe),cafe::cost(s.recipe),cafe::PRICES[s.recipe],s.stock[0],s.stock[1],s.stock[2]);text(8,149,b,CREAM);
 const char *hint=toast_ms?toast_text():s.cook_recipe!=cafe::NONE?tr("BREWING... PATIENCE CONTINUES TO COUNT","制作中，客人的耐心仍在减少。"):s.ready_recipe!=cafe::NONE?tr("3 MATCH TABLE THEN SERVE. TAP TRAY: BIN","3 选对应桌位上菜；点托盘可清空。"):s.customers[s.table].stage==2?tr("2 MAKE THE RECIPE. LEFT/RIGHT: CHANGE","2 制作对应配方，左右更换饮品。"):tr("1 SELECT ORDER BUBBLE. ACCEPT TO START","1 点订单气泡，接单后开始制作。");
 text(8,159,hint,WHITE);
 button(12,171,62,17,tr("< TABLE","< 桌位"));button(82,171,122,17,action_name(),true);button(212,171,64,17,tr("TABLE >","桌位 >"));
 button(12,192,52,17,tr("< DRINK","< 饮品"));button(70,192,52,17,tr("DRINK >","饮品 >"));button(128,192,72,17,tr("SUPPLY X","备料 X"));button(206,192,72,17,tr("GROW Y","升级 Y"));
}
void supplies() {
 char b[64];centered(0,40,288,tr("PANTRY / FOUR-UNIT DELIVERY","备料间 / 每次进货四份"));
 const char *names[]={tr("BEANS","咖啡豆"),tr("MILK","牛奶"),tr("BERRIES","莓果")};
 for(unsigned i=0;i<3;++i) { int y=57+int(i)*29;snprintf(b,sizeof(b),tr("%s +4  G%u  HAVE %u","%s +4 花%uG 库存%u"),names[i],cafe::PACK_COST[i],s.stock[i]);button(24,y,240,25,b,s.shop==i); }
 button(24,144,240,25,s.rescued?tr("RECOVERY USED TODAY","今日已领取应急补给"):tr("RECOVERY CRATE: UP TO G8 CREDIT","应急补给：最多赊账8G"),s.shop==3);
 const char *status=toast_text();
 text(24,173,toast_ms?status:tr("CRATE ONLY IF BEANS <2. ONCE A DAY.","咖啡豆不足2份可领，每天一次。"),NAVY);text(24,184,tr("NEXT SALES REPAY CREDIT. NO CASH FINE.","销售先还赊账，不收现金罚款。"),NAVY);
 button(24,196,108,14,tr("BACK / B","返回 / B"));button(148,196,116,14,tr("GUIDE / L","指南 / L"));
}
void upgrades() {
 char b[64];centered(0,40,288,tr("BUILD YOUR SEASIDE COFFEE CLUB","布置海边咖啡小馆"));
 const char *names[]={tr("MACHINE","咖啡机"),tr("TERRACE","露台"),tr("FLOWERS","花艺")};const char *desc[]={tr("FASTER BREW: -0.5S PER LEVEL","每级制作时间减少0.5秒"),tr("MORE PATIENCE: +6S PER LEVEL","每级增加6秒耐心"),tr("FLOWERS + LIGHTS: +G1 TIP / LV","每级增加1G小费，增添花与灯")};
 for(unsigned i=0;i<3;++i) { int y=57+int(i)*42;snprintf(b,sizeof(b),tr("%s LV%u  %sG%u","%s %u级 %s%uG"),names[i],s.level[i],s.level[i]==3?tr("MAX ","满级 "):tr("BUY ","购买 "),s.level[i]==3?0:cafe::upgrade_cost(s,i));button(24,y,240,23,b,s.shop==i);text(24,y+27,desc[i],NAVY); }
 const char *status=toast_text();
 text(24,185,toast_ms?status:tr("LONG GOAL: THREE DAYS + ALL LV2","长远目标：营业三天，全路线二级"),NAVY);button(24,196,108,14,tr("BACK / B","返回 / B"));button(148,196,116,14,tr("GUIDE / L","指南 / L"));
}
void help() {
 centered(0,40,288,tr("WELCOME TO HARBOR CAFE","欢迎来到港湾咖啡馆"));
 const char *lines[]={tr("1 SELECT A TABLE, ACCEPT ITS ORDER.","1 选桌位，接下客人的订单。"),tr("2 MAKE: STOCK IS USED AT THE BAR.","2 点制作，吧台立即消耗原料。"),tr("3 WAIT FOR THE CUP, THEN SERVE.","3 等待饮品完成，再上菜。"),tr("A ACTION / UP DOWN SELECT TABLE.","A 操作，上下选择桌位。"),tr("LEFT RIGHT DRINK. B BIN OR BACK.","左右换饮品，B 清盘或返回。"),tr("X PANTRY / Y UPGRADES / L GUIDE.","X 备料，Y 升级，L 指南。"),tr("150S DAY. GOAL 3+ ORDERS = G8 STAR.","每日150秒，达标得8G和星。"),tr("RENT G10. MISSED GUESTS HAVE NO FINE.","租金10G，客人离开不罚款。"),tr("RESET REOPENS TODAY, KEEPS GROWTH.","重开当天，保留金币与成长。"),tr("AUTOSAVE 12S + EVERY TRANSACTION.","每12秒及每次交易自动保存。")};
 for(unsigned i=0;i<10;++i) text(24,56+int(i)*12,lines[i],NAVY);
 button(24,183,108,27,tr("BACK / B","返回 / B"));button(148,183,116,27,s.sound?tr("SOUND ON","音效 开"):tr("SOUND OFF","音效 关"),s.sound);
}
int hit(int x,int y) {
 if(s.summary) return x>=58&&x<230&&y>=177&&y<205?10:-1;
 if(s.view==cafe::View::CAFE) {
  if(x>=32&&x<100&&y>=91&&y<145&&(s.cook_recipe!=cafe::NONE||s.ready_recipe!=cafe::NONE)) return 17;
  for(int i=0;i<3;++i) if(x>=135+i*48&&x<179+i*48&&y>=86&&y<146) return i;
  if(y>=171&&y<188) { if(x>=12&&x<74) return 11;if(x>=82&&x<204) return 10;if(x>=212&&x<276) return 12; }
  if(y>=192&&y<209) { if(x>=12&&x<64) return 13;if(x>=70&&x<122) return 14;if(x>=128&&x<200) return 15;if(x>=206&&x<278) return 16; }
 } else {
  if(s.view==cafe::View::SUPPLY) for(int i=0;i<4;++i) if(x>=24&&x<264&&y>=57+i*29&&y<82+i*29) return 20+i;
  if(s.view==cafe::View::UPGRADES) for(int i=0;i<3;++i) if(x>=24&&x<264&&y>=57+i*42&&y<80+i*42) return 20+i;
  if(s.view==cafe::View::HELP&&y>=183&&y<210) { if(x>=24&&x<132) return 30;if(x>=148&&x<264) return 32; }
  if(s.view!=cafe::View::HELP&&y>=196&&y<210) { if(x>=24&&x<132) return 30;if(x>=148&&x<264) return 31; }
 }
 return -1;
}
void click(int id) {
 if(id<0) return;
 if(id<3) { s.table=uint8_t(id);redraw();return; }
 switch(id) {
 case 10:record(cafe::act(s));break;
 case 11:s.table=uint8_t((s.table+2)%3);break;
 case 12:s.table=uint8_t((s.table+1)%3);break;
 case 13:s.recipe=uint8_t((s.recipe+2)%3);break;
 case 14:s.recipe=uint8_t((s.recipe+1)%3);break;
 case 15:change_view(cafe::View::SUPPLY);break;
 case 16:change_view(cafe::View::UPGRADES);break;
 case 17:record(cafe::discard(s));break;
 case 20:case 21:case 22:case 23:s.shop=uint8_t(id-20);record(s.view==cafe::View::SUPPLY?cafe::buy(s,s.shop):cafe::upgrade(s,s.shop));break;
 case 30:change_view(cafe::View::CAFE);break;
 case 31:change_view(cafe::View::HELP);break;
 case 32:s.sound^=1;effects.set_enabled(s.sound!=0);cue(pocket_music::Cue::Confirm);save();break;
 default:break;
 }
 redraw();
}
}
namespace game {
const char *title() { return tr("HARBOR CAFE","港湾咖啡馆"); }
const char *app_id() { return "pxa-harbor-cafe"; }
void init() { cafe::fresh(s);effects.reset(s.sound!=0);toast_en=toast_zh="";anim=paint_ms=save_ms=toast_ms=walk_ms=0;pressed=-1;waiter_x=80; }
void reset() { cafe::restart(s);pressed=-1;record(Event::NEXT_DAY); }
void key(Key k) {
 if(k==Key::L) { change_view(s.view==cafe::View::HELP?cafe::View::CAFE:cafe::View::HELP);return; }
 if(k==Key::B) { if(s.view!=cafe::View::CAFE)change_view(cafe::View::CAFE);else record(cafe::discard(s));return; }
 if(s.summary) { if(k==Key::A)click(10);return; }
 if(k==Key::X) { change_view(cafe::View::SUPPLY);return; }
 if(k==Key::Y) { change_view(cafe::View::UPGRADES);return; }
 if(s.view==cafe::View::CAFE) {
  if(k==Key::UP)click(11);else if(k==Key::DOWN)click(12);else if(k==Key::LEFT)click(13);else if(k==Key::RIGHT)click(14);else if(k==Key::A)click(10);
 } else if(s.view==cafe::View::HELP) { if(k==Key::A)click(32); }
 else { unsigned n=s.view==cafe::View::SUPPLY?4:3;if(k==Key::UP||k==Key::LEFT)s.shop=uint8_t((s.shop+n-1)%n);else if(k==Key::DOWN||k==Key::RIGHT)s.shop=uint8_t((s.shop+1)%n);else if(k==Key::A)click(20+s.shop);redraw(); }
}
void pointer(Phase p,int x,int y) { if(p==Phase::CANCEL){pressed=-1;return;}if(p==Phase::DOWN)pressed=hit(x,y);else if(p==Phase::UP){int v=pressed;pressed=-1;if(v==hit(x,y))click(v);} }
void tick(uint32_t ms) {
 if(ms>100) ms=100;
 anim+=ms;paint_ms+=ms;save_ms+=ms;toast_ms=ms>=toast_ms?0:toast_ms-ms;walk_ms=ms>=walk_ms?0:walk_ms-ms;
 int target=walk_ms?145+48*s.table:80;if(waiter_x<target)waiter_x+=2;else if(waiter_x>target)waiter_x-=2;
 record(cafe::step(s,ms));
 if(save_ms>=12000){save_ms=0;save();}
 if(paint_ms>=100){paint_ms=0;redraw();}
}
void draw() {
 clear(CREAM);rect(0,0,288,20,NAVY);text(8,7,tr("HARBOR","港湾"),CREAM);
 char b[40];snprintf(b,sizeof(b),tr("DAY%u","第%u天"),s.day);text(8,23,b,NAVY);snprintf(b,sizeof(b),"G%u",unsigned(s.coins));text(66,23,b,NAVY);snprintf(b,sizeof(b),"%03uS",unsigned((cafe::DAY_MS-s.day_ms+999)/1000));text(144,23,b,NAVY);snprintf(b,sizeof(b),tr("STAR%u","星%u"),s.stars);text(218,23,b,NAVY);
 rect(8,32,272,2,WOOD);rect(8,32,int(272*s.day_ms/cafe::DAY_MS),2,SEA);
 if(s.summary||s.view==cafe::View::CAFE)main_view();else if(s.view==cafe::View::SUPPLY)supplies();else if(s.view==cafe::View::UPGRADES)upgrades();else help();
 text(12,213,tr("HARBOR CAFE / ORIGINAL PIXEL COFFEE CLUB","港湾咖啡馆 / 原创像素小馆"),NAVY);
}
size_t serialize(uint8_t *p,size_t n) { return cafe::encode(s,p,n); }
void load(const uint8_t *p,size_t n) { if(cafe::decode(s,p,n)){effects.set_enabled(s.sound!=0);pressed=-1;char line[120];snprintf(line,sizeof(line),"harbor save-restored day=%u coins=%u served=%u levels=%u/%u/%u",s.day,unsigned(s.coins),s.total_served,s.level[0],s.level[1],s.level[2]);log(line);redraw();}else log("harbor save-rejected; fresh cafe retained"); }
}
