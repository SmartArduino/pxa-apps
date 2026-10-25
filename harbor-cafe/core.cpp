// SPDX-License-Identifier: CC0-1.0
#include "core.hpp"
#include <string.h>
namespace cafe {
const char *const RECIPES[3]={"ESPRESSO","LATTE","BERRY SODA"};
const uint8_t INGREDIENTS[3][3]={{1,0,0},{1,1,0},{0,0,2}};
const uint16_t PRICES[3]={9,14,10}, UNIT_COST[3]={2,1,1}, PACK_COST[3]={8,4,4};
static uint32_t cap(uint32_t a,uint32_t b,uint32_t maximum) { return a+b>maximum?maximum:a+b; }
static void inc(uint16_t &v) { if(v<60000) ++v; }
unsigned target(const State &s) { return s.day<5?s.day+2:7; }
unsigned cost(unsigned r) { if(r>=3) return 0; unsigned n=0; for(unsigned i=0;i<3;++i) n+=INGREDIENTS[r][i]*UNIT_COST[i]; return n; }
unsigned upgrade_cost(const State &s,unsigned i) { if(i>=3) return 0; unsigned l=s.level[i]; return 20+22*l+6*l*l; }
unsigned patience_limit(const State &s) { return 35000+6000*s.level[1]; }
void fresh(State &s) { s=State{}; }
static void clean_day(State &s) {
 s.day_ms=0; s.spawn_ms=11400; s.cook_ms=0; s.cook_recipe=NONE; s.ready_recipe=NONE;
 s.revenue=0; s.cogs=0; s.served=0; s.failed=0; s.bonus=0; s.fee=0;
 s.summary=0; s.table=0; s.recipe=0; s.view=View::CAFE; s.shop=0;
 for(auto &c:s.customers) c=Customer{};
}
void restart(State &s) { clean_day(s); }
void next_day(State &s) { if(!s.summary) return; if(s.day<999) ++s.day; s.rescued=0; clean_day(s); }
Event discard(State &s) {
 if(s.cook_recipe==NONE&&s.ready_recipe==NONE) return Event::EMPTY;
 s.cook_recipe=NONE; s.ready_recipe=NONE; s.cook_ms=0; return Event::DISCARDED;
}
Event act(State &s) {
 if(s.summary) { next_day(s); return Event::NEXT_DAY; }
 Customer &c=s.customers[s.table];
 if(s.cook_recipe!=NONE) return Event::BUSY;
 if(s.ready_recipe!=NONE) {
  if(c.stage!=2||c.recipe!=s.ready_recipe) return Event::WRONG;
  unsigned tip=(c.patience>patience_limit(s)/2?1u:0u)+s.level[2];
  unsigned sale=PRICES[c.recipe]+tip, repayment=s.debt<sale?s.debt:sale;
  s.debt=uint16_t(s.debt-repayment); s.coins=cap(s.coins,sale-repayment,999999);
  s.revenue=cap(s.revenue,sale,10000); inc(s.served); inc(s.total_served);
  c=Customer{}; s.ready_recipe=NONE; return Event::SOLD;
 }
 if(c.stage==1) { c.stage=2; s.recipe=c.recipe; return Event::ACCEPTED; }
 if(c.stage!=2) return Event::EMPTY;
 for(unsigned i=0;i<3;++i) if(s.stock[i]<INGREDIENTS[s.recipe][i]) return Event::STOCK;
 for(unsigned i=0;i<3;++i) s.stock[i]=uint16_t(s.stock[i]-INGREDIENTS[s.recipe][i]);
 s.cogs=cap(s.cogs,cost(s.recipe),10000); s.cook_recipe=s.recipe;
 s.cook_ms=2400-500*s.level[0]; return Event::COOKING;
}
Event buy(State &s,unsigned i) {
 if(i>3||s.summary) return Event::EMPTY;
 if(i==3) {
  if(s.rescued||s.stock[0]>=2) return Event::MAXED;
  s.stock[0]=uint16_t(s.stock[0]+2); s.stock[1]=uint16_t(s.stock[1]+1>99?99:s.stock[1]+1);
  s.stock[2]=uint16_t(s.stock[2]+2>99?99:s.stock[2]+2); s.debt=uint16_t(s.debt+8>1000?1000:s.debt+8); s.rescued=1; return Event::CREDIT;
 }
 if(s.stock[i]>95) return Event::MAXED;
 if(s.coins<PACK_COST[i]) return Event::POOR;
 s.coins-=PACK_COST[i]; s.stock[i]=uint16_t(s.stock[i]+4); return Event::BOUGHT;
}
Event upgrade(State &s,unsigned i) {
 if(i>=3||s.summary) return Event::EMPTY;
 if(s.level[i]>=3) return Event::MAXED;
 unsigned price=upgrade_cost(s,i); if(s.coins<price) return Event::POOR;
 s.coins-=price; ++s.level[i]; return Event::UPGRADED;
}
Event step(State &s,uint32_t ms) {
 if(s.summary||!ms) return Event::NONE;
 if(ms>100) ms=100; // Work never scales with an untrusted elapsed value.
 Event e=Event::NONE;
 s.day_ms+=ms;
 if(s.cook_recipe!=NONE) {
  if(ms>=s.cook_ms) { s.ready_recipe=s.cook_recipe; s.cook_recipe=NONE; s.cook_ms=0; e=Event::READY; }
  else s.cook_ms-=ms;
 }
 for(auto &c:s.customers) if(c.stage) {
  if(ms>=c.patience) { c=Customer{}; inc(s.failed); inc(s.total_failed); e=Event::FAILED; }
  else c.patience-=ms;
 }
 s.spawn_ms+=ms;
 if(s.spawn_ms>=12000) {
  s.spawn_ms=0;
  for(unsigned i=0;i<3;++i) if(!s.customers[i].stage) {
   s.seed=s.seed*1664525u+1013904223u;
   const bool first=s.total_served==0&&s.total_failed==0&&s.day_ms<2000;
   s.customers[i]={1,uint8_t(first?0:(s.seed>>16)%3),uint8_t((s.seed>>24)%4),patience_limit(s)};
   if(first) s.table=uint8_t(i);
   e=Event::ARRIVED; break;
  }
 }
 if(s.day_ms>=DAY_MS) {
  s.day_ms=DAY_MS;
  for(auto &c:s.customers) { if(c.stage) { inc(s.failed); inc(s.total_failed); } c=Customer{}; }
  s.cook_recipe=NONE; s.ready_recipe=NONE; s.cook_ms=0;
  if(s.paid_day!=s.day) {
   s.fee=10; s.coins=s.coins>=10?s.coins-10:0;
   if(s.served>=target(s)) { s.bonus=8; if(s.stars<999) ++s.stars; s.coins=cap(s.coins,8,999999); }
   s.paid_day=s.day;
  }
  s.summary=1; s.view=View::CAFE; e=Event::CLOSED;
 }
 return e;
}
bool valid(const State &s) {
 if(s.coins>999999||s.day<1||s.day>999||s.paid_day>s.day||s.total_served>60000||s.total_failed>60000||s.stars>999||s.debt>1000) return false;
 for(unsigned i=0;i<3;++i) if(s.stock[i]>99||s.level[i]>3) return false;
 if(s.recipe>2||s.table>2||unsigned(s.view)>3||s.shop>3||s.sound>1||s.rescued>1||s.summary>1) return false;
 if(s.day_ms>DAY_MS||s.spawn_ms>12000||s.cook_ms>2400||s.cook_recipe>NONE||s.ready_recipe>NONE||s.served>1000||s.failed>1000||s.revenue>10000||s.cogs>10000) return false;
 if((s.cook_recipe==NONE)!=(s.cook_ms==0)|| (s.cook_recipe!=NONE&&s.ready_recipe!=NONE)) return false;
 if((s.summary&&(s.day_ms!=DAY_MS||s.paid_day!=s.day))||(!s.summary&&s.day_ms>=DAY_MS)) return false;
 if((s.bonus!=0&&s.bonus!=8)||(s.fee!=0&&s.fee!=10)) return false;
 for(const auto &c:s.customers) {
  if(c.stage>2||c.recipe>2||c.skin>3||c.patience>60000) return false;
  if((c.stage==0&&(c.patience||c.recipe||c.skin))||(c.stage&&c.patience==0)) return false;
  if(s.summary&&c.stage) return false;
 }
 return !s.summary||(s.cook_recipe==NONE&&s.ready_recipe==NONE);
}
static void put16(uint8_t *p,uint16_t v) { p[0]=uint8_t(v);p[1]=uint8_t(v>>8); }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;++i) p[i]=uint8_t(v>>(8*i)); }
static uint16_t get16(const uint8_t *p) { return uint16_t(p[0]|uint16_t(p[1])<<8); }
static uint32_t get32(const uint8_t *p) { uint32_t v=0;for(unsigned i=0;i<4;++i) v|=uint32_t(p[i])<<(8*i);return v; }
static uint32_t hash(const uint8_t *p) { uint32_t h=2166136261u;for(unsigned i=0;i<156;++i) h=(h^p[i])*16777619u;return h; }
size_t encode(const State &s,uint8_t *p,size_t n) {
 if(!p||n<SAVE_BYTES||!valid(s)) return 0;
 memset(p,0,SAVE_BYTES); memcpy(p,"HCFE",4);p[4]=1;put16(p+6,SAVE_BYTES);
 put32(p+8,s.coins);const uint16_t words[]={s.day,s.paid_day,s.total_served,s.total_failed,s.stars,s.debt,s.stock[0],s.stock[1],s.stock[2]};
 for(unsigned i=0;i<9;++i) put16(p+12+i*2,words[i]);
 for(unsigned i=0;i<3;++i) p[30+i]=s.level[i];
 p[33]=s.recipe;p[34]=s.table;p[35]=uint8_t(s.view);p[36]=s.shop;p[37]=s.sound;p[38]=s.rescued;p[39]=s.summary;
 put32(p+40,s.day_ms);put32(p+44,s.spawn_ms);put32(p+48,s.seed);put32(p+52,s.cook_ms);
 p[56]=s.cook_recipe;p[57]=s.ready_recipe;put16(p+58,s.served);put16(p+60,s.failed);put32(p+62,s.revenue);put32(p+66,s.cogs);put16(p+70,s.bonus);put16(p+72,s.fee);
 for(unsigned i=0;i<3;++i) { unsigned a=74+8*i;const auto &c=s.customers[i];p[a]=c.stage;p[a+1]=c.recipe;p[a+2]=c.skin;put32(p+a+4,c.patience); }
 put32(p+156,hash(p));return SAVE_BYTES;
}
bool decode(State &s,const uint8_t *p,size_t n) {
 if(!p||n!=SAVE_BYTES||memcmp(p,"HCFE",4)||p[4]!=1||p[5]||get16(p+6)!=SAVE_BYTES||get32(p+156)!=hash(p)) return false;
 for(unsigned i=98;i<156;++i) if(p[i]) return false;
 State t;
 t.coins=get32(p+8);t.day=get16(p+12);t.paid_day=get16(p+14);t.total_served=get16(p+16);t.total_failed=get16(p+18);t.stars=get16(p+20);t.debt=get16(p+22);
 for(unsigned i=0;i<3;++i) { t.stock[i]=get16(p+24+2*i);t.level[i]=p[30+i]; }
 t.recipe=p[33];t.table=p[34];t.view=View(p[35]);t.shop=p[36];t.sound=p[37];t.rescued=p[38];t.summary=p[39];
 t.day_ms=get32(p+40);t.spawn_ms=get32(p+44);t.seed=get32(p+48);t.cook_ms=get32(p+52);t.cook_recipe=p[56];t.ready_recipe=p[57];t.served=get16(p+58);t.failed=get16(p+60);t.revenue=get32(p+62);t.cogs=get32(p+66);t.bonus=get16(p+70);t.fee=get16(p+72);
 for(unsigned i=0;i<3;++i) { unsigned a=74+8*i;if(p[a+3]) return false;t.customers[i]={p[a],p[a+1],p[a+2],get32(p+a+4)}; }
 if(!valid(t)) return false;
 s=t;return true;
}
}
